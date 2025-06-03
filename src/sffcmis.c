#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <linux/types.h>
#include "internal.h"
#include "i2c.h"
#include "sffcmis.h"

int
get_eeprom_page(struct cmd_context *ctx, struct module_eeprom *request)
{
	assert(ctx != NULL);
	assert(request != NULL);
	assert(ctx->bus_num >= 0);

	if (ctx->device.bus < 0) {
		char bus_name[PATH_MAX];

		snprintf(bus_name, sizeof(bus_name), "/dev/i2c-%u", ctx->bus_num);

		int bus;
		if ((bus = i2c_open(bus_name)) == -1) {
			fprintf(stderr, "Open i2c bus:%s error!\n", bus_name);
			return -EIO;
		}

		i2c_init_device(&ctx->device);

		ctx->device.bus = bus;
	}

	ctx->device.addr = request->i2c_address & 0x3ff;

	int ret = i2c_ioctl_read(&ctx->device, request->i2c_address, request->data, request->length);

	return ret;
}
