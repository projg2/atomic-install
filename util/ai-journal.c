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

	if (argc < 2) {
		printf("Synopsis: ai-journal journal.aij [location]\n");
		return 0;
	}

	if (argc < 3) {
		ai_journal_t j;

		ret = ai_journal_open(argv[1], &j);
		if (ret)
			printf("Journal open failed: %s\n", strerror(ret));
		else {
			ai_journal_file_t *p;

			for (p = ai_journal_get_files(j); p; p = ai_journal_file_next(p))
				printf("%s%s\n", ai_journal_file_path(p), ai_journal_file_name(p));

			ret = ai_journal_close(j);
			if (ret)
				printf("Journal close failed: %s\n", strerror(ret));
		}
	} else {
		ret = ai_journal_create(argv[1], argv[2]);
		if (ret)
			printf("Journal creation failed: %s\n", strerror(ret));
	}

	return ret;
}
