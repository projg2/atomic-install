/* atomic-install -- I/O helper functions
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdio.h>
#include <errno.h>

int mv(const char *source, const char *dest) {
	if (!rename(source, dest))
		return 0;

	return errno;
}

int cp(const char *source, const char *dest) {
	/* link() will not overwrite */
	unlink(dest);

	if (!link(source, dest))
		return 0;

	return errno;
}
