/* atomic-install -- merge process
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.hxx"
#include "journal.hxx"
#include "merge.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#include <stdexcept>

#include "exceptions.hxx"

namespace ai = atomic_install;

static void ai_mkdir_cp(char *source, char *dest, const char *path,
		ai_merge_progress_callback_t progress_callback)
{
	char *sp = strrchr(source, '/') - strlen(path) + 1;
	char *dp = strrchr(dest, '/') - strlen(path) + 1;

	const char *relpath = sp;

	while (sp) {
		*sp = 0;
		*dp = 0;

		if (progress_callback && *relpath)
			progress_callback(relpath, 0, 0);
		/* Try to copy the directory entry */
		try
		{
			try
			{
				ai_cp_a(source, dest);
			}
			catch (ai::io_error& e)
			{
				if (e != EEXIST && e != EISDIR)
					throw;
			}
		}
		catch (std::exception& e)
		{
			*sp = '/';
			*dp = '/';

			throw;
		}
		*sp = '/';
		*dp = '/';

		sp = strchr(sp+1, '/');
		dp = strchr(dp+1, '/');
	}
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

typedef void (*copy_func_t)(const char*, const char*);

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j,
		ai_merge_progress_callback_t progress_callback)
{
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t oldpathlen = strlen(source) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .new */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;
	const char *relpath;

	if (!ai_merge_constraint_flags(j, 0, AI_MERGE_COPIED_NEW|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

	char oldpathbuf[oldpathlen];
	char newpathbuf[newpathlen];

	relpath = oldpathbuf + strlen(source);

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		const unsigned char flags = ai_journal_file_flags(pp);
		copy_func_t copy_func;

		sprintf(oldpathbuf, "%s%s%s", source, path, name);
		if (flags & AI_MERGE_FILE_DIR) {
			sprintf(newpathbuf, "%s%s%s", dest, path, name);
			copy_func = ai_cp_a;
		} else {
			sprintf(newpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);
			copy_func = ai_cp_l;
		}

		if (flags & AI_MERGE_FILE_REMOVE) {
			struct stat tmp;

			/* file exists in sourcedir -> will be replaced -> ignore */
			if (!lstat(oldpathbuf, &tmp)) {
				const int sf_ret = ai_journal_file_set_flag(pp, AI_MERGE_FILE_IGNORE);
				if (sf_ret)
					return sf_ret;
			}
			continue;
		}

		if (progress_callback && !(flags & AI_MERGE_FILE_DIR))
			progress_callback(relpath, 0, 0);
		try
		{
			copy_func(oldpathbuf, newpathbuf);
		}
		catch (ai::io_error& e)
		{
			// try to create parent directories
			if (e == ENOENT)
			{
				ai_mkdir_cp(oldpathbuf, newpathbuf, path, progress_callback);
				copy_func(oldpathbuf, newpathbuf);
			}
		}
	}

	/* Mark as done. */
	ai_journal_set_flag(j, AI_MERGE_COPIED_NEW);

	return 0;
}

int ai_merge_rollback_new(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* + .<fn-prefix>~ + .new */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	/* Mark rollback as started. */
	const int sf_ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (sf_ret)
		return sf_ret;

	char newpathbuf[newpathlen];

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		const unsigned char flags = ai_journal_file_flags(pp);

		if (flags & AI_MERGE_FILE_REMOVE)
			continue;

		if (flags & AI_MERGE_FILE_DIR)
			sprintf(newpathbuf, "%s%s%s", dest, path, name);
		else
			sprintf(newpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);

		if (remove(newpathbuf) && errno != ENOENT
				&& errno != ENOTEMPTY && errno != EEXIST)
			throw ai::io_error("remove()", errno, newpathbuf);

		/* XXX: remove new directories */
	}

	return 0;
}

int ai_merge_backup_old(const char *dest, ai_journal_t j)
{
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	/* Already done? */
	/* AI_MERGE_COPIED_NEW required due to AI_MERGE_FILE_REMOVE marking. */
	if (!ai_merge_constraint_flags(j, AI_MERGE_COPIED_NEW,
				AI_MERGE_BACKED_OLD_UP|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

	char oldpathbuf[oldpathlen];
	char newpathbuf[newpathlen];

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp))
	{
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		unsigned char flags = ai_journal_file_flags(pp);

		if (flags & (AI_MERGE_FILE_IGNORE|AI_MERGE_FILE_DIR))
			continue;

		sprintf(oldpathbuf, "%s%s%s", dest, path, name);

		if (flags & AI_MERGE_FILE_REMOVE)
		{
			struct stat st;

			/* omit directories */
			if (!lstat(oldpathbuf, &st) && S_ISDIR(st.st_mode)) {
				const int sf_ret = ai_journal_file_set_flag(pp, AI_MERGE_FILE_DIR);
				if (sf_ret)
					return sf_ret;
				continue;
			}
		}

		sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);

		try
		{
			ai_cp_l(oldpathbuf, newpathbuf);
			const int sf_ret = ai_journal_file_set_flag(pp, AI_MERGE_FILE_BACKED_UP);
			if (sf_ret)
				return sf_ret;
		}
		catch (ai::io_error& e)
		{
			if (e != ENOENT)
				throw;
		}
	}

	/* Mark as done. */
	const int sf_ret = ai_journal_set_flag(j, AI_MERGE_BACKED_OLD_UP);
	return sf_ret;
}

int ai_merge_rollback_old(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator
	 * + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	/* replace could be started already; we need to rollback that instead */
	if (!ai_merge_constraint_flags(j, 0, AI_MERGE_BACKED_OLD_UP))
		return EINVAL;

	/* Mark rollback as started. */
	const int sf_ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (sf_ret)
		return sf_ret;

	char newpathbuf[newpathlen];

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);

		if (ai_journal_file_flags(pp) & (AI_MERGE_FILE_IGNORE|AI_MERGE_FILE_DIR))
			continue;

		sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);

		if (unlink(newpathbuf) && errno != ENOENT)
			throw ai::io_error("unlink()", errno, newpathbuf);

		/* XXX: remove new directories */
	}

	return 0;
}

int ai_merge_replace(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t newpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .new */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	if (!ai_merge_constraint_flags(j,
				AI_MERGE_COPIED_NEW|AI_MERGE_BACKED_OLD_UP,
				AI_MERGE_REPLACED|AI_MERGE_ROLLBACK_STARTED))
		return EINVAL;

	char oldpathbuf[oldpathlen];
	char newpathbuf[newpathlen];

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp)) {
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		const unsigned char flags = ai_journal_file_flags(pp);

		if (flags & (AI_MERGE_FILE_IGNORE|AI_MERGE_FILE_DIR))
			continue;

		sprintf(newpathbuf, "%s%s%s", dest, path, name);

		if (flags & AI_MERGE_FILE_REMOVE) {
			if (flags & AI_MERGE_FILE_DIR)
				continue;
			if (unlink(newpathbuf) && errno != ENOENT)
				throw ai::io_error("unlink()", errno, newpathbuf);
		} else {
			sprintf(oldpathbuf, "%s%s.%s~%s.new", dest, path, fn_prefix, name);
			ai_mv(oldpathbuf, newpathbuf);
		}
	}

	/* Mark as done. */
	const int sf_ret = ai_journal_set_flag(j, AI_MERGE_REPLACED);
	return sf_ret;
}

int ai_merge_rollback_replace(const char *dest, ai_journal_t j) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator */
	const size_t newpathlen = strlen(dest) + maxpathlen + 1;
	/* + .<fn-prefix>~ + .old */
	const size_t oldpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	if (!ai_merge_constraint_flags(j,
				AI_MERGE_COPIED_NEW|AI_MERGE_BACKED_OLD_UP,
				AI_MERGE_REPLACED))
		return EINVAL;

	char oldpathbuf[oldpathlen];
	char newpathbuf[newpathlen];

	/* Mark rollback as started. */
	const int sf_ret = ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
	if (sf_ret)
		return sf_ret;

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp))
	{
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		const unsigned char flags = ai_journal_file_flags(pp);

		if (flags & (AI_MERGE_FILE_IGNORE|AI_MERGE_FILE_DIR))
			continue; /* ignore duplicates */

		try
		{
			/* if backed up, then restore */
			if (flags & AI_MERGE_FILE_BACKED_UP)
			{
				sprintf(oldpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);
				sprintf(newpathbuf, "%s%s%s", dest, path, name);

				ai_mv(oldpathbuf, newpathbuf);
			}
			else
			{
				// just unlink the new one
				if (unlink(newpathbuf))
					throw ai::io_error("unlink()", errno, newpathbuf);
			}
		}
		catch (ai::io_error& e)
		{
			if (e != ENOENT)
				throw;
		}
	}

	return 0;
}

int ai_merge_cleanup(const char *dest, ai_journal_t j,
		ai_merge_removal_callback_t removal_callback) {
	const uint64_t maxpathlen = ai_journal_get_maxpathlen(j);
	const char *fn_prefix = ai_journal_get_filename_prefix(j);
	/* maxpathlen covers path + filename, + 1 for null terminator
	 * + .<fn-prefix>~ + .old */
	const size_t newpathlen = strlen(dest) + maxpathlen + 7 + strlen(fn_prefix);

	ai_journal_file_t *pp;

	if (!ai_merge_constraint_flags(j, AI_MERGE_REPLACED, 0))
		return EINVAL;

	char newpathbuf[newpathlen];

	for (pp = ai_journal_get_files(j); pp; pp = ai_journal_file_next(pp))
	{
		const char *path = ai_journal_file_path(pp);
		const char *name = ai_journal_file_name(pp);
		const unsigned char flags = ai_journal_file_flags(pp);

		if (removal_callback && (flags & AI_MERGE_FILE_REMOVE)) {
			sprintf(newpathbuf, "%s%s", path, name);
			if (flags & AI_MERGE_FILE_IGNORE)
				removal_callback(newpathbuf, EEXIST);
			else if (!(flags & (AI_MERGE_FILE_BACKED_UP|AI_MERGE_FILE_DIR)))
				removal_callback(newpathbuf, ENOENT);
		}

		if (flags & (AI_MERGE_FILE_IGNORE|AI_MERGE_FILE_DIR))
			continue;

		if (flags & AI_MERGE_FILE_DIR)
			sprintf(newpathbuf, "%s%s%s", dest, path, name);
		else if (flags & AI_MERGE_FILE_BACKED_UP)
			sprintf(newpathbuf, "%s%s.%s~%s.old", dest, path, fn_prefix, name);
		else
			continue;

		if (remove(newpathbuf))
		{
			if (errno == EEXIST)
				errno = ENOTEMPTY;
			else if (errno != ENOENT && errno != ENOTEMPTY)
				throw ai::io_error("remove()", errno, newpathbuf);
		}
		else
			errno = 0;

		if (removal_callback && (flags & AI_MERGE_FILE_REMOVE)) {
			sprintf(newpathbuf, "%s%s", path, name);
			removal_callback(newpathbuf, errno);
		}
	}

	return 0;
}
