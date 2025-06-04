#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sffcmis.h"

int
main(int argc, char *argv[]) {
	struct cmd_context ctx;

	if (argc != 1) {
		printf("Missing argument\n");
		return EXIT_SUCCESS;
	}

	ctx = (struct cmd_context){
		.bus_num = atoi(argv[1]),
	};
	i2c_init(&ctx);
	int ret = eeprom_parse(&ctx);

	if (ret)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
