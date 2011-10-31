/* atomic-install -- I/O helper tool (for testing)
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdio.h>
#include <string.h>

#include "lib/copy.h"

int main(int argc, char *argv[]) {
	int (*copy_func)(const char*, const char*) = ai_cp;
	int args_index = 1;
	int ret;

	if (argc < 3 || (argv[1][0] == '-' && argc < 4)) {
		printf("Synopsis: ai-cp [-m|-l|--] source dest\n");
		return 0;
	}

	if (argv[1][0] == '-') {
		switch(argv[1][1]) {
			case 'm':
				copy_func = ai_mv;
				break;
			case 'l':
				copy_func = ai_cp_l;
				break;
			case '-':
				break;
			default:
				printf("Invalid arg: %s\n", argv[1]);
				return 0;
		}

		args_index++;
	}

	ret = copy_func(argv[args_index], argv[args_index+1]);
	if (ret)
		printf("Copying failed: %s\n", strerror(ret));

	return ret;
}
