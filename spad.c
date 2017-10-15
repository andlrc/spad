#include "spad.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define	VENDOR_ID	0x0AB1
#define	PRODUCT_ID	0x0002
#define	INTERFACE	0
#define	TIMEOUT		1000

#define	SPAD_MSB(n)	((n & 0xFF00) >> 8)
#define	SPAD_LSB(n)	(n & 0xFF)

void spad_dumphex(void *buf, int siz)
{
	int i;
	if (siz > 0) {
		for (i = 0; i < siz; i++)
			printf("%02x ", ((unsigned char *) buf)[i]);
		printf("\n");
	} else {
		printf("(empty)\n");
	}
}

int spad_inventory(struct spad_context *ctx, spad_inventory_callback cb)
{
	unsigned char reqbuf[] = { 0xB0, 0x01, 0x00 };
	static unsigned char resbuf[0xFFFF];
	int reslen;
	int tagcnt;
	int i;
	unsigned char *tag;

	if (spad_write(ctx, reqbuf, sizeof(reqbuf), TIMEOUT) == -1)
		return -1;

	if ((reslen =
	     spad_read(ctx, resbuf, sizeof(resbuf), TIMEOUT)) == -1)
		return -1;

	if (resbuf[0] != 0xB0) {	/* CONTROL-BYTE */
		return -1;
	}

	if (resbuf[1] != 0x00) {	/* STATUS, could be 0x94 if more tags are able to be read */
		return -1;
	}

	/* Tags is the following:
	 * 1:           CONTROL-BYTE
	 * 2:           STATUS
	 * 3:           TAG COUNT
	 * [
	 *   1-2:       TAG TYPE
	 *   3-10:      TAG VALUE
	 * ] 4..n
	 */
	tagcnt = resbuf[2];

	for (i = 0; i < tagcnt; i++) {
		tag = resbuf + 3 + 10 * i;
		cb(tag, tag + 2);
	}

	return 0;
}

int spad_init(struct spad_context *ctx)
{
	int rc;
	int debuglvl = LIBUSB_LOG_LEVEL_ERROR;
	char *debug;
	if ((rc = libusb_init(&ctx->usb_context)) != 0)
		return -1;

	debug = getenv("SPAD_DEBUG");
	if (debug && *debug) {
		debuglvl = LIBUSB_LOG_LEVEL_DEBUG;
	}
	libusb_set_debug(ctx->usb_context, debuglvl);

	ctx->dev_handle =
	    libusb_open_device_with_vid_pid(ctx->usb_context, VENDOR_ID,
					    PRODUCT_ID);
	if (!ctx->dev_handle)
		return -1;

	if (libusb_kernel_driver_active(ctx->dev_handle, INTERFACE))
		if ((rc = libusb_detach_kernel_driver(ctx->dev_handle,
						      INTERFACE)) != 0)
			return -1;

	if ((rc = libusb_claim_interface(ctx->dev_handle, INTERFACE)))
		return -1;

	return 0;
}

unsigned int spad_crc(unsigned char *buf, int bufsiz)
{
#define	CRC_PRESET	0xFFFF
#define CRC_POLYNOM	0x8408

	/* Compute checksum */
	unsigned int crc;
	int i, j;
	crc = CRC_PRESET;
	for (i = 0; i < bufsiz; i++) {
		crc ^= buf[i];
		for (j = 0; j < 8; j++) {
			if (crc & 0x0001)
				crc = (crc >> 1) ^ CRC_POLYNOM;
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

int spad_write(struct spad_context *ctx, unsigned char *reqbuf, int reqsiz,
	       int timeout)
{
	unsigned char *buf;
	int bufsiz;
	unsigned int crc;
	int rc;
	int written;

	/* Request is the following:
	 * 1:           STX
	 * 2:           MSB ALENGTH
	 * 3:           LSB ALENGTH
	 * 4:           COM-ADR
	 * 5..n-2:      { CONTROL-BYTE DATA }
	 * n-1:         LSB CRC16
	 * n:           MSB CRC16
	 */
	bufsiz = reqsiz + 6;
	if (!(buf = malloc(bufsiz)))
		return -1;

	buf[0] = 0x02;
	buf[1] = SPAD_MSB(bufsiz);
	buf[2] = SPAD_LSB(bufsiz);
	buf[3] = 0xFF;
	memcpy(buf + 4, reqbuf, reqsiz);

	crc = spad_crc(buf, bufsiz - 2);

	buf[bufsiz - 2] = SPAD_LSB(crc);
	buf[bufsiz - 1] = SPAD_MSB(crc);

	rc = libusb_bulk_transfer(ctx->dev_handle,
				  0x02 | LIBUSB_ENDPOINT_OUT,
				  buf, bufsiz, &written, timeout);
	free(buf);

	if (rc != LIBUSB_SUCCESS || written != bufsiz)
		return -1;

	return 0;
}

int spad_read(struct spad_context *ctx, unsigned char *resbuf, int ressiz,
	      int timeout)
{
	unsigned char *buf;
	int bufsiz;
	int rc;
	int received;
	unsigned int crc;

	/* Response is the following:
	 * 1:           STX
	 * 2:           MSB ALENGTH
	 * 3:           LSB ALENGTH
	 * 4:           COM-ADR
	 * 5..n-2:      { CONTROL-BYTE STATSUS DATA }
	 * n-1:         LSB CRC16
	 * n:           MSB CRC16
	 */
	bufsiz = ressiz + 6;
	if (!(buf = malloc(bufsiz)))
		return -1;

	rc = libusb_bulk_transfer(ctx->dev_handle,
				  0x81 | LIBUSB_ENDPOINT_IN,
				  buf, bufsiz, &received, timeout);

	if (rc != LIBUSB_SUCCESS) {
		free(buf);
		return -1;
	}

	/* Validate Checksum */
	crc = spad_crc(buf, received - 2);
	if (buf[received - 2] != SPAD_LSB(crc) ||
	    buf[received - 1] != SPAD_MSB(crc)) {
		free(buf);
		return -1;
	}

	/* Validate Length */
	if (buf[1] != SPAD_MSB(received) || buf[2] != SPAD_LSB(received)) {
		free(buf);
		return -1;
	}

	memcpy(resbuf, buf + 4, received - 6);
	free(buf);
	return received - 6;
}

int spad_exit(struct spad_context *ctx)
{
	int rc;
	if ((rc = libusb_release_interface(ctx->dev_handle, INTERFACE)))
		return -1;

	libusb_close(ctx->dev_handle);
	libusb_exit(ctx->usb_context);

	return 0;
}
