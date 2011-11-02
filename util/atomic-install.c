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
	int ret;

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

	ret = ai_merge_copy_new(argv[2], argv[3], j);
	if (ret)
		printf("Copying new failed: %s\n", strerror(ret));

	ret = ai_journal_close(j);
	if (ret)
		printf("Journal close failed: %s\n", strerror(ret));

	return ret;
}
