#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <linux/types.h>
#include "internal.h"
#include "i2c.h"
#include "sffcmis.h"

void
i2c_init(struct cmd_context *ctx)
{
	char *argv = getenv("LIBSFFCMIS_ARG");
	char *savetok = NULL;

	if (argv == NULL)
		return;

	for (char *argp = strtok_r(argv, " ", &savetok);
	     argp != NULL;
	     argp  = strtok_r(NULL,  " ", &savetok)) {
		if (argp && !strcmp(argp, "--debug")) {
			char *eptr;

			if ((argp = strtok_r(argp, " ", &savetok)) == NULL) {
				printf("missing argument --debug N");
				continue;
			}
			argp = strtok_r(argp, " ", &savetok);
			if ((argp == NULL) || *eptr) {
				printf("bad argument --debug N");
				continue;
			}
			ctx->debug = strtoul(argp, &eptr, 0);
			continue;
		}
		if (argp && !strcmp(argp, "--busnum")) {
			char *eptr;

			if ((argp = strtok_r(argp, " ", &savetok)) == NULL) {
				printf("missing argument --busnum N");
				continue;
			}
			argp = strtok_r(argp, " ", &savetok);
			if ((argp == NULL) || *eptr) {
				printf("bad argument --busnum N");
				continue;
			}
			ctx->bus_num = strtoul(argp, &eptr, 0);
			continue;
		}
		if (argp && (!strcmp(argp, "--json") ||
			     !strcmp(argp, "-j"))) {
			ctx->json = true;
			continue;
		}
		if (argp && (!strcmp(argp, "--include-statistics") ||
			     !strcmp(argp, "-I"))) {
			ctx->show_stats = true;
			continue;
		}
	}
}

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

#define  MODULE_ID_OFFSET				0x00
#define  MODULE_ID_UNKNOWN				0x00
#define  MODULE_ID_GBIC				0x01
#define  MODULE_ID_SOLDERED_MODULE		0x02
#define  MODULE_ID_SFP					0x03
#define  MODULE_ID_300_PIN_XBI			0x04
#define  MODULE_ID_XENPAK				0x05
#define  MODULE_ID_XFP					0x06
#define  MODULE_ID_XFF					0x07
#define  MODULE_ID_XFP_E				0x08
#define  MODULE_ID_XPAK				0x09
#define  MODULE_ID_X2					0x0A
#define  MODULE_ID_DWDM_SFP			0x0B
#define  MODULE_ID_QSFP				0x0C
#define  MODULE_ID_QSFP_PLUS			0x0D
#define  MODULE_ID_CXP					0x0E
#define  MODULE_ID_HD4X				0x0F
#define  MODULE_ID_HD8X				0x10
#define  MODULE_ID_QSFP28				0x11
#define  MODULE_ID_CXP2				0x12
#define  MODULE_ID_CDFP				0x13
#define  MODULE_ID_HD4X_FANOUT			0x14
#define  MODULE_ID_HD8X_FANOUT			0x15
#define  MODULE_ID_CDFP_S3				0x16
#define  MODULE_ID_MICRO_QSFP			0x17
#define  MODULE_ID_QSFP_DD				0x18
#define  MODULE_ID_OSFP				0x19
#define  MODULE_ID_DSFP				0x1B
#define  MODULE_ID_QSFP_PLUS_CMIS			0x1E
#define  MODULE_ID_SFP_DD_CMIS				0x1F
#define  MODULE_ID_SFP_PLUS_CMIS			0x20
#define  MODULE_ID_LAST				MODULE_ID_SFP_PLUS_CMIS
#define  MODULE_ID_UNALLOCATED_LAST	0x7F
#define  MODULE_ID_VENDOR_START		0x80
#define  MODULE_ID_VENDOR_LAST			0xFF

#define ETH_I2C_ADDRESS_LOW     0x50

int eeprom_parse(struct cmd_context *ctx)
{
	struct module_eeprom request = {
		.length = 1,
		.i2c_address = ETH_I2C_ADDRESS_LOW,
	};
	int ret;

	/* Fetch the SFF-8024 Identifier Value. For all supported standards, it
	 * is located at I2C address 0x50, byte 0. See section 4.1 in SFF-8024,
	 * revision 4.9.
	 */
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	switch (request.data[0]) {
	case MODULE_ID_GBIC:
	case MODULE_ID_SOLDERED_MODULE:
	case MODULE_ID_SFP:
		return sff8079_show_all_nl(ctx);
	case MODULE_ID_QSFP:
	case MODULE_ID_QSFP28:
	case MODULE_ID_QSFP_PLUS:
		return sff8636_show_all_nl(ctx);
	case MODULE_ID_QSFP_DD:
	case MODULE_ID_OSFP:
	case MODULE_ID_DSFP:
	case MODULE_ID_QSFP_PLUS_CMIS:
	case MODULE_ID_SFP_DD_CMIS:
	case MODULE_ID_SFP_PLUS_CMIS:
		return cmis_show_all_nl(ctx);
	default:
		printf("Warning %s:%d: unsupported module 0x%x\n", __func__, __LINE__, request.data[0]);
		return 0;
	}
}
