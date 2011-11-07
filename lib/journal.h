/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_JOURNAL_H
#define _ATOMIC_INSTALL_JOURNAL_H

/**
 * SECTION: journal
 * @short_description: Utility functions for journal handling
 * @include: atomic-install/journal.h
 *
 * libjournal provides a set of functions to create and use atomic-install
 * journal files.
 */

/**
 * ai_journal_t
 *
 * The type describing an open journal. Returned by ai_journal_open(); when done
 * with it, pass to ai_journal_close().
 */
typedef struct ai_journal *ai_journal_t;
/**
 * ai_journal_file_t
 *
 * The type describing a single file in the journal. Used via a pointer.
 */
typedef char ai_journal_file_t;

/**
 * ai_journal_create
 * @journal_path: path for the new journal file
 * @location: source tree location
 *
 * Create a new journal file and fill it with files from source tree @location.
 * This function doesn't open the newly-created journal; for that, use
 * ai_journal_open() afterwards.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_journal_create(const char *journal_path, const char *location);

/**
 * ai_journal_open
 * @journal_path: path to the journal file
 * @ret: location to store new #ai_journal_t
 *
 * Open the journal file at @journal_path and put #ai_journal_t for it at
 * location pointed by @ret. Note that @ret may be modified even if this
 * function fails (e.g. when journal contents are invalid).
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_journal_open(const char *journal_path, ai_journal_t *ret);
/**
 * ai_journal_close
 * @j: an open journal
 *
 * Close the journal file.
 *
 * Returns: 0 on success, errno otherwise (very unlikely)
 */
int ai_journal_close(ai_journal_t j);

/**
 * ai_journal_get_maxpathlen
 * @j: an open journal
 *
 * Get the maximum path length for files within the journal. This is the length
 * of ai_journal_file_path() + ai_journal_file_name() for the longest path in
 * the journal. This variable can be used to allocate buffer quickly.
 *
 * Returns: maximum path length
 */
int ai_journal_get_maxpathlen(ai_journal_t j);
/**
 * ai_journal_get_filename_prefix
 * @j: an open journal
 *
 * Get the random prefix used for temporary files associated with this journal
 * (session).
 *
 * Returns: a pointer to null-terminated prefix in the journal
 */
const char *ai_journal_get_filename_prefix(ai_journal_t j);

/**
 * ai_journal_get_files
 * @j: an open journal
 *
 * Get the pointer to the first file in journal.
 *
 * Returns: a pointer to #ai_journal_file_t, or %NULL if no files
 */
ai_journal_file_t *ai_journal_get_files(ai_journal_t j);
/**
 * ai_journal_file_next
 * @f: the current file
 *
 * Get the pointer to the next file in journal.
 *
 * Returns: a pointer to #ai_journal_file_t, or %NULL if @f is last
 */
ai_journal_file_t *ai_journal_file_next(ai_journal_file_t *f);

/**
 * ai_journal_file_flags
 * @f: the file
 *
 * Get flags for the specified file.
 *
 * Returns: an 8-bit flag field contents
 */
unsigned char ai_journal_file_flags(ai_journal_file_t *f);
/**
 * ai_journal_file_set_flag
 * @f: the file
 * @new_flag: bitfield for new flags to set
 *
 * Set specified flag for the file.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_journal_file_set_flag(ai_journal_file_t *f, unsigned char new_flag);
/**
 * ai_journal_file_path
 * @f: the file
 *
 * Get the directory part of file path. The path is guaranteed to have
 * a trailing slash, i.e. files in root directory will return '/'.
 *
 * Returns: a pointer to static, null-terminated path inside journal
 */
const char *ai_journal_file_path(ai_journal_file_t *f);
/**
 * ai_journal_file_name
 * @f: the file
 *
 * Get the filename part of file path.
 *
 * Returns: a pointer to static, null-terminated filename inside journal
 */
const char *ai_journal_file_name(ai_journal_file_t *f);

/**
 * ai_journal_get_flags
 * @j: an open journal
 *
 * Get global journal flags.
 *
 * Returns: a 32-bit flag field contents
 */
unsigned long int ai_journal_get_flags(ai_journal_t j);
/**
 * ai_journal_set_flag
 * @j: an open journal
 * @new_flag: bitfield for new flags to set
 *
 * Set specified flag for the journal. The journal will be synced to disk
 * afterwards.
 *
 * Returns: 0 on success, errno otherwise
 */
int ai_journal_set_flag(ai_journal_t j, unsigned long int new_flag);

#endif /*_ATOMIC_INSTALL_JOURNAL_H*/
