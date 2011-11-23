/* atomic-install -- file copying tests
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#ifndef PRIuMAX
#	define PRIuMAX "llu"
#endif
#ifndef PRIxMAX
#	define PRIxMAX "llx"
#endif

enum test_codes {
	T_REGULAR = 'r',
	T_EMPTY = 'e',
	T_SYMLINK = 's',
	T_BROKEN_SYMLINK = 'i',
	T_NAMED_PIPE = 'p',
	T_BLK_DEV = 'b',
	T_CHR_DEV = 'c'
};

int randumness[0x2000] = {0x777};
char ex_linkdest[] = ADDITIONAL_TMPFILE;

static int create_input(const char *path, int fill) {
	FILE *f = fopen(path, "wb");
	int ret = 1;

	if (!f)
		return 0;
	if (fill)
		ret = (fwrite(randumness, sizeof(randumness), 1, f) == 1);

	fclose(f);
	return ret;
}

static void print_diff(const char *output_prefix, const char *msg,
		uintmax_t left, uintmax_t right)
{
	fprintf(stderr, "[%s] %s (%" PRIuMAX " vs %" PRIuMAX ")\n",
			output_prefix, msg, left, right);
}

static void print_diff_x(const char *output_prefix, const char *msg,
		uintmax_t left, uintmax_t right)
{
	fprintf(stderr, "[%s] %s (%" PRIxMAX " vs %" PRIxMAX ")\n",
			output_prefix, msg, left, right);
}

static int compare_files(const char *inp, const char *out, const char *output_prefix) {
	struct stat st_in, st_out;
	int ret = 0;

	if (lstat(inp, &st_in)) {
		perror("lstat(INPUT) failed");
		return 2;
	}
	if (lstat(out, &st_out)) {
		perror("lstat(OUTPUT) failed");
		return 2;
	}

	if (st_in.st_size != st_out.st_size) {
		print_diff(output_prefix, "Size differs",
				st_in.st_size, st_out.st_size);
		ret = 1;
	}

	if (st_in.st_size > 0) {
		if (S_ISREG(st_in.st_mode)) {
			FILE *f = fopen(out, "rb");
			char buf[sizeof(randumness)];

			if (!f) {
				perror("Unable to open output file for reading");
				ret = 2;
			} else {
				if (fread(buf, sizeof(buf), 1, f) != 1) {
					perror("Output file read failed");
					ret = 2;
				} else if (memcmp(buf, randumness, sizeof(buf))) {
					fprintf(stderr, "[%s] File contents differ\n",
							output_prefix);
					ret = 1;
				}

				fclose(f);
			}
		} else if (S_ISLNK(st_in.st_mode)) {
			char buf[sizeof(ex_linkdest)];

			switch (readlink(out, buf, sizeof(buf))) {
				case -1:
					perror("readlink(OUTPUT) failed");
					ret = 2;
					break;
				case sizeof(buf) - 1:
					if (memcmp(buf, ex_linkdest, sizeof(buf))) {
						buf[sizeof(buf) - 1] = 0;
						fprintf(stderr, "[%s] Symlink target differs: %s vs %s\n",
								output_prefix, ex_linkdest, buf);
						ret = 1;
					}
					break;
				default:
					fprintf(stderr, "[%s] Invalid length when getting symlink target\n",
							output_prefix);
			}
		} else {
			fprintf(stderr, "[%s] Unhandled non-empty file format\n",
					output_prefix);
			ret = 77;
		}
	}

#ifdef S_ISCHR
	if (S_ISCHR(st_in.st_mode) && st_in.st_rdev != st_out.st_rdev) {
		print_diff_x(output_prefix, "Character device rdev differs",
				st_in.st_rdev, st_out.st_rdev);
		ret = 1;
	}
#endif
#ifdef S_ISBLK
	if (S_ISBLK(st_in.st_mode) && st_in.st_rdev != st_out.st_rdev) {
		print_diff_x(output_prefix, "Block device rdev differs",
				st_in.st_rdev, st_out.st_rdev);
		ret = 1;
	}
#endif

	if (st_in.st_mode != st_out.st_mode) {
		print_diff_x(output_prefix, "Mode differs",
				st_in.st_mode, st_out.st_mode);
		ret = 1;
	}

	if (st_in.st_uid != st_out.st_uid) {
		print_diff(output_prefix, "UID differs",
				st_in.st_uid, st_out.st_uid);
		ret = 1;
	}

	if (st_in.st_gid != st_out.st_gid) {
		print_diff(output_prefix, "GID differs",
				st_in.st_gid, st_out.st_gid);
		ret = 1;
	}

	if (st_in.st_mtime != st_out.st_mtime) {
		print_diff(output_prefix, "mtime (in seconds) differs",
				st_in.st_mtime, st_out.st_mtime);
		ret = 1;
	}

	return ret;
}

int main(int argc, char *argv[]) {
	const char *code = argv[1];
	const char *slash = strrchr(code, '/');

	int ret;

	/* stupid automake! */
	if (slash)
		code = slash + 1;

	/* XXX: replace tests */
	unlink(INPUT_FILE);
	unlink(OUTPUT_FILE);

	switch (code[0]) {
		case T_REGULAR:
		case T_EMPTY:
			if (!create_input(INPUT_FILE, code[0] == T_REGULAR)) {
				perror("Input creation failed");
				return 2;
			}
			break;
		case T_BROKEN_SYMLINK:
			ex_linkdest[0]++;
		case T_SYMLINK:
			if (symlink(ex_linkdest, INPUT_FILE)) {
				perror("Input symlink creation failed");
				return 77;
			}
			break;
		case T_NAMED_PIPE:
			if (mkfifo(INPUT_FILE, 0700)) {
				perror("Named pipe creation failed");
				return 77;
			}
			break;
		case T_BLK_DEV:
#ifdef S_IFBLK
			if (mknod(INPUT_FILE, 0700 | S_IFBLK, 0xff00)) {
				perror("Block device creation failed");
				return 77;
			}
#else
			fprintf(stderr, "Block devices not supported\n");
			return 77;
#endif
			break;
		case T_CHR_DEV:
#ifdef S_IFCHR
			if (mknod(INPUT_FILE, 0700 | S_IFCHR, 0x0103)) {
				perror("Character device creation failed");
				return 77;
			}
#else
			fprintf(stderr, "Character devices not supported\n");
			return 77;
#endif
			break;
		default:
			fprintf(stderr, "Invalid arg: [%s]\n", code);
			return 3;
	}

	ret = ai_cp_a(INPUT_FILE, OUTPUT_FILE);
	if (ret) {
		fprintf(stderr, "[%s] Copying failed: %s\n",
				code, strerror(errno));
		return 1;
	}

	return compare_files(INPUT_FILE, OUTPUT_FILE, code);
}
