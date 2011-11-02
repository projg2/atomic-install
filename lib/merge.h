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

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j);

#endif /*_ATOMIC_INSTALL_MERGE_H*/
