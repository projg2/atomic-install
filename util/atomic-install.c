/* atomic-install
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#include "lib/journal.h"
#include "lib/merge.h"

int main(int argc, char *argv[]) {
	ai_journal_t j;
	int ret, ret2;

	const char *journal_file, *source, *dest;

	if (argc < 4) {
		printf("Synopsis: atomic-install journal source dest\n");
		return 0;
	}

	journal_file = argv[1];
	source = argv[2];
	dest = argv[3];

	/* Try to open.
	 * If it doesn't exist, try to create and then open. */
	ret = ai_journal_open(journal_file, &j);
	if (!ret)
		printf("* Journal file open, resuming.\n");
	else if (ret == ENOENT) {
		printf("* Journal not found, creating...\n");

		ret = ai_journal_create(journal_file, source);
		if (ret) {
			printf("Journal creation failed: %s\n", strerror(ret));
			return ret;
		}

		ret = ai_journal_open(journal_file, &j);
	}
	if (ret) {
		printf("Journal open failed: %s\n", strerror(ret));
		return ret;
	}

	while (1) {
		const uint32_t flags = ai_journal_get_flags(j);

		if (flags & AI_MERGE_ROLLBACK_STARTED) {
			/* Proceed with rollback. */
			if (flags & AI_MERGE_BACKED_OLD_UP) {
				printf("* Rolling back replacement...\n");
				ret = ai_merge_rollback_replace(dest, j);
				if (ret) {
					printf("* Replacement rollback failed: %s\n", strerror(ret));
					break;
				}
			} else {
				printf("* Rolling back old backup...\n");
				ret = ai_merge_rollback_old(dest, j);
				if (ret) {
					printf("* Old rollback failed: %s\n", strerror(ret));
					break;
				}
			}
			printf("* Rolling back new copying...\n");
			ret = ai_merge_rollback_new(dest, j);
			if (ret)
				printf("* New rollback failed: %s\n", strerror(ret));
			else {
				printf("* Rollback successful.\n");
				if (unlink(journal_file))
					printf("Journal removal failed: %s\n", strerror(errno));
			}
			break;
		} else if (flags & AI_MERGE_REPLACED) {
			printf("* Post-merge clean up...\n");
			ret = ai_merge_cleanup(dest, j);
			if (ret)
				printf("Cleanup failed: %s\n", strerror(ret));
			else {
				printf("* Install done.\n");
				if (unlink(journal_file))
					printf("Journal removal failed: %s\n", strerror(errno));
			}
			break;
		} else if (flags & AI_MERGE_BACKED_OLD_UP && flags & AI_MERGE_COPIED_NEW) {
			printf("* Replacing files...\n");
			ret = ai_merge_replace(dest, j);
			if (ret) {
				printf("Replacement failed: %s\n", strerror(ret));
				ai_journal_set_flag(j, AI_MERGE_ROLLBACK_STARTED);
			}
		} else if (flags & AI_MERGE_COPIED_NEW) {
			printf("* Backing up existing files...\n");
			ret = ai_merge_backup_old(dest, j);
			if (ret) {
				printf("Backing old up failed: %s\n", strerror(ret));
				break;
			}
		} else {
			printf("* Copying new files...\n");
			ret = ai_merge_copy_new(source, dest, j);
			if (ret) {
				printf("Copying new failed: %s\n", strerror(ret));
				break;
			}
		}
	}

	ret2 = ai_journal_close(j);
	if (ret2)
		printf("Journal close failed: %s\n", strerror(ret));

	return ret || ret2;
}
