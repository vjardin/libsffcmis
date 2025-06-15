#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sffcmis.h"

int
main(int argc, char *argv[]) {
	struct cmd_context ctx;
	char desc[BUFSIZ];

	if (argc != 2) {
		printf("Missing argument: i2c bus id\n");
		return EXIT_SUCCESS;
	}

	ctx = (struct cmd_context){
		.bus_num = atoi(argv[1]),
	};
	i2c_init(&ctx);
	printf("i2c_get_device_desc: %s\n", i2c_get_device_desc(ctx.device, desc, sizeof(desc)));
	int ret = eeprom_parse(&ctx);

	if (ret)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
