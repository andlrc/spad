#include "spad.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define	INTERFACE	0
#define	TIMEOUT		1000

#define	SPAD_MSB(n)	((n & 0xFF00) >> 8)
#define	SPAD_LSB(n)	(n & 0xFF)

static const char *spad_errors[] = {
	/* Unknown error */	"Unknown errnum",							
	/* ESPAD_NOTRANS */	"No Transponder is located within the detection range of the Reader",	
	/* ESPAD_CRC */		"CRC16 data error at received data.",					
	/* ESPAD_WRITE */	"Attempt to write on a read-only storing-area.",			
	/* ESPAD_ADDR */	"The address is beyond the max. address space of the Transponder.",	
	/* ESPAD_WRTRANS */	"A special command is not applicable to the Transponder.",		
	/* ESPAD_LENGTH */	"Protocol is too short or too long",					
	/* ESPAD_INVCTL */	"Invalid control byte"							
};

const char *spad_strerror(int errnum)
{
	if (errnum & ESPAD_LIBUSB)
		return libusb_strerror(~(errnum ^ ESPAD_LIBUSB));

	if (errnum == ESPAD_SYSTEM)
		return strerror(errno);

	switch (errnum) {
	case ESPAD_NOTRANS:
		return spad_errors[1];
	case ESPAD_CRC:
		return spad_errors[2];
	case ESPAD_WRITE:
		return spad_errors[3];
	case ESPAD_ADDR:
		return spad_errors[4];
	case ESPAD_WRTRANS:
		return spad_errors[5];
	case ESPAD_LENGTH:
		return spad_errors[6];
	case ESPAD_INVCTL:
		return spad_errors[7];
	default:
		return spad_errors[0];
	}
}

int spad_init(struct spad_context *ctx)
{
	int rc;
	int debuglvl = LIBUSB_LOG_LEVEL_ERROR;
	char *debug;
	if ((rc = libusb_init(&ctx->usb_context)))
		return ESPAD_LIBUSB | ~rc;

	debug = getenv("SPAD_DEBUG");
	if (debug && *debug) {
		debuglvl = LIBUSB_LOG_LEVEL_DEBUG;
	}
	libusb_set_debug(ctx->usb_context, debuglvl);

	ctx->dev_handle =
	    libusb_open_device_with_vid_pid(ctx->usb_context,
					    ctx->vendor_id,
					    ctx->product_id);
	if (!ctx->dev_handle)
		return ESPAD_LIBUSB | ~LIBUSB_ERROR_NO_DEVICE;

	if (libusb_kernel_driver_active(ctx->dev_handle, INTERFACE))
		if ((rc = libusb_detach_kernel_driver(ctx->dev_handle,
						      INTERFACE)))
			return ESPAD_LIBUSB | ~rc;

	if ((rc = libusb_claim_interface(ctx->dev_handle, INTERFACE)))
		return ESPAD_LIBUSB | ~rc;

	return SPAD_SUCCESS;
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
		return ESPAD_SYSTEM;

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

	if (rc)
		return ESPAD_LIBUSB | ~rc;
	if (written != bufsiz)
		return ESPAD_LENGTH;

	return SPAD_SUCCESS;
}

int spad_read(struct spad_context *ctx, unsigned char *resbuf, int ressiz,
	      int *written, int timeout)
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
	 * 5..n-2:      { CONTROL-BYTE STATUS DATA }
	 * n-1:         LSB CRC16
	 * n:           MSB CRC16
	 */
	bufsiz = ressiz + 6;
	if (!(buf = malloc(bufsiz)))
		return ESPAD_SYSTEM;

	rc = libusb_bulk_transfer(ctx->dev_handle,
				  0x81 | LIBUSB_ENDPOINT_IN,
				  buf, bufsiz, &received, timeout);

	if (rc) {
		free(buf);
		return ESPAD_LIBUSB | ~rc;
	}

	/* Validate Checksum */
	crc = spad_crc(buf, received - 2);
	if (buf[received - 2] != SPAD_LSB(crc) ||
	    buf[received - 1] != SPAD_MSB(crc)) {
		free(buf);
		return ESPAD_CRC;
	}

	/* Validate Length */
	if (buf[1] != SPAD_MSB(received) || buf[2] != SPAD_LSB(received)) {
		free(buf);
		return ESPAD_LENGTH;
	}

	/* Check status */
	if (buf[5] != 0x00) {
		rc = ESPAD_SCAN | buf[5];
		free(buf);
		return rc;
	}

	memcpy(resbuf, buf + 4, received - 6);
	free(buf);
	*written = received - 6;
	return SPAD_SUCCESS;
}

int spad_inventory(struct spad_context *ctx, spad_inventory_callback cb)
{
	unsigned char reqbuf[] = { 0xB0, 0x01, 0x00 };
	static unsigned char resbuf[0xFFFF];
	int rc;
	int reslen;
	int tagcnt;
	int i;
	unsigned char *tag;

	if ((rc = spad_write(ctx, reqbuf, sizeof(reqbuf), TIMEOUT)))
		return rc;

	if ((rc = spad_read(ctx, resbuf, sizeof(resbuf), &reslen, TIMEOUT)))
		return rc;

	if (resbuf[0] != 0xB0)	/* CONTROL-BYTE */
		return ESPAD_INVCTL;

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
		cb(ctx, tag, tag + 2);
	}

	return SPAD_SUCCESS;
}

int spad_exit(struct spad_context *ctx)
{
	int rc;
	if ((rc = libusb_release_interface(ctx->dev_handle, INTERFACE)))
		return ESPAD_LIBUSB | ~rc;

	libusb_close(ctx->dev_handle);
	libusb_exit(ctx->usb_context);

	return SPAD_SUCCESS;
}
