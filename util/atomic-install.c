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

#include <unistd.h>
#include <getopt.h>

#include "lib/journal.h"
#include "lib/merge.h"

static const struct option opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },

	{ "no-replace", no_argument, NULL, 'n' },
	{ "onestep", no_argument, NULL, '1' },
	{ "resume", no_argument, NULL, 'r' },
	{ "rollback", no_argument, NULL, 'R' },
	{ 0, 0, 0, 0 }
};

static void print_help(const char *argv0) {
	printf("Usage: %s [options] journal-file source dest\n"
"\n"
"Options:\n"
"    --help, -h          this help message\n"
"    --version, -V       print program version\n"
"\n"
"    --no-replace, -n    terminate before the replacement step\n"
"    --onestep, -1       perform a smallest step possible\n"
"    --resume, -r        resume existing merge, do not try creating new one\n"
"    --rollback, -R      roll existing merge back\n"
"", argv0);
}

int main(int argc, char *argv[]) {
	ai_journal_t j;
	int opt;
	int ret, ret2;

	const char *journal_file, *source, *dest;
	int onestep = 0;
	int resume = 0;
	int rollback = 0;
	int noreplace = 0;

	while ((opt = getopt_long(argc, argv, "hV1nrR", opts, NULL)) != -1) {
		switch (opt) {
			case '1':
				onestep = 1;
				break;
			case 'n':
				noreplace = 1;
				break;
			case 'r':
				resume = 1;
				break;
			case 'R':
				rollback = 1;
				break;
			case 'V':
				printf("%s\n", PACKAGE_STRING);
				return 0;
			case 0:
				break;
			default:
				print_help(argv[0]);
				return 0;
		}
	}

	if (argc - optind < 3) {
		printf("Synopsis: atomic-install journal source dest\n");
		return 0;
	}

	journal_file = argv[optind];
	source = argv[optind + 1];
	dest = argv[optind + 2];

	/* Try to open.
	 * If it doesn't exist, try to create and then open. */
	ret = ai_journal_open(journal_file, &j);
	if (!ret)
		printf("* Journal file open, %s.\n",
				rollback ? "rolling back" : "resuming");
	else if (ret == ENOENT && !resume && !rollback) {
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

		if (flags & AI_MERGE_ROLLBACK_STARTED || rollback) {
			/* Proceed with rollback. */
			if (flags & AI_MERGE_REPLACED) {
				printf("! Replacement complete, rollback impossible.\n");
				ret = 1;
				break;
			} else if (flags & AI_MERGE_BACKED_OLD_UP) {
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
			if (noreplace)
				break;
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

		if (onestep)
			break;
	}

	ret2 = ai_journal_close(j);
	if (ret2)
		printf("Journal close failed: %s\n", strerror(ret));

	return ret || ret2;
}
