#ifndef INTERNAL_H
#define INTERNAL_H

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define ETH_MODULE_SFF_8079		0x1
#define ETH_MODULE_SFF_8079_LEN		256

#define ETH_MODULE_SFF_8472		0x2
#define ETH_MODULE_SFF_8472_LEN		512

#define ETH_MODULE_SFF_8636		0x3
#define ETH_MODULE_SFF_8636_LEN		256
#define ETH_MODULE_SFF_8636_MAX_LEN     640

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* Struct for managing module EEPROM pages */
struct module_eeprom {
	u32	offset;
	u32	length;
	u8	page;
	u8	bank;
	u8	i2c_address;
	u8	*data;
};

/* Optics diagnostics */
void sff8472_show_all(const __u8 *id);

#endif /* INTERNAL_H */
