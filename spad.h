#ifndef SPAD_H
#define SPAD_H 1
#include <libusb-1.0/libusb.h>

struct spad_context {
	int vendor_id;
	int product_id;
	struct libusb_context *usb_context;
	struct libusb_device_handle *dev_handle;
};

typedef void (*spad_inventory_callback) (struct spad_context *ctx,
					 unsigned char tagtype[2],
					 unsigned char tag[8]);

#define	SPAD_SUCCESS	0
/* Scanner errors */
#define ESPAD_SCAN	0x0100
#define	ESPAD_NOTRANS	(ESPAD_SCAN | 0x0001)
#define	ESPAD_CRC	(ESPAD_SCAN | 0x0002)
#define	ESPAD_WRITE	(ESPAD_SCAN | 0x0003)
#define	ESPAD_ADDR	(ESPAD_SCAN | 0x0004)
#define	ESPAD_WRTRANS	(ESPAD_SCAN | 0x0005)
#define	ESPAD_LENGTH	(ESPAD_SCAN | 0x0081)
/* libusb errors */
#define	ESPAD_LIBUSB	0x0200
/* libusb errors range from 0 - -100 OR'ing the bitwise NOT of the libusb error
 * making the final error code, i.e:
 * ESPAD_LIBUSB | (~LIBUSB_ERROR_NO_DEVICE)
 */

/* System Errors, read error in 'errno' */
#define	ESPAD_SYSTEM	0x0400

/* Custom errors */
#define	ESPAD_INVCTL	0x0801	/* Invalid Control Byte */

const char *spad_strerror(int errnum);
int spad_init(struct spad_context *ctx);
int spad_write(struct spad_context *ctx, unsigned char *reqbuf, int reqsiz,
	       int timeout);
int spad_read(struct spad_context *ctx, unsigned char *resbuf, int ressiz,
	      int *written, int timeout);
int spad_inventory(struct spad_context *ctx, spad_inventory_callback cb);
int spad_exit(struct spad_context *ctx);

#endif
