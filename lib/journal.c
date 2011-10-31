/* libcopy -- journal support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#if HAVE_STDINT_H
#	include <stdint.h>
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
	FILE *f;
	struct ai_journal newj = { "AIj!", 0x0000, 0, 0 };

	f = fopen(journal_path, "wb");
	if (!f)
		return errno;

	if (fwrite(&newj, sizeof(newj), 1, f) < 1) {
		fclose(f);
		return errno;
	}

	/* XXX */

	if (fclose(f))
		return errno;

	return 0;
}
