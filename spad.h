#ifndef SPAD_H
#define SPAD_H 1
#include <libusb-1.0/libusb.h>

struct spad_context {
	struct libusb_context *usb_context;
	struct libusb_device_handle *dev_handle;
};

typedef void (*spad_inventory_callback) (unsigned char tagtype[2],
					 unsigned char tag[8]);

void spad_dumphex(void *buf, int siz);
int spad_init(struct spad_context *ctx);
int spad_inventory(struct spad_context *ctx, spad_inventory_callback cb);
int spad_write(struct spad_context *ctx, unsigned char *reqbuf, int reqsiz,
	       int timeout);
int spad_read(struct spad_context *ctx, unsigned char *resbuf, int ressiz,
	      int timeout);
int spad_exit(struct spad_context *ctx);

#endif
