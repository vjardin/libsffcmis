#ifndef SFFCMIS_H
#define SFFCMIS_H

#include "i2c.h"

struct cmd_context {
	int bus_num;
	I2CDevice device;
	bool json;		/* Output JSON, if supported */
};

int get_eeprom_page(struct cmd_context *ctx, struct module_eeprom *request);

#endif /* SFFCMIS_H */
