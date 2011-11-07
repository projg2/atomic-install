/* atomic-install -- merge process
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

/**
 * ai_merge_constraint_flags
 * @j: an open journal
 * @required: flags which have to be set in the journal
 * @unallowed: flags which can't be set in the journal
 *
 * Check the flags field of journal @j for @required and @unallowed flags.
 *
 * Returns: true if all required flags are set and unallowed aren't, false otherwise
 */
static int ai_merge_constraint_flags(ai_journal_t j, uint32_t required, uint32_t unallowed) {
	return (ai_journal_get_flags(j) & (required|unallowed)) == required;
}

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t oldpathlen = strlen(source) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .new */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *oldpathbuf, *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	if (!ai_merge_constraint_flags(j, 0, AI_MERGE_COPIED_NEW|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

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

	/* Mark as done. */
	if (!ret)
		ret = ai_journal_set_flag(j, AI_MERGE_COPIED_NEW);

	return ret;
}

int ai_merge_rollback_new(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* + .<fn-prefix>~ + .new */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	if (!ai_merge_constraint_flags(j, AI_MERGE_COPIED_NEW, 0))
		return EINVAL;

	/* Mark rollback as started. */
	ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (ret)
		return ret;

	newpathbuf = malloc(newpathlen);
	if (!newpathbuf)
		return errno;

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		sprintf(newpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);

		if (unlink(newpathbuf) && errno != ENOENT) {
			ret = errno;
			break;
		}

		/* XXX: remove new directories */
	}

	free(newpathbuf);

	return ret;
}

int ai_merge_backup_old(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *oldpathbuf, *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	/* Already done? */
	if (!ai_merge_constraint_flags(j, 0, AI_MERGE_BACKED_OLD_UP|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

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

		sprintf(oldpathbuf, "%s%s%s", dest, path, name);
		sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);

		ret = ai_cp_l(oldpathbuf, newpathbuf);
		if (!ret)
			ret = ai_journal_file_set_flag(pp, AI_MERGE_FILE_BACKED_UP);
		if (ret && ret != ENOENT)
			break;
	}

	free(oldpathbuf);
	free(newpathbuf);

	/* Mark as done. */
	if (!ret || ret == ENOENT)
		ret = ai_journal_set_flag(j, AI_MERGE_BACKED_OLD_UP);

	return ret;
}

int ai_merge_rollback_old(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator
	 * + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);
	const uint32_t flags = ai_journal_get_flags(j);

	char *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	/* replace could be started already; we need to rollback that instead */
	if (!ai_merge_constraint_flags(j, AI_MERGE_BACKED_OLD_UP, AI_MERGE_COPIED_NEW))
		return EINVAL;

	/* Mark rollback as started. */
	ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (ret)
		return ret;

	newpathbuf = malloc(newpathlen);
	if (!newpathbuf)
		return errno;

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);

		if (unlink(newpathbuf) && errno != ENOENT) {
			ret = errno;
			break;
		}

		/* XXX: remove new directories */
	}

	free(newpathbuf);

	return ret;
}

int ai_merge_replace(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const uint32_t j_flags = ai_journal_get_flags(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t newpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .new */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *oldpathbuf, *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	if (!ai_merge_constraint_flags(j,
				AI_MERGE_COPIED_NEW|AI_MERGE_BACKED_OLD_UP,
				AI_MERGE_REPLACED|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

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

		sprintf(oldpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);
		sprintf(newpathbuf, "%s%s%s", dest, path, name);

		ret = ai_mv(oldpathbuf, newpathbuf);
		if (ret)
			break;
	}

	free(oldpathbuf);
	free(newpathbuf);

	/* Mark as done. */
	if (!ret)
		ret = ai_journal_set_flag(j, AI_MERGE_REPLACED);

	return ret;
}

int ai_merge_rollback_replace(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const uint32_t j_flags = ai_journal_get_flags(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t newpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .old */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *oldpathbuf, *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	if (!ai_merge_constraint_flags(j,
				AI_MERGE_COPIED_NEW|AI_MERGE_BACKED_OLD_UP,
				AI_MERGE_REPLACED))
		return EINVAL;

	oldpathbuf = malloc(oldpathlen);
	if (!oldpathbuf)
		return errno;

	newpathbuf = malloc(newpathlen);
	if (!newpathbuf) {
		free(oldpathbuf);
		return errno;
	}

	/* Mark rollback as started. */
	ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (ret)
		return ret;

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		/* if backed up, then restore */
		if (ai_journal_file_flags(pp) & AI_MERGE_FILE_BACKED_UP) {
			sprintf(oldpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);
			sprintf(newpathbuf, "%s%s%s", dest, path, name);

			ret = ai_mv(oldpathbuf, newpathbuf);
		} else { /* just unlink the new one */
			if (unlink(newpathbuf))
				ret = errno;
		}

		if (ret && ret != ENOENT)
			break;
	}

	free(oldpathbuf);
	free(newpathbuf);

	return ret == ENOENT ? 0 : ret;
}

int ai_merge_cleanup(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator
	 * + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	char *newpathbuf;
	ai_journal_file_t *pp;

	int ret = 0;

	if (!ai_merge_constraint_flags(j, AI_MERGE_REPLACED, 0))
		return EINVAL;

	newpathbuf = malloc(newpathlen);
	if (!newpathbuf)
		return errno;

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		if (!(ai_journal_file_flags(pp) & AI_MERGE_FILE_BACKED_UP))
			continue;

		sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);

		if (unlink(newpathbuf) && errno != ENOENT) {
			ret = errno;
			break;
		}
	}

	free(newpathbuf);

	return ret;
}
