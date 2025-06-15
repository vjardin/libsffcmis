#ifndef SFFCMIS_H
#define SFFCMIS_H

//#include "i2c.h"

/* Struct for managing module EEPROM pages */
struct module_eeprom {
	u_int32_t	offset;
	u_int32_t	length;
	u_int8_t	page;
	u_int8_t	bank;
	u_int8_t	i2c_address;
	u_int8_t	*data;
};

/* forward declartion, but keep the details private */
struct i2c_device;
typedef struct i2c_device I2CDevice;

struct cmd_context {
	I2CDevice *device;
	int bus_num;
	unsigned long debug;	/* debugging mask */
	bool json;		/* Output JSON, if supported */
	bool show_stats;	/* include command-specific stats */
};

int i2c_init(struct cmd_context *ctx);
int get_eeprom_page(struct cmd_context *ctx, struct module_eeprom *request);
int eeprom_parse(struct cmd_context *ctx);

int sff8079_show_all_nl(struct cmd_context *ctx);
int sff8636_show_all_nl(struct cmd_context *ctx);
int cmis_show_all_nl(struct cmd_context *ctx);

#endif /* SFFCMIS_H */
