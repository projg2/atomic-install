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
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#define AI_JOURNAL_MAGIC "AIj!"

#pragma pack(push)
#pragma pack(1)

struct ai_journal {
	char magic[sizeof(AI_JOURNAL_MAGIC)]; /* AIj!\0 */
	uint16_t version; /* 0x0000 */
	uint32_t flags; /* what has been done */

	uint32_t prefix_bits; /* bits used to create filename prefix */

	uint64_t length; /* file length */
	uint64_t maxpathlen; /* max filename length */

	char files[]; /* list of null-terminated path+filename pairs */
	/* (terminated by "") */
};

#pragma pack(pop)

static int ai_traverse_tree(const char *root, const char *path, FILE *outf, int is_dir, uint64_t *filelen, uint64_t *maxpathlen) {
	char *fn;
	DIR *dir;
	struct dirent *dent;
	int ret;

	fn = malloc(strlen(root) + strlen(path) + 1);
	if (!fn)
		return errno;
	sprintf(fn, "%s%s", root, path);

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
			ret = ai_traverse_tree(root, fn, outf, is_dir == 1, filelen, maxpathlen);

			if (ret == ENOTDIR)
				is_dir = 0;
		}

		if (!is_dir) {
			const char *fnpart = strrchr(fn, '/') + 1;
			const size_t pathlen = fnpart - fn;

			if (fwrite(fn, pathlen, 1, outf) != 1
					|| fputc(0, outf) == EOF
					|| fwrite(fnpart, len - pathlen, 1, outf) != 1)
				ret = errno;
			*filelen += len + 1;

			if (*maxpathlen < len)
				*maxpathlen = len;
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
	struct ai_journal newj = { AI_JOURNAL_MAGIC, 0x0000, 0, 0 };

	FILE *f;
	int ret;
#ifdef HAVE_LOCKF
	int fd;
#endif
	uint64_t len = sizeof(newj) + 1;
	uint64_t maxpathlen = 0;

	f = fopen(journal_path, "wb");
	if (!f)
		return errno;

#ifdef HAVE_LOCKF
	fd = fileno(f);
	lockf(fd, F_LOCK, 0);
#endif

	srandom(time(NULL));
	newj.prefix_bits = random();

	if (fwrite(&newj, sizeof(newj), 1, f) < 1) {
		fclose(f);
		return errno;
	}

	ret = ai_traverse_tree(location, "", f, 1, &len, &maxpathlen);

	if (!ret) {
		/* Null-terminate the list. */
		if (fputc(0, f) == EOF)
			ret = errno;
		else {
			newj.length = len;
			newj.maxpathlen = maxpathlen - 1;

			rewind(f);
			if (fwrite(&newj, sizeof(newj), 1, f) < 1)
				ret = errno;
		}
	}

#ifdef HAVE_LOCKF
	lockf(fd, F_ULOCK, 0);
#endif
	if (fclose(f) && !ret)
		ret = errno;

	return ret;
}

int ai_journal_open(const char *journal_path, ai_journal_t *ret) {
	int fd;
	struct stat st;
	int retval = 0;

	fd = open(journal_path, O_RDWR);
	if (fd == -1)
		return errno;

	lockf(fd, F_LOCK, 0);
	do {
		if (fstat(fd, &st)) {
			retval = errno;
			break;
		}

		if (st.st_size < sizeof(struct ai_journal)) {
			retval = EINVAL;
			break;
		}

		*ret = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (!ret) {
			retval = errno;
			break;
		}

		if (memcmp((*ret)->magic, AI_JOURNAL_MAGIC, sizeof(AI_JOURNAL_MAGIC))
				|| (*ret)->version != 0
				|| (*ret)->length != st.st_size) {

			munmap(ret, st.st_size);
			retval = EINVAL;
		}
	} while (0);

	close(fd);
	return retval;
}

int ai_journal_close(ai_journal_t j) {
	if (munmap(j, j->length))
		return errno;

	return 0;
}

ai_journal_file_t *ai_journal_get_files(ai_journal_t j) {
	return j->files;
}

int ai_journal_get_maxpathlen(ai_journal_t j) {
	return j->maxpathlen;
}

const char *ai_journal_get_filename_prefix(ai_journal_t j) {
	uint32_t prefix_bits = j->prefix_bits;
	static char prefix_buf[7] = {0, 0, 0, 0, 0, 0, 0};

	int i;

	for (i = 0; i < 6; i++) {
		prefix_buf[i] = 'a' + prefix_bits % 26;
		prefix_bits >>= 5;
	}

	return prefix_buf;
}

const char *ai_journal_file_path(ai_journal_file_t *f) {
	return f;
}

const char *ai_journal_file_name(ai_journal_file_t *f) {
	return f + strlen(f) + 1;
}

ai_journal_file_t *ai_journal_file_next(ai_journal_file_t *f) {
	const char *next = f + strlen(f) + 1;
	next += strlen(next) + 1;

	return *next ? next : NULL;
}

unsigned long int ai_journal_get_flags(ai_journal_t j) {
	return j->flags;
}

int ai_journal_set_flag(ai_journal_t j, unsigned long int new_flag) {
	if (mprotect(j, sizeof(struct ai_journal), PROT_READ|PROT_WRITE))
		return errno;

	j->flags |= new_flag;

	if (msync(j, sizeof(struct ai_journal), MS_SYNC))
		return errno;
	mprotect(j, sizeof(struct ai_journal), PROT_READ);

	return 0;
}
