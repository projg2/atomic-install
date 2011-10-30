/* atomic-install -- I/O helper functions
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int mv(const char *source, const char *dest) {
	if (!rename(source, dest))
		return 0;

	/* cross-device? try manually. */
	if (errno == EXDEV) {
		int ret = clonefile(source, dest);
		if (!ret)
			unlink(source);
		return ret;
	}

	return errno;
}

int cp(const char *source, const char *dest) {
	/* link() will not overwrite */
	unlink(dest);

	if (!link(source, dest))
		return 0;

	/* cross-device or not supported? try manually. */
	if (errno == EXDEV || errno == EACCES)
		return clonefile(source, dest);

	return errno;
}

static int clone_link(const char *source, const char *dest, ssize_t symlen) {
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

static int clone_reg(const char *source, const char *dest) {
	return -1;
}

int clonefile(const char *source, const char *dest) {
	int ret;
	struct stat st;

	/* First lstat() it, see what we got. */
	if (lstat(source, &st))
		return errno;

	/* Is it a symlink? */
	if (S_ISLNK(st.st_mode))
		ret = clone_link(source, dest, st.st_size);
	else if (S_ISREG(st.st_mode))
		ret = clone_reg(source, dest);
	else
		return EINVAL;

	if (!ret)
		/* XXX, clone attributes */;

	return ret;
}
