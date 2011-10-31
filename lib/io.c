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

int ai_mv(const char *source, const char *dest) {
	if (!rename(source, dest))
		return 0;

	/* cross-device? try manually. */
	if (errno == EXDEV) {
		int ret = ai_cp(source, dest);
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
		return ai_cp(source, dest);

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

static int ai_cp_reg(const char *source, const char *dest) {
	return -1;
}

int ai_cp(const char *source, const char *dest) {
	int ret;
	struct stat st;

	/* First lstat() it, see what we got. */
	if (lstat(source, &st))
		return errno;

	/* Is it a symlink? */
	if (S_ISLNK(st.st_mode))
		ret = ai_cp_symlink(source, dest, st.st_size);
	else if (S_ISREG(st.st_mode))
		ret = ai_cp_reg(source, dest);
	else
		return EINVAL;

	if (!ret)
		/* XXX, clone attributes */;

	return ret;
}
