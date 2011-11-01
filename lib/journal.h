/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_JOURNAL_H
#define _ATOMIC_INSTALL_JOURNAL_H

typedef struct ai_journal *journal_t;
typedef const char journal_file_t;

int ai_journal_create(const char *journal_path, const char *location);

int ai_journal_open(const char *journal_path, journal_t *ret);
int ai_journal_close(journal_t j);

journal_file_t *ai_journal_get_files(journal_t j);
int ai_journal_get_maxpathlen(journal_t j);

const char *ai_journal_file_path(journal_file_t *f);
const char *ai_journal_file_name(journal_file_t *f);
journal_file_t *ai_journal_file_next(journal_file_t *f);

#endif /*_ATOMIC_INSTALL_JOURNAL_H*/
