#include "spad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	TAG_SIZE	8
static struct {
	unsigned char *tags;
	int size;
	int length;
} seentags;

void inv_cb(unsigned char type[2], unsigned char tag[8])
{
	unsigned char *seentmp;
	int i;

	if (seentags.length == seentags.size) {
		seentags.size *= 2;
		seentmp = realloc(seentags.tags, TAG_SIZE * seentags.size);
		if (!seentmp) {
			perror("realloc");
			exit(1);
		}
	}

	for (i = 0; i < seentags.length; i++) {
		/* Found tag, bail */
		if (memcmp(seentags.tags + i * TAG_SIZE, tag, TAG_SIZE) == 0)
			return;
	}

	memcpy(seentags.tags + seentags.length * TAG_SIZE, tag, TAG_SIZE);
	seentags.length++;
	type = type;
	spad_dumphex(tag, 8);
}

int main(void)
{
	struct spad_context ctx;
	if (spad_init(&ctx) == -1) {
		fprintf(stderr, "Failed to initialize device\n");
		return 1;
	}

	seentags.size = 16;
	seentags.length = 0;
	seentags.tags = malloc(TAG_SIZE * seentags.size);
	if (!seentags.tags) {
		perror("malloc");
		return 1;
	}

	for (;;)
		spad_inventory(&ctx, inv_cb);
	spad_exit(&ctx);

	return 0;
}