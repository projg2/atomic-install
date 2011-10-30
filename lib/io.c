/* atomic-install -- I/O helper functions
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdio.h>
#include <errno.h>

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

int clonefile(const char *source, const char *dest) {
	/* XXX */
	return -1;
}
