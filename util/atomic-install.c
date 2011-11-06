/* atomic-install
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdio.h>
#include <string.h>

#include "lib/journal.h"
#include "lib/merge.h"

int main(int argc, char *argv[]) {
	ai_journal_t j;
	int ret, ret2;

	if (argc < 4) {
		printf("Synopsis: atomic-install journal source dest\n");
		return 0;
	}

	ret = ai_journal_create(argv[1], argv[2]);
	if (ret) {
		printf("Journal creation failed: %s\n", strerror(ret));
		return ret;
	}

	ret = ai_journal_open(argv[1], &j);
	if (ret) {
		printf("Journal open failed: %s\n", strerror(ret));
		return ret;
	}

	do {
		ret = ai_merge_copy_new(argv[2], argv[3], j);
		if (ret) {
			printf("Copying new failed: %s\n", strerror(ret));
			break;
		}

		ret = ai_merge_backup_old(argv[3], j);
		if (ret) {
			printf("Backing old up failed: %s\n", strerror(ret));
			break;
		}

#if 1
		ret = ai_merge_replace(argv[3], j);
		if (ret) {
			printf("Replacement failed: %s\n", strerror(ret));
			break;
		}

		ret = ai_merge_cleanup(argv[3], j);
		if (ret) {
			printf("Cleanup failed: %s\n", strerror(ret));
			break;
		}
#else
#if 0
		ret = ai_merge_rollback_old(argv[3], j);
		if (ret) {
			printf("Old rollback failed: %s\n", strerror(ret));
			break;
		}
#else
		ret = ai_merge_rollback_replace(argv[3], j);
		if (ret) {
			printf("Replace rollback failed: %s\n", strerror(ret));
			break;
		}
#endif
		ret = ai_merge_rollback_new(argv[3], j);
		if (ret) {
			printf("New rollback failed: %s\n", strerror(ret));
			break;
		}
#endif
	} while (0);

	ret2 = ai_journal_close(j);
	if (ret2)
		printf("Journal close failed: %s\n", strerror(ret));

	return ret || ret2;
}
