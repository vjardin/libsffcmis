#ifndef SFFCMIS_H
#define SFFCMIS_H

#include "i2c.h"

struct cmd_context {
	int bus_num;
	I2CDevice device;
	unsigned long debug;	/* debugging mask */
	bool json;		/* Output JSON, if supported */
	bool show_stats;	/* include command-specific stats */
};

void i2c_init(struct cmd_context *ctx);
int get_eeprom_page(struct cmd_context *ctx, struct module_eeprom *request);
int eeprom_parse(struct cmd_context *ctx);

int sff8079_show_all_nl(struct cmd_context *ctx);
int sff8636_show_all_nl(struct cmd_context *ctx);
int cmis_show_all_nl(struct cmd_context *ctx);

#endif /* SFFCMIS_H */
