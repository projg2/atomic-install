/* libcopy -- merge process
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_MERGE_H
#define _ATOMIC_INSTALL_MERGE_H

#include "journal.h"

enum ai_merge_flags {
	AI_MERGE_COPIED_NEW = 1,
	AI_MERGE_BACKED_OLD_UP = 2,
	AI_MERGE_REPLACED = 4
};

enum ai_merge_file_flags {
	AI_MERGE_FILE_BACKED_UP = 1
};

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j);
int ai_merge_backup_old(const char *dest, ai_journal_t j);
int ai_merge_replace(const char *dest, ai_journal_t j);

#endif /*_ATOMIC_INSTALL_MERGE_H*/
