/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "journal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#pragma pack(push)
#pragma pack(1)

struct ai_journal {
	char magic[4]; /* AIj! */
	uint16_t version; /* 0x0000 */
	uint32_t flags; /* unused right now */
	uint8_t stage; /* 0.. */

	uint64_t length; /* file length */

	char files[]; /* list of null-terminated paths */
};

#pragma pack(pop)

static int ai_traverse_tree(const char *root, const char *path, FILE *outf, int is_dir, uint64_t *filelen) {
	char *fn;
	DIR *dir;
	struct dirent *dent;
	int ret;

	fn = malloc(strlen(root) + strlen(path) + 2);
	if (!fn)
		return errno;
	sprintf(fn, "%s/%s", root, path);

	/* We need to check whether it's a directory */
	if (!is_dir) {
		struct stat st;

		if (lstat(fn, &st)) {
			free(fn);
			return errno;
		}

		if (!S_ISDIR(st.st_mode)) {
			free(fn);
			return ENOTDIR;
		}
	}

	dir = opendir(fn);
	free(fn);
	if (!dir)
		return errno;

	errno = 0;
	while ((dent = readdir(dir))) {
		int len, ret;
		int is_dir = -1;

		/* Omit . & .. */
		if (dent->d_name[0] == '.' && (!dent->d_name[1]
					|| (dent->d_name[1] == '.' && !dent->d_name[2])))
			continue;

#ifdef DT_DIR
		/* It's awesome if we can avoid falling back to stat() */
		if (dent->d_type == DT_DIR)
			is_dir = 1;
		else if (dent->d_type != DT_UNKNOWN)
			is_dir = 0;
#endif

		/* Prepare the relative path */
		len = strlen(path) + strlen(dent->d_name) + 2;
		fn = malloc(len);
		if (!fn)
			break;
		sprintf(fn, "%s/%s", path, dent->d_name);

		if (is_dir != 0) {
			ret = ai_traverse_tree(root, fn, outf, is_dir == 1, filelen);

			if (ret == ENOTDIR)
				is_dir = 0;
		}

		if (!is_dir) {
			if (fwrite(fn, len, 1, outf) != 1)
				ret = errno;
			*filelen += len;
		}

		free(fn);
		if (ret) {
			closedir(dir);
			return ret;
		}

		errno = 0;
	}
	ret = errno;

	if (closedir(dir) && !ret)
		ret = errno;
	return ret;
}

int ai_journal_create(const char *journal_path, const char *location) {
	struct ai_journal newj = { "AIj!", 0x0000, 0, 0 };

	FILE *f;
	int ret;
#ifdef HAVE_LOCKF
	int fd;
#endif
	uint64_t len = sizeof(newj);

	f = fopen(journal_path, "wb");
	if (!f)
		return errno;

#ifdef HAVE_LOCKF
	fd = fileno(f);
	lockf(fd, F_LOCK, 0);
#endif

	if (fwrite(&newj, sizeof(newj), 1, f) < 1) {
		fclose(f);
		return errno;
	}

	ret = ai_traverse_tree(location, "", f, 1, &len);

	if (!ret) {
		newj.length = len;

		rewind(f);
		if (fwrite(&newj, sizeof(newj), 1, f) < 1)
			ret = errno;
	}

#ifdef HAVE_LOCKF
	lockf(fd, F_ULOCK, 0);
#endif
	if (fclose(f) && !ret)
		ret = errno;

	return ret;
}
