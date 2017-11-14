#include "spad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PROGRAM_NAME	"spad"
#define	PROGRAM_VERSION	"0.5.4"
#define	PROGRAM_USAGE	"Usage: " PROGRAM_NAME				\
			" [OPTION]... <vendor:product>...\n"

#define	TAG_SIZE	8
#define	TAGTYPE_SIZE	2
static struct {
	unsigned char *tags;
	int size;
	int length;
} seentags;

enum {
	OUT_PLAIN,
	OUT_JSON
} output_type;

static int outputtype;

static void print_help(void)
{
	printf(PROGRAM_USAGE "\n"
	       "  -1    use the device '0ab1:0002'\n"
	       "  -2    use the device '0ab1:0004'\n"
	       "  -j    output JSON\n"
	       "\n"
	       "  -h    show this help and exit\n");
}

static void print_version(void)
{
	printf(PROGRAM_NAME " " PROGRAM_VERSION "\n");
}

static void inv_cb(struct spad_context *ctx, unsigned char type[2],
		   unsigned char tag[8])
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
		seentags.tags = seentmp;
	}

	for (i = 0; i < seentags.length; i++) {
		/* Found tag, bail */
		if (memcmp(seentags.tags + i * TAG_SIZE, tag, TAG_SIZE) == 0)
			return;
	}

	memcpy(seentags.tags + seentags.length * TAG_SIZE, tag, TAG_SIZE);
	seentags.length++;
	switch (outputtype) {
	case OUT_PLAIN:
		for (i = 0; i < TAGTYPE_SIZE; i++)
			printf("%02X", type[i]);
		printf(" ");
		for (i = 0; i < TAG_SIZE; i++)
			printf("%02X", tag[i]);
		printf(" %04X:%04X\n", ctx->vendor_id, ctx->product_id);
		break;
	case OUT_JSON:
		printf("{\"type\":\"");
		for (i = 0; i < TAGTYPE_SIZE; i++)
			printf("%02X", type[i]);
		printf("\",\"tag\":\"");
		for (i = 0; i < TAG_SIZE; i++)
			printf("%02X", tag[i]);
		printf("\",\"device\":\"%04X:%04X\"}\n",
		       ctx->vendor_id, ctx->product_id);
	}
	fflush(stdout);
}

int main(int argc, char **argv)
{
	struct spad_context ctxs[8];
	int ctxcnt;
	int rc;
	int c;
	int i;
	char *endp;

	outputtype = OUT_PLAIN;

	memset(ctxs, 0, sizeof(ctxs));
	ctxcnt = 0;

	while ((c = getopt(argc, argv, "hV12j")) != -1) {
		switch (c) {
		case 'h':
			print_help();
			return 0;
		case 'V':
			print_version();
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
		case 'j':
			outputtype = OUT_JSON;
			break;
		default:
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
		if ((rc = spad_init(&ctxs[i]))) {
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
			if ((rc = spad_inventory(&ctxs[i], inv_cb)))
				fprintf(stderr, "%d, %s\n", rc,
					spad_strerror(rc));
			sleep(1);
		}
	}

	for (i = 0; i < ctxcnt; i++) {
		spad_exit(&ctxs[i]);
	}

	return 0;
}
