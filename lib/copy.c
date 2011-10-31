/* libcopy -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>

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

static int ai_splice(int fd_in, int fd_out) {
#ifdef HAVE_SPLICE
	return splice(fd_in, NULL, fd_out, NULL, AI_BUFSIZE, 0);
#else
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
		if (wr == -1 && errno != EINTR)
			return -1;
		ret -= wr;
		bufp += wr;
	}

	return wr;
#endif
}

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

static int ai_cp_stat(const char *dest, struct stat st) {
	if (lchown(dest, st.st_uid, st.st_gid))
		return errno;

	if (!S_ISLNK(st.st_mode)) {
		struct utimbuf ts;

		ts.actime = st.st_atime;
		ts.modtime = st.st_mtime;
		if (utime(dest, &ts))
			return errno;

		if (chmod(dest, st.st_mode & ~S_IFMT))
			return errno;
	}

	return 0;
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
	else
		return EINVAL;

	if (!ret)
		ret = ai_cp_stat(dest, st);

	return ret;
}
