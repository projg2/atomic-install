/* atomic-install -- journal support
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

#ifdef HAVE_FLOCK
#	include <sys/file.h>
#endif

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

/**
 * AI_JOURNAL_MAGIC
 *
 * Magic used to identify the journal.
 */
#define AI_JOURNAL_MAGIC "AIj!"
/**
 * AI_JOURNAL_EOF
 *
 * Special filename bits used to identify end of filelist.
 */
static const unsigned char AI_JOURNAL_EOF = 0xff;

#pragma pack(push)
#pragma pack(1)

/**
 * ai_journal
 * @magic: %AI_JOURNAL_MAGIC
 * @version: journal format version, 0x0000 right now
 * @flags: global journal 32-bit flags field, for use by caller
 * @prefix: random prefix for temporary files associated with journal
 * @length: exact journal file length, in bytes
 * @maxpathlen: max length of path+filename in journal
 * @files: array of (flag + path + \0 + filename + \0), terminated
 *	by %AI_JOURNAL_EOF (on flag field)
 *
 * The journal format.
 */
struct ai_journal {
	char magic[sizeof(AI_JOURNAL_MAGIC)];
	uint16_t version;
	uint32_t flags;

	char prefix[7];

	uint64_t length;
	uint64_t maxpathlen;

	unsigned char files[];
};

#pragma pack(pop)

/**
 * ai_traverse_tree
 * @root: source tree root
 * @path: current subdirectory in source tree
 * @outf: open journal file
 * @is_dir: 1 if @path is certainly a directory, 0 otherwise
 * @filelen: location to update journal file length in
 * @maxpathlen: location to update max filename length in
 *
 * Traverse recursively a subtree of source tree @root/@path. If it's
 * a directory, recurse into it; otherwise, write it to the journal file @outf
 * and update @filelen and @maxpathlen as necessary.
 *
 * If @is_dir is passed zero, this function lstats the path to check whether
 * it's a directory (to avoid opening symlinks to directories); otherwise, it
 * just assumes caller has ensured it is a directory.
 *
 * Returns: 0 on success, ENOTDIR if called recursively on a non-directory,
 *	errno otherwise
 */
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

			if (fputc(0, outf) == EOF /* flags */
					|| fwrite(fn, pathlen, 1, outf) != 1 /* path */
					|| fputc(0, outf) == EOF /* sep */
					|| fwrite(fnpart, len - pathlen, 1, outf) != 1) /* fn */
				ret = errno;
			else
				ret = 0;
			*filelen += len + 2;

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

/**
 * ai_journal_set_filename_prefix
 * @out: output buffer, at least 7 bytes long
 * @bits: input bitdata for the prefix
 *
 * Create the prefix used for temporary files associated with the journal from
 * @bits and write it to @out. @out must be at least 7 bytes long.
 */
static void ai_journal_set_filename_prefix(char *out, long int bits) {
	int i;

	for (i = 0; i < 6; i++) {
		out[i] = 'a' + bits % 26;
		bits >>= 5;
	}

	/* Null-terminate. */
	out[i] = 0;
}

int ai_journal_create(const char *journal_path, const char *location) {
	struct ai_journal newj = { AI_JOURNAL_MAGIC, 0x0000, 0 };

	FILE *f;
	int ret;
#ifdef HAVE_FLOCK
	int fd;
#endif

	f = fopen(journal_path, "wb");
	if (!f)
		return errno;

#ifdef HAVE_FLOCK
	fd = fileno(f);
	flock(fd, LOCK_EX);
#endif

	newj.length = sizeof(newj) + 1;
	newj.maxpathlen = 0;

	srandom(time(NULL));
	ai_journal_set_filename_prefix(newj.prefix, random());

	if (fwrite(&newj, sizeof(newj), 1, f) < 1) {
		fclose(f);
		return errno;
	}

	ret = ai_traverse_tree(location, "", f, 1,
			&newj.length, &newj.maxpathlen);

	if (!ret) {
		/* Terminate the list. */
		if (fputc(AI_JOURNAL_EOF, f) == EOF)
			ret = errno;
		else {
			rewind(f);
			if (fwrite(&newj, sizeof(newj), 1, f) < 1)
				ret = errno;
		}
	}

#ifdef HAVE_FLOCK
	flock(fd, LOCK_UN);
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

#ifdef HAVE_FLOCK
	flock(fd, LOCK_EX);
#endif
	do {
		if (fstat(fd, &st)) {
			retval = errno;
			break;
		}

		if (st.st_size < sizeof(struct ai_journal)) {
			retval = EINVAL;
			break;
		}

		*ret = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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
	return *(j->files) != AI_JOURNAL_EOF ? j->files : NULL;
}

int ai_journal_get_maxpathlen(ai_journal_t j) {
	return j->maxpathlen;
}

const char *ai_journal_get_filename_prefix(ai_journal_t j) {
	return j->prefix;
}

unsigned char ai_journal_file_flags(ai_journal_file_t *f) {
	return *f;
}

int ai_journal_file_set_flag(ai_journal_file_t *f, unsigned char new_flag) {
	*f |= new_flag;

	return 0;
}

const char *ai_journal_file_path(ai_journal_file_t *f) {
	return (const char*) f + 1;
}

const char *ai_journal_file_name(ai_journal_file_t *f) {
	const char *path = ai_journal_file_path(f);

	return path + strlen(path) + 1;
}

ai_journal_file_t *ai_journal_file_next(ai_journal_file_t *f) {
	const char *fn = ai_journal_file_name(f);
	unsigned char *next = (unsigned char*) fn + strlen(fn) + 1;

	return *next != AI_JOURNAL_EOF ? next : NULL;
}

unsigned long int ai_journal_get_flags(ai_journal_t j) {
	return j->flags;
}

int ai_journal_set_flag(ai_journal_t j, unsigned long int new_flag) {
#ifdef HAVE_SYNC
	sync();
#endif

	j->flags |= new_flag;

	/* sync whole to update file flags as well */
	if (msync(j, j->length, MS_SYNC))
		return errno;

	return 0;
}
