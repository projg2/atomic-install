/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_LIBATTR
#	include <attr/libattr.h>
#endif

#include <stdexcept>
#include "exceptions.hxx"

namespace ai = atomic_install;

ai::local_fd::local_fd(int fd) throw()
	: _fd(fd)
{
}

ai::local_fd::~local_fd() throw()
{
	close(_fd);
}

ai::local_fd::operator int() const throw()
{
	return _fd;
}

void ai_mv(const char *source, const char *dest)
{
	if (!rename(source, dest))
		return;

	/* cross-device? try manually. */
	if (errno == EXDEV)
	{
		try
		{
			ai_cp_a(source, dest);
		}
		catch (std::exception& e)
		{
			unlink(source);
			throw;
		};
	}
	else
		throw ai::io_error("rename()", errno, source, dest);
}

void ai_cp_l(const char *source, const char *dest)
{
	/* link() will not overwrite */
	if (unlink(dest) && errno != ENOENT)
		throw ai::io_error("unlink()", errno, dest);

	if (!link(source, dest))
		return;

	/* cross-device or not supported? try manually. */
	if (errno == EXDEV || errno == EACCES || errno == EPERM)
		ai_cp_a(source, dest);
	else
		throw ai::io_error("link()", errno, source, dest);
}

static void ai_cp_symlink(const char *source, const char *dest, ssize_t symlen)
{
	char buf[symlen + 1];

	/* ensure content length didn't change */
	if (readlink(source, buf, symlen + 1) != symlen)
		throw std::runtime_error("symlink length changed"); // XXX

	/* null terminate */
	buf[symlen + 1] = 0;

	if (symlink(buf, dest))
		throw ai::io_error("symlink()", errno, buf, dest);
}

#ifndef AI_BUFSIZE
#	define AI_BUFSIZE 65536
#endif

static int ai_splice(int fd_in, int fd_out)
{
	static char buf[AI_BUFSIZE];
	char *bufp = buf;
	ssize_t ret, wr = 0;

	ret = read(fd_in, buf, sizeof(buf));
	if (ret == -1)
	{
		if (errno == EINTR)
			return 1; // force retry
		else
			throw ai::io_error("read() [source]", errno);
	}

	while (ret > 0)
	{
		wr = write(fd_out, bufp, ret);
		if (wr == -1)
		{
			if (errno != EINTR)
				throw ai::io_error("write() [dest]", errno);
		}
		else
		{
			ret -= wr;
			bufp += wr;
		}
	}

	return wr;
}

static void ai_cp_reg(const char *source, const char *dest, off_t expsize)
{
	int splret;

	ai::local_fd fd_in(open(source, O_RDONLY));
	if (fd_in == -1)
		throw ai::io_error("open()", errno, source);

	/* don't care about perms, will have to chmod anyway */
	ai::local_fd fd_out = creat(dest, 0666);
	if (fd_out == -1)
		throw ai::io_error("creat()", errno, dest);

#ifdef HAVE_POSIX_FALLOCATE
	if (expsize != 0)
	{
		const int ret = posix_fallocate(fd_out, 0, expsize);

		if (ret)
			throw ai::io_error("posix_fallocate()", ret, dest);
	}
#endif

#ifdef HAVE_POSIX_FADVISE
	posix_fadvise(fd_in, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
	posix_fadvise(fd_out, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
	do
	{
		try
		{
			splret = ai_splice(fd_in, fd_out);
		}
		catch (ai::io_error& e)
		{
			e.set_paths(source, dest);
		}
	}
	while (splret > 0);
}

static void ai_cp_stat(const char *dest, struct stat st)
{
	if (lchown(dest, st.st_uid, st.st_gid))
		throw ai::io_error("lchown()", errno, dest);

	/* there's no point in copying directory mtime,
	 * it will be modified when copying files anyway */
	if (!S_ISDIR(st.st_mode))
	{
#ifdef HAVE_UTIMENSAT
		struct timespec ts[2];

		ts[0] = st.st_atim;
		ts[1] = st.st_mtim;
		if (utimensat(AT_FDCWD, dest, ts, AT_SYMLINK_NOFOLLOW))
			throw ai::io_error("utimensat()", errno, dest);
#else
		/* utime() can't handle touching symlinks */
		if (!S_ISLNK(st.st_mode))
		{
			struct utimbuf ts;

			ts.actime = st.st_atime;
			ts.modtime = st.st_mtime;
			if (utime(dest, &ts))
				throw ai::io_error("utime()", errno, dest);
		}
#endif
	}

#ifdef HAVE_FCHMODAT
	if (!fchmodat(AT_FDCWD, dest, st.st_mode, AT_SYMLINK_NOFOLLOW));
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
		throw ai::io_error("fchmodat()", errno, dest);
	else
#endif
	if (!S_ISLNK(st.st_mode))
	{
		if (chmod(dest, st.st_mode & ~S_IFMT))
			throw ai::io_error("chmod()", errno, dest);
	}
}

static void ai_cp_attr(const char *source, const char *dest)
{
#ifdef HAVE_LIBATTR
	attr_copy_file(source, dest, NULL, NULL);
#endif
}

void ai_cp_a(const char *source, const char *dest)
{
	struct stat st;

	/* First lstat() it, see what we got. */
	if (lstat(source, &st))
		throw ai::io_error("lstat()", errno, source);

	/* ensure to remove destination file before proceeding;
	 * otherwise, we could rewrite hardlinked file */
	if (!S_ISDIR(st.st_mode) && unlink(dest) && errno != ENOENT)
		throw ai::io_error("unlink()", errno, dest);

	/* Is it a symlink? */
	if (S_ISLNK(st.st_mode))
		ai_cp_symlink(source, dest, st.st_size);
	else if (S_ISREG(st.st_mode))
		ai_cp_reg(source, dest, st.st_size);
	else
	{
		if (S_ISDIR(st.st_mode)) {
			if (mkdir(dest, st.st_mode & ~S_IFMT) && errno != EEXIST)
				throw ai::io_error("mkdir()", errno, dest);
		}
		else if (S_ISFIFO(st.st_mode))
		{
			if (mkfifo(dest, st.st_mode & ~S_IFMT))
				throw ai::io_error("mkfifo()", errno, dest);
		}
		else if (0
#ifdef S_ISCHR
				|| S_ISCHR(st.st_mode)
#endif
#ifdef S_ISBLK
				|| S_ISBLK(st.st_mode)
#endif
				)
		{
			if (mknod(dest, st.st_mode, st.st_rdev))
				throw ai::io_error("mknod()", errno, dest);
		}
		else
			throw std::runtime_error("Invalid file type"); // XXX
	}

	ai_cp_stat(dest, st);
	ai_cp_attr(source, dest);
}
