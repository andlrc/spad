#ifndef SPAD_H
#define SPAD_H 1
#include <libusb-1.0/libusb.h>

struct spad_context {
	struct libusb_context *usb_context;
	struct libusb_device_handle *dev_handle;
};

typedef void (*spad_inventory_callback) (unsigned char tagtype[2],
					 unsigned char tag[8]);
enum spad_errors {
	SPAD_SUCCESS	=  0x00,	/* Data / parameters have been read or stored without error */
	SPAD_ENOTRANS	= -0x01,	/* No Transponder is located within the detection range of the Reader */
	SPAD_ECRC	= -0x02,	/* CRC16 data error at received data. */
	SPAD_EWRITE	= -0x03,	/* Attempt to write on a read-only storing-area. */
	SPAD_EADDR	= -0x04,	/* The address is beyond the max. address space of the Transponder. */
	SPAD_EWRTRANS	= -0x05,	/* A special command is not applicable to the Transponder. */
	SPAD_ELENGTH	= -0x81,	/* Protocol is too short or too long */
	/* Custom */
	SPAD_ELIBUSB	= -0xFD,	/* libusb errors */
	SPAD_EERRNO	= -0xFE,	/* stdlib errors */
	SPAD_EUNKNOWN	= -0xFF		/* other, unclassified errors */
};

const char *spad_strerror(enum spad_errors errno);
void spad_dumphex(void *buf, int siz);
int spad_init(struct spad_context *ctx);
int spad_write(struct spad_context *ctx, unsigned char *reqbuf, int reqsiz,
	       int timeout);
int spad_read(struct spad_context *ctx, unsigned char *resbuf, int ressiz,
	      int timeout);
int spad_inventory(struct spad_context *ctx, spad_inventory_callback cb);
int spad_exit(struct spad_context *ctx);

#endif
