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

/**
 * ai_mkdir_cp
 * @source: source tree path buffer
 * @dest: dest tree path buffer
 * @path: path relative to both trees
 *
 * Create all missing directories for @path in @dest tree. Copy attributes for
 * the newly-created directories from @source tree.
 *
 * Note that both @source and @dest will be modified during run-time, thus they
 * have to be writable. The initial contents will be restored on return.
 *
 * Returns: 0 on success, errno on failure
 */
static int ai_mkdir_cp(char *source, char *dest, const char *path) {
	char *sp = strrchr(source, '/') - strlen(path) + 1;
	char *dp = strrchr(dest, '/') - strlen(path) + 1;

	while (sp) {
		int ret;

		*sp = 0;
		*dp = 0;

		/* Try to copy the directory entry */
		ret = ai_cp_a(source, dest);

		*sp = '/';
		*dp = '/';

		if (ret && ret != EEXIST)
			return ret;

		sp = strchr(sp+1, '/');
		dp = strchr(dp+1, '/');
	}

	return 0;
}

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	const size_t oldpathlen = strlen(source) + maxpathlen + 1;
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *oldpathbuf, *newpathbuf;
	ai_journal_file_t *pp;

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
		sprintf(newpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);

		ret = ai_cp_l(oldpathbuf, newpathbuf);

		if (ret == ENOENT) {
			ret = ai_mkdir_cp(oldpathbuf, newpathbuf, path);
			if (!ret)
				ret = ai_cp_l(oldpathbuf, newpathbuf);
		}

		if (ret)
			break;
	}

	free(oldpathbuf);
	free(newpathbuf);

	return ret;
}
