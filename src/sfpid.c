/****************************************************************************
 * Support for Solarflare Solarstorm network controllers and boards
 * Copyright 2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "module-common.h"
#include "i2c.h"
#include "sffcmis.h"

#define SFF8079_PAGE_SIZE		0x80
#define SFF8079_I2C_ADDRESS_LOW		0x50
#define SFF8079_I2C_ADDRESS_HIGH	0x51

static void sff8079_show_identifier(const __u8 *id)
{
	module_show_identifier(id, 0);
}

static void sff8079_show_ext_identifier(const __u8 *id)
{
	char description[SFF_MAX_DESC_LEN];

	if (id[1] == 0x00)
		sprintf(description, "%s",
			"GBIC not specified / not MOD_DEF compliant");
	else if (id[1] == 0x04)
		sprintf(description, "%s",
			"GBIC/SFP defined by 2-wire interface ID");
	else if (id[1] <= 0x07)
		sprintf(description, "%s %u", "GBIC compliant with MOD_DEF",
			id[1]);
	else
		sprintf(description, "%s", "unknown");

	sff_print_any_hex_field("Extended identifier", "extended_identifier",
				id[1], description);
}

static void sff8079_show_connector(const __u8 *id)
{
	module_show_connector(id, 2);
}

static void sff8079_show_transceiver(const __u8 *id)
{
	static const char *pfx = "Transceiver type";

	if (is_json_context()) {
		open_json_array("transceiver_codes", "");
		print_uint(PRINT_JSON, NULL, "%u", id[3]);
		print_uint(PRINT_JSON, NULL, "%u", id[4]);
		print_uint(PRINT_JSON, NULL, "%u", id[5]);
		print_uint(PRINT_JSON, NULL, "%u", id[6]);
		print_uint(PRINT_JSON, NULL, "%u", id[7]);
		print_uint(PRINT_JSON, NULL, "%u", id[8]);
		print_uint(PRINT_JSON, NULL, "%u", id[9]);
		print_uint(PRINT_JSON, NULL, "%u", id[10]);
		print_uint(PRINT_JSON, NULL, "%u", id[36]);
		close_json_array("");
	} else {
		printf("\t%-41s : 0x%02x 0x%02x 0x%02x " \
		       "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		       "Transceiver codes", id[3], id[4], id[5], id[6],
		       id[7], id[8], id[9], id[10], id[36]);
	}
	/* 10G Ethernet Compliance Codes */
	if (id[3] & (1 << 7))
		module_print_any_string(pfx,
			"10G Ethernet: 10G Base-LRM [SFF-8472 rev10.4 onwards]");
	if (id[3] & (1 << 6))
		module_print_any_string(pfx, "10G Ethernet: 10G Base-LRM");
	if (id[3] & (1 << 5))
		module_print_any_string(pfx, "10G Ethernet: 10G Base-LR");
	if (id[3] & (1 << 4))
		module_print_any_string(pfx, "10G Ethernet: 10G Base-SR");
	/* Infiniband Compliance Codes */
	if (id[3] & (1 << 3))
		module_print_any_string(pfx, "Infiniband: 1X SX");
	if (id[3] & (1 << 2))
		module_print_any_string(pfx, "Infiniband: 1X LX");
	if (id[3] & (1 << 1))
		module_print_any_string(pfx, "Infiniband: 1X Copper Active");
	if (id[3] & (1 << 0))
		module_print_any_string(pfx, "Infiniband: 1X Copper Passive");
	/* ESCON Compliance Codes */
	if (id[4] & (1 << 7))
		module_print_any_string(pfx, "ESCON: ESCON MMF, 1310nm LED");
	if (id[4] & (1 << 6))
		module_print_any_string(pfx, "ESCON: ESCON SMF, 1310nm Laser");
	/* SONET Compliance Codes */
	if (id[4] & (1 << 5))
		module_print_any_string(pfx, "SONET: OC-192, short reach");
	if (id[4] & (1 << 4))
		module_print_any_string(pfx,
					"SONET: SONET reach specifier bit 1");
	if (id[4] & (1 << 3))
		module_print_any_string(pfx,
					"SONET: SONET reach specifier bit 2");
	if (id[4] & (1 << 2))
		module_print_any_string(pfx, "SONET: OC-48, long reach");
	if (id[4] & (1 << 1))
		module_print_any_string(pfx,
					"SONET: OC-48, intermediate reach");
	if (id[4] & (1 << 0))
		module_print_any_string(pfx, "SONET: OC-48, short reach");
	if (id[5] & (1 << 6))
		module_print_any_string(pfx,
					"SONET: OC-12, single mode, long reach");
	if (id[5] & (1 << 5))
		module_print_any_string(pfx,
					"SONET: OC-12, single mode, inter. reach");
	if (id[5] & (1 << 4))
		module_print_any_string(pfx, "SONET: OC-12, short reach");
	if (id[5] & (1 << 2))
		module_print_any_string(pfx,
					"SONET: OC-3, single mode, long reach");
	if (id[5] & (1 << 1))
		module_print_any_string(pfx,
					"SONET: OC-3, single mode, inter. reach");
	if (id[5] & (1 << 0))
		module_print_any_string(pfx, "SONET: OC-3, short reach");
	/* Ethernet Compliance Codes */
	if (id[6] & (1 << 7))
		module_print_any_string(pfx, "Ethernet: BASE-PX");
	if (id[6] & (1 << 6))
		module_print_any_string(pfx, "Ethernet: BASE-BX10");
	if (id[6] & (1 << 5))
		module_print_any_string(pfx, "Ethernet: 100BASE-FX");
	if (id[6] & (1 << 4))
		module_print_any_string(pfx, "Ethernet: 100BASE-LX/LX10");
	if (id[6] & (1 << 3))
		module_print_any_string(pfx, "Ethernet: 1000BASE-T");
	if (id[6] & (1 << 2))
		module_print_any_string(pfx, "Ethernet: 1000BASE-CX");
	if (id[6] & (1 << 1))
		module_print_any_string(pfx, "Ethernet: 1000BASE-LX");
	if (id[6] & (1 << 0))
		module_print_any_string(pfx, "Ethernet: 1000BASE-SX");
	/* Fibre Channel link length */
	if (id[7] & (1 << 7))
		module_print_any_string(pfx, "FC: very long distance (V)");
	if (id[7] & (1 << 6))
		module_print_any_string(pfx, "FC: short distance (S)");
	if (id[7] & (1 << 5))
		module_print_any_string(pfx, "FC: intermediate distance (I)");
	if (id[7] & (1 << 4))
		module_print_any_string(pfx, "FC: long distance (L)");
	if (id[7] & (1 << 3))
		module_print_any_string(pfx, "FC: medium distance (M)");
	/* Fibre Channel transmitter technology */
	if (id[7] & (1 << 2))
		module_print_any_string(pfx,
					"FC: Shortwave laser, linear Rx (SA)");
	if (id[7] & (1 << 1))
		module_print_any_string(pfx, "FC: Longwave laser (LC)");
	if (id[7] & (1 << 0))
		module_print_any_string(pfx,
					"FC: Electrical inter-enclosure (EL)");
	if (id[8] & (1 << 7))
		module_print_any_string(pfx,
					"FC: Electrical intra-enclosure (EL)");
	if (id[8] & (1 << 6))
		module_print_any_string(pfx,
					"FC: Shortwave laser w/o OFC (SN)");
	if (id[8] & (1 << 5))
		module_print_any_string(pfx,
					"FC: Shortwave laser with OFC (SL)");
	if (id[8] & (1 << 4))
		module_print_any_string(pfx, "FC: Longwave laser (LL)");
	if (id[8] & (1 << 3))
		module_print_any_string(pfx, "Active Cable");
	if (id[8] & (1 << 2))
		module_print_any_string(pfx, "Passive Cable");
	if (id[8] & (1 << 1))
		module_print_any_string(pfx, "FC: Copper FC-BaseT");
	/* Fibre Channel transmission media */
	if (id[9] & (1 << 7))
		module_print_any_string(pfx, "FC: Twin Axial Pair (TW)");
	if (id[9] & (1 << 6))
		module_print_any_string(pfx, "FC: Twisted Pair (TP)");
	if (id[9] & (1 << 5))
		module_print_any_string(pfx, "FC: Miniature Coax (MI)");
	if (id[9] & (1 << 4))
		module_print_any_string(pfx, "FC: Video Coax (TV)");
	if (id[9] & (1 << 3))
		module_print_any_string(pfx, "FC: Multimode, 62.5um (M6)");
	if (id[9] & (1 << 2))
		module_print_any_string(pfx, "FC: Multimode, 50um (M5)");
	if (id[9] & (1 << 0))
		module_print_any_string(pfx, "FC: Single Mode (SM)");
	/* Fibre Channel speed */
	if (id[10] & (1 << 7))
		module_print_any_string(pfx, "FC: 1200 MBytes/sec");
	if (id[10] & (1 << 6))
		module_print_any_string(pfx, "FC: 800 MBytes/sec");
	if (id[10] & (1 << 4))
		module_print_any_string(pfx, "FC: 400 MBytes/sec");
	if (id[10] & (1 << 2))
		module_print_any_string(pfx, "FC: 200 MBytes/sec");
	if (id[10] & (1 << 0))
		module_print_any_string(pfx, "FC: 100 MBytes/sec");
	/* Extended Specification Compliance Codes from SFF-8024 */
	{
		const char *ext = sff8024_ext_spec_compliance_name(id[36]);

		if (ext) {
			char value[140];

			snprintf(value, sizeof(value), "Extended: %s", ext);
			module_print_any_string(pfx, value);
		}
	}
}

static void sff8079_show_encoding(const __u8 *id)
{
	sff8024_show_encoding(id, 11, ETH_MODULE_SFF_8472);
}

static void sff8079_show_rate_identifier(const __u8 *id)
{
	char description[SFF_MAX_DESC_LEN];

	switch (id[13]) {
	case 0x00:
		sprintf(description, "%s", "unspecified");
		break;
	case 0x01:
		sprintf(description, "%s", "4/2/1G Rate_Select & AS0/AS1");
		break;
	case 0x02:
		sprintf(description, "%s", "8/4/2G Rx Rate_Select only");
		break;
	case 0x03:
		sprintf(description, "%s",
			"8/4/2G Independent Rx & Tx Rate_Select");
		break;
	case 0x04:
		sprintf(description, "%s", "8/4/2G Tx Rate_Select only");
		break;
	default:
		sprintf(description, "%s", "reserved or unknown");
		break;
	}

	sff_print_any_hex_field("Rate identifier", "rate_identifier", id[13],
				description);
}

static void sff8079_show_wavelength_or_copper_compliance(const __u8 *id)
{
	char description[SFF_MAX_DESC_LEN];

	if (id[8] & (1 << 2)) {
		switch (id[60]) {
		case 0x00:
			strncpy(description, "unspecified",
				SFF_MAX_DESC_LEN);
			break;
		case 0x01:
			strncpy(description, "SFF-8431 appendix E",
				SFF_MAX_DESC_LEN);
			break;
		default:
			strncpy(description, "unknown", SFF_MAX_DESC_LEN);
			break;
		}
		strcat(description, " [SFF-8472 rev10.4 only]");
		sff_print_any_hex_field("Passive Cu cmplnce.",
					"passive_cu_cmplnce.",
					id[60], description);
	} else if (id[8] & (1 << 3)) {
		printf("\t%-41s : 0x%02x", "Active Cu cmplnce.", id[60]);
		switch (id[60]) {
		case 0x00:
			strncpy(description, "unspecified",
				SFF_MAX_DESC_LEN);
			break;
		case 0x01:
			strncpy(description, "SFF-8431 appendix E",
				SFF_MAX_DESC_LEN);
			break;
		case 0x04:
			strncpy(description, "SFF-8431 limiting",
				SFF_MAX_DESC_LEN);
			break;
		default:
			strncpy(description, "unknown", SFF_MAX_DESC_LEN);
			break;
		}
		strcat(description, " [SFF-8472 rev10.4 only]");
		sff_print_any_hex_field("Active Cu cmplnce.",
					"active_cu_cmplnce.",
					id[60], description);
	} else {
		module_print_any_uint("Laser wavelength",
				      (id[60] << 8) | id[61], "nm");
	}
}

static void sff8079_show_options(const __u8 *id)
{
	static const char *pfx = "Option";
	char value[64] = "";

	if (is_json_context()) {
		open_json_array("option_values", "");
		print_uint(PRINT_JSON, NULL, "%u", id[64]);
		print_uint(PRINT_JSON, NULL, "%u", id[65]);
		close_json_array("");
	} else {
		printf("\t%-41s : 0x%02x 0x%02x\n", "Option values", id[64],
		       id[65]);
	}
	if (id[65] & (1 << 1))
		sprintf(value, "%s", "RX_LOS implemented");
	if (id[65] & (1 << 2))
		sprintf(value, "%s", "RX_LOS implemented, inverted");
	if (id[65] & (1 << 3))
		sprintf(value, "%s", "TX_FAULT implemented");
	if (id[65] & (1 << 4))
		sprintf(value, "%s", "TX_DISABLE implemented");
	if (id[65] & (1 << 5))
		sprintf(value, "%s", "RATE_SELECT implemented");
	if (id[65] & (1 << 6))
		sprintf(value, "%s", "Tunable transmitter technology");
	if (id[65] & (1 << 7))
		sprintf(value, "%s", "Receiver decision threshold implemented");
	if (id[64] & (1 << 0))
		sprintf(value, "%s", "Linear receiver output implemented");
	if (id[64] & (1 << 1))
		sprintf(value, "%s", "Power level 2 requirement");
	if (id[64] & (1 << 2))
		sprintf(value, "%s", "Cooled transceiver implemented");
	if (id[64] & (1 << 3))
		sprintf(value, "%s", "Retimer or CDR implemented");
	if (id[64] & (1 << 4))
		sprintf(value, "%s", "Paging implemented");
	if (id[64] & (1 << 5))
		sprintf(value, "%s", "Power level 3 requirement");

	if (value[0] != '\0')
		module_print_any_string(pfx, value);
}

static void sff8079_show_all_common(const __u8 *id)
{
	sff8079_show_identifier(id);
	if (((id[0] == 0x02) || (id[0] == 0x03)) && (id[1] == 0x04)) {
		unsigned int br_nom, br_min, br_max;

		if (id[12] == 0) {
			br_nom = br_min = br_max = 0;
		} else if (id[12] == 255) {
			br_nom = id[66] * 250;
			br_max = id[67];
			br_min = id[67];
		} else {
			br_nom = id[12] * 100;
			br_max = id[66];
			br_min = id[67];
		}
		sff8079_show_ext_identifier(id);
		sff8079_show_connector(id);
		sff8079_show_transceiver(id);
		sff8079_show_encoding(id);
		module_print_any_uint("BR Nominal", br_nom, "MBd");
		sff8079_show_rate_identifier(id);
		module_show_value_with_unit(id, 14, "Length (SMF)", 1, "km");
		module_show_value_with_unit(id, 16, "Length (OM2)", 10, "m");
		module_show_value_with_unit(id, 17, "Length (OM1)", 10, "m");
		module_show_value_with_unit(id, 18,
					    "Length (Copper or Active cable)",
					    1, "m");
		module_show_value_with_unit(id, 19, "Length (OM3)", 10, "m");
		sff8079_show_wavelength_or_copper_compliance(id);
		module_show_ascii(id, 20, 35, "Vendor name");
		module_show_oui(id, 37);
		module_show_ascii(id, 40, 55, "Vendor PN");
		module_show_ascii(id, 56, 59, "Vendor rev");
		sff8079_show_options(id);
		module_print_any_uint("BR margin max", br_max, "%");
		module_print_any_uint("BR margin min", br_min, "%");
		module_show_ascii(id, 68, 83, "Vendor SN");
		module_show_date_code(id, 84);
	}
}

void sff8079_show_all_ioctl(const __u8 *id)
{
	sff8079_show_all_common(id);
}

static int sff8079_get_eeprom_page(struct cmd_context *ctx, u8 i2c_address,
				   __u8 *buf)
{
	struct module_eeprom request = {
		.length = SFF8079_PAGE_SIZE,
		.i2c_address = i2c_address,
	};
	int ret;

	ret = get_eeprom_page(ctx, &request);
	if (!ret)
		memcpy(buf, request.data, SFF8079_PAGE_SIZE);

	return ret;
}

int sff8079_show_all_nl(struct cmd_context *ctx)
{
	u8 *buf;
	int ret;

	/* The SFF-8472 parser expects a single buffer that contains the
	 * concatenation of the first 256 bytes from addresses A0h and A2h,
	 * respectively.
	 */
	buf = calloc(1, ETH_MODULE_SFF_8472_LEN);
	if (!buf)
		return -ENOMEM;

	/* Read A0h page */
	ret = sff8079_get_eeprom_page(ctx, SFF8079_I2C_ADDRESS_LOW, buf);
	if (ret)
		goto out;

	new_json_obj(ctx->json);
	open_json_object(NULL);
	sff8079_show_all_common(buf);

	/* Finish if A2h page is not present */
	if (!(buf[92] & (1 << 6)))
		goto out_json;

	/* Read A2h page */
	ret = sff8079_get_eeprom_page(ctx, SFF8079_I2C_ADDRESS_HIGH,
				      buf + ETH_MODULE_SFF_8079_LEN);
	if (ret) {
		fprintf(stderr, "Failed to read Page A2h.\n");
		goto out_json;
	}

	sff8472_show_all(buf);
out_json:
	close_json_object();
	delete_json_obj();
out:
	free(buf);

	return ret;
}
