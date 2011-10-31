/* atomic-install -- journal helper tool (for testing)
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdio.h>
#include <string.h>

#include "lib/journal.h"

int main(int argc, char *argv[]) {
	int args_index = 1;
	int ret;

	if (argc < 3) {
		printf("Synopsis: ai-journal-create journal.aij location\n");
		return 0;
	}

	ret = ai_journal_create(argv[1], argv[2]);
	if (ret)
		printf("Journal creation failed: %s\n", strerror(ret));

	return ret;
}
