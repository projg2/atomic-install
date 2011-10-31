/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#ifdef HAVE_LOCKF
#	include <unistd.h>
#endif

#pragma pack(push)
#pragma pack(1)

struct ai_journal {
	char magic[4]; /* AIj! */
	uint16_t version; /* 0x0000 */
	uint32_t flags; /* unused right now */
	uint8_t stage; /* 0.. */

	char padding[5];

	char files[]; /* list of null-terminated paths */
};

#pragma pack(pop)

int ai_journal_create(const char *journal_path, const char *location) {
	struct ai_journal newj = { "AIj!", 0x0000, 0, 0 };

	FILE *f;
#ifdef HAVE_LOCKF
	int fd;
#endif

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

	/* XXX */

#ifdef HAVE_LOCKF
	lockf(fd, F_ULOCK, 0);
#endif
	if (fclose(f))
		return errno;

	return 0;
}
