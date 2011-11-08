/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"

#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_LIBATTR
#	include <attr/libattr.h>
#endif

int ai_mv(const char *source, const char *dest) {
	if (!rename(source, dest))
		return 0;

	/* cross-device? try manually. */
	if (errno == EXDEV) {
		int ret = ai_cp_a(source, dest);
		if (!ret)
			unlink(source);
		return ret;
	}

	return errno;
}

int ai_cp_l(const char *source, const char *dest) {
	/* link() will not overwrite */
	unlink(dest);

	if (!link(source, dest))
		return 0;

	/* cross-device or not supported? try manually. */
	if (errno == EXDEV || errno == EACCES)
		return ai_cp_a(source, dest);

	return errno;
}

/**
 * ai_cp_symlink
 * @source: current file path
 * @dest: new complete file path
 * @symlen: symlink length (obligatory)
 *
 * Create a copy of symlink @source at @dest. The @symlen should state the exact
 * length of the symlink (as reported by lstat()) as it is used for buffer
 * allocation.
 *
 * If @symlen is too small, the function returns EINVAL.
 *
 * Returns: 0 on success, errno on failure
 */
static int ai_cp_symlink(const char *source, const char *dest, ssize_t symlen) {
	static char *buf = NULL;
	static ssize_t bufsize;

	/* ensure buffer is at least symlen+1 long */
	if (!buf || bufsize <= symlen) {
		bufsize = symlen + 1;
		buf = realloc(buf, bufsize);
		if (!buf)
			return errno;
	}

	/* ensure content length didn't change */
	if (readlink(source, buf, bufsize) != symlen)
		return EINVAL; /* XXX? */

	/* null terminate */
	buf[symlen + 1] = 0;

	unlink(dest);
	if (symlink(source, dest))
		return errno;
	return 0;
}

#ifndef AI_BUFSIZE
#	define AI_BUFSIZE 65536
#endif

/**
 * ai_splice
 * @fd_in: input fd
 * @fd_out: output fd
 *
 * Copy a block of data from @fd_in to @fd_out.
 *
 * Returns: positive number on success, 0 on EOF, -1 on failure
 *	(and errno is set then)
 */
static int ai_splice(int fd_in, int fd_out) {
	static char buf[AI_BUFSIZE];
	char *bufp = buf;
	ssize_t ret, wr = 0;

	ret = read(fd_in, buf, sizeof(buf));
	if (ret == -1) {
		if (errno == EINTR)
			return 1;
		else
			return -1;
	}

	while (ret > 0) {
		wr = write(fd_out, bufp, ret);
		if (wr == -1) {
			if (errno != EINTR)
				return -1;
		} else {
			ret -= wr;
			bufp += wr;
		}
	}

	return wr;
}

/**
 * ai_cp_reg
 * @source: current file path
 * @dest: new complete file path
 * @expsize: expected file length (for preallocation)
 *
 * Copies the contents of @source to a new file at @dest (@dest is unlinked
 * first).
 *
 * The destination file will be preallocated to size @expsize if possible.
 * However, this is no hard limit and the actual file length may be larger.
 * If it shorter, the file may be padded.
 *
 * Returns: 0 on success, errno on failure
 */
static int ai_cp_reg(const char *source, const char *dest, off_t expsize) {
	int fd_in, fd_out;
	int ret = 0, splret;

	fd_in = open(source, O_RDONLY);
	if (fd_in == -1)
		return errno;

	/* ensure to remove destination file before proceeding;
	 * otherwise, we could rewrite hardlinked file */
	if (unlink(dest) && errno != ENOENT)
		return errno;

	/* don't care about perms, will have to chmod anyway */
	fd_out = creat(dest, 0666);
	if (fd_out == -1) {
		const int tmp = errno;
		close(fd_in);
		return tmp;
	}

#ifdef HAVE_POSIX_FALLOCATE
	ret = posix_fallocate(fd_out, 0, expsize);
#endif

#ifdef HAVE_POSIX_FADVISE
	posix_fadvise(fd_in, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
	posix_fadvise(fd_out, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
	if (!ret) {
		do {
			splret = ai_splice(fd_in, fd_out);

			if (splret == -1)
				ret = errno;
		} while (splret > 0);
	}

	if (close(fd_out) && !ret)
		ret = errno;
	close(fd_in);

	return ret;
}

/**
 * ai_cp_stat
 * @dest: destination file
 * @st: struct with lstat() results
 *
 * Set ownership, permissions and timestamps from @st to destination file @dest.
 *
 * Returns: 0 on success, errno on failure
 */
static int ai_cp_stat(const char *dest, struct stat st) {
	int ret;

	if (lchown(dest, st.st_uid, st.st_gid))
		return errno;

	/* there's no point in copying directory mtime,
	 * it will be modified when copying files anyway */
	if (!S_ISDIR(st.st_mode)) {
#ifdef HAVE_UTIMENSAT
		struct timespec ts[2];

		ts[0] = st.st_atim;
		ts[1] = st.st_mtim;
		if (utimensat(AT_FDCWD, dest, ts, AT_SYMLINK_NOFOLLOW))
			return errno;
#else
		/* utime() can't handle touching symlinks */
		if (!S_ISLNK(st.st_mode)) {
			struct utimbuf ts;

			ts.actime = st.st_atime;
			ts.modtime = st.st_mtime;
			if (utime(dest, &ts))
				return errno;
		}
#endif
	}

#ifdef HAVE_FCHMODAT
	ret = fchmodat(AT_FDCWD, dest, st.st_mode, AT_SYMLINK_NOFOLLOW);

	if (!ret);
	/* fchmodat() may or may not support touching symlinks,
	 * if it doesn't, fall back to chmod() */
	else if (errno != EINVAL
#ifdef EOPNOTSUPP /* POSIX-2008 */
			&& errno != EOPNOTSUPP
#endif
#ifdef ENOTSUP /* glibc */
			&& errno != ENOTSUP
#endif
			)
		return errno;
	else
#endif
	if (!S_ISLNK(st.st_mode)) {
		if (chmod(dest, st.st_mode & ~S_IFMT))
			return errno;
	}

	return 0;
}

#ifdef HAVE_LIBATTR
static int attr_allow_all(const char *name, struct error_context *err) {
	return 1;
}
#endif

/**
 * ai_cp_attr
 * @source: source file
 * @dest: destination file
 *
 * Copy extended attributes from @source to @dest.
 */
static void ai_cp_attr(const char *source, const char *dest) {
#ifdef HAVE_LIBATTR
	attr_copy_file(source, dest, NULL, NULL);
#endif
}

int ai_cp_a(const char *source, const char *dest) {
	int ret;
	struct stat st;

	/* First lstat() it, see what we got. */
	if (lstat(source, &st))
		return errno;

	/* Is it a symlink? */
	if (S_ISLNK(st.st_mode))
		ret = ai_cp_symlink(source, dest, st.st_size);
	else if (S_ISREG(st.st_mode))
		ret = ai_cp_reg(source, dest, st.st_size);
	else if (S_ISDIR(st.st_mode)) {
		if (mkdir(dest, st.st_mode & ~S_IFMT))
			ret = errno;
		else
			ret = 0;
	} else
		return EINVAL;

	if (!ret) {
		ret = ai_cp_stat(dest, st);
		if (!ret) {
			ai_cp_attr(source, dest);
		}
	}

	return ret;
}
