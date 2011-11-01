/* libcopy -- merge process
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"
#include "journal.h"
#include "merge.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

int ai_merge_copy_new(const char *source, const char *dest, journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const size_t oldpathlen = strlen(source) + maxpathlen + 1;
	const size_t newpathlen = strlen(dest) + maxpathlen + 1;

	char *oldpathbuf, *newpathbuf;
	journal_file_t *pp;

	int ret = 0;

	oldpathbuf = malloc(oldpathlen);
	if (!oldpathbuf)
		return errno;

	newpathbuf = malloc(newpathlen);
	if (!newpathbuf) {
		free(oldpathbuf);
		return errno;
	}

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		sprintf(oldpathbuf, "%s%s%s", source, path, name);
		sprintf(newpathbuf, "%s%s.AI~%s.new", dest, path, name);

		if (ai_cp_l(oldpathbuf, newpathbuf)) {
			ret = errno;
			break;
		}
	}

	free(oldpathbuf);
	free(newpathbuf);

	return ret;
}
