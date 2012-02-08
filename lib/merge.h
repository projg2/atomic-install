/* atomic-install -- merge process
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_MERGE_H
#define _ATOMIC_INSTALL_MERGE_H

/**
 * SECTION: merge
 * @short_description: Functions to perform parts of tree merge
 * @include: atomic-install/merge.h
 *
 * libai-merge provides functions to prepare and merge files from the source
 * tree to the destination tree atomically.
 *
 * Note: the API is unstable right now, and will be undergoing changes.
 */

#include "journal.h"

/**
 * ai_merge_flags_t
 * @AI_MERGE_COPIED_NEW: files have been copied from the source tree to .new
 * @AI_MERGE_BACKED_OLD_UP: existing files in the destination tree were
 *	backed up to .old
 * @AI_MERGE_REPLACED: the actual merge has been performed, and existing files
 *	were replaced by .new
 * @AI_MERGE_ROLLBACK_STARTED: any kind of rollback has been started, and thus
 *	proceeding is no longer allowed
 *
 * An enumeration listing global flags used by libai-merge.
 */
typedef enum {
	AI_MERGE_COPIED_NEW = 1,
	AI_MERGE_BACKED_OLD_UP = 2,
	AI_MERGE_REPLACED = 4,
	AI_MERGE_ROLLBACK_STARTED = 8
} ai_merge_flags_t;

/**
 * ai_merge_file_flags_t
 * @AI_MERGE_FILE_BACKED_UP: the file existed in destination tree, and thus has
 *	been backed up
 * @AI_MERGE_FILE_REMOVE: if the file exists in the destination tree, it shall
 *	be either replaced or removed (i.e. belongs to an older version)
 * @AI_MERGE_FILE_IGNORE: ignore the file entry (e.g. duplicate)
 *
 * An enumeration listing file flags used by libai-merge.
 */
typedef enum {
	AI_MERGE_FILE_BACKED_UP = 1,
	AI_MERGE_FILE_REMOVE = 2,
	AI_MERGE_FILE_IGNORE = 4
} ai_merge_file_flags_t;

/**
 * ai_merge_progress_callback_t
 * @path: relative path to the file being processed
 * @megs: number of mebibytes copied already
 * @total: file size in mebibytes
 *
 * Progress callback function. Called:
 * - before the file is copied - @megs = 0,
 * - while copying large files - @megs != 0,
 * - after copying large file - @megs = @total.
 *
 * One can assume that if at least one @megs != 0 is called, there will be one
 * @megs = @total as well.
 */
typedef void (*ai_merge_progress_callback_t)(
		const char *path,
		unsigned long int megs,
		unsigned long int total);

/**
 * ai_merge_copy_new
 * @source: path to the source tree
 * @dest: path to the destination tree
 * @j: an open journal
 * @progress_callback: callback function for progress reporting, or %NULL
 *
 * Copy files from the source tree at @source to the destination tree at @dest.
 * The new files will be written as temporary files with .new suffix.
 *
 * If all files are copied successfully, the %AI_MERGE_COPIED_NEW flag will be
 * set on journal. Otherwise, the copying process can be either resumed by
 * calling ai_merge_copy_new() again or rolled back using
 * ai_merge_rollback_new().
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j,
		ai_merge_progress_callback_t progress_callback);
/**
 * ai_merge_backup_old
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Backup files in the destination tree which will be replaced during the merge
 * process. The backup copies will be named as temporary files with .old suffix.
 *
 * If all files are backed up successfully, the %AI_MERGE_BACKED_OLD_UP flag
 * will be set on journal. Otherwise, the backup process can be either resumed
 * by calling ai_merge_backup_old() again or rolled back using
 * ai_merge_rollback_old().
 *
 * Note that after this function succeeds, it is no longer possible to call
 * ai_merge_rollback_old() and ai_merge_rollback_replace() has to be used
 * instead.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_backup_old(const char *dest, ai_journal_t j);
/**
 * ai_merge_replace
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Perform the actual merge in the destination tree replacing any existing
 * files.
 *
 * Before calling this function, it is necessary to call ai_merge_copy_new()
 * and ai_merge_backup_old().
 *
 * If all files are merged successfully, the %AI_MERGE_REPLACED flag will be set
 * on journal. Otherwise, ai_merge_rollback_replace() needs to be called ASAP to
 * restore old files. Resuming is not possible.
 *
 * After this function succeeds, it is no longer possible to rollback.
 * ai_merge_cleanup() should be called instead to remove stale temporary files.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_replace(const char *dest, ai_journal_t j);
/**
 * ai_merge_cleanup
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Remove stale temporary files in the destination tree after replacement
 * succeeds. This function can be used only after successful ai_merge_replace().
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_cleanup(const char *dest, ai_journal_t j);

/**
 * ai_merge_rollback_old
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Rollback backing up existing files in the destination tree -- in other words,
 * remove the backup copies.
 *
 * This function can be called only before ai_merge_backup_old() succeeds,
 * and should be used if it fails. After it succeeds,
 * ai_merge_rollback_replace() should be used instead.
 *
 * This function sets %AI_MERGE_ROLLBACK_STARTED flag on journal -- which means
 * that it is no longer possible to call any non-rollback functions after using
 * it.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_rollback_old(const char *dest, ai_journal_t j);
/**
 * ai_merge_rollback_new
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Rollback copying new files to the destination tree -- in other words,
 * remove the .new-suffixed temporary files.
 *
 * This function sets %AI_MERGE_ROLLBACK_STARTED flag on journal -- which means
 * that it is no longer possible to call any non-rollback functions after using
 * it.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_rollback_new(const char *dest, ai_journal_t j);
/**
 * ai_merge_rollback_replace
 * @dest: path to the destination tree
 * @j: an open journal
 *
 * Rollback started replacement process by restoring their backup copies.
 * The backup copies will be removed as well.
 *
 * This function can be called only after ai_merge_backup_old() succeeds.
 * It must be used instead of ai_merge_rollback_old() in that case, and it
 * should be called ASAP if ai_merge_replace() fails.
 *
 * This function sets %AI_MERGE_ROLLBACK_STARTED flag on journal -- which means
 * that it is no longer possible to call any non-rollback functions after using
 * it.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_merge_rollback_replace(const char *dest, ai_journal_t j);

#endif /*_ATOMIC_INSTALL_MERGE_H*/
