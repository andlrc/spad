#include "spad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PROGRAM_NAME	"spad"
#define	PROGRAM_USAGE	"Usage: " PROGRAM_NAME " <vendor:product>...\n"
#define	TAG_SIZE	8
static struct {
	unsigned char *tags;
	int size;
	int length;
} seentags;

static void print_help(void)
{
	printf(PROGRAM_USAGE "\n"
	       "  -1    use the device '0ab1:0002'\n"
	       "  -2    use the device '0ab1:0004'\n"
	       "\n"
	       "  -h    show this help and exit\n");
}

static void inv_cb(unsigned char type[2], unsigned char tag[8])
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
	/* type = byte byte, tag[0], tag[1], tag[2], tag[3], tag[4], tag[5],
	 * tag[6], tag[7] */
	spad_dumphex(type, 10);
}

int main(int argc, char **argv)
{
	struct spad_context ctxs[8];
	int ctxcnt;
	struct spad_context ctx;
	int rc;
	int c;
	int i;
	char *endp;

	memset(ctxs, 0, sizeof(ctxs));
	ctxcnt = 0;

	while ((c = getopt(argc, argv, "h12")) != -1) {
		switch (c) {
		case 'h':
			print_help();
			return 0;
		case '1':
			ctxs[ctxcnt].vendor_id = 0x0AB1;
			ctxs[ctxcnt].product_id = 0x0002;
			ctxcnt++;
			break;
		case '2':
			ctxs[ctxcnt].vendor_id = 0x0AB1;
			ctxs[ctxcnt].product_id = 0x0004;
			ctxcnt++;
			break;
		default:
			fprintf(stderr, PROGRAM_USAGE);
			return 1;
		}
	}

	for (i = optind; i < argc; i++) {
		ctxs[ctxcnt].vendor_id = strtol(argv[i], &endp, 16);
		ctxs[ctxcnt].product_id = strtol(endp + 1, 0, 16);
		ctxcnt++;
	}

	if (!ctxcnt) {
		fprintf(stderr, PROGRAM_USAGE);
		return 1;
	}

	for (i = 0; i < ctxcnt; i++) {
		ctx = ctxs[i];
		if ((rc = spad_init(&ctx))) {
			fprintf(stderr, "%s\n", spad_strerror(rc));
			return 1;
		}
	}

	seentags.size = 16;
	seentags.length = 0;
	seentags.tags = malloc(TAG_SIZE * seentags.size);
	if (!seentags.tags) {
		perror("malloc");
		return 1;
	}

	for (;;) {
		for (i = 0; i < ctxcnt; i++) {
			ctx = ctxs[i];
			if ((rc = spad_inventory(&ctx, inv_cb)))
				fprintf(stderr, "%d, %s\n", rc, spad_strerror(rc));
			sleep(1);
		}
	}

	for (i = 0; i < ctxcnt; i++) {
		spad_exit(&ctx);
	}

	return 0;
}
