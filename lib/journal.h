/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_JOURNAL_H
#define _ATOMIC_INSTALL_JOURNAL_H

typedef struct ai_journal *ai_journal_t;
typedef const char ai_journal_file_t;

int ai_journal_create(const char *journal_path, const char *location);

int ai_journal_open(const char *journal_path, ai_journal_t *ret);
int ai_journal_close(ai_journal_t j);

ai_journal_file_t *ai_journal_get_files(ai_journal_t j);
int ai_journal_get_maxpathlen(ai_journal_t j);
const char *ai_journal_get_filename_prefix(ai_journal_t j);

const char *ai_journal_file_path(ai_journal_file_t *f);
const char *ai_journal_file_name(ai_journal_file_t *f);
ai_journal_file_t *ai_journal_file_next(ai_journal_file_t *f);

unsigned char ai_journal_get_flags(ai_journal_t j);
int ai_journal_set_flag(ai_journal_t j, unsigned char new_flag);

#endif /*_ATOMIC_INSTALL_JOURNAL_H*/
