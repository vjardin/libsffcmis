/*
 * qsfp.c: Implements SFF-8636 based QSFP+/QSFP28 Diagnostics Memory map.
 *
 * Copyright 2010 Solarflare Communications Inc.
 * Aurelien Guillaume <aurelien@iwi.me> (C) 2012
 * Copyright (C) 2014 Cumulus networks Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Freeoftware Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  Vidya Ravipati <vidya@cumulusnetworks.com>
 *   This implementation is loosely based on current SFP parser
 *   and SFF-8636 spec Rev 2.7 (ftp://ftp.seagate.com/pub/sff/SFF-8636.PDF)
 *   by SFF Committee.
 */

/*
 *	Description:
 *	a) The register/memory layout is up to 5 128 byte pages defined by
 *		a "pages valid" register and switched via a "page select"
 *		register. Memory of 256 bytes can be memory mapped at a time
 *		according to SFF 8636.
 *	b) SFF 8636 based 640 bytes memory layout is presented for parser
 *
 *           SFF 8636 based QSFP Memory Map
 *
 *           2-Wire Serial Address: 1010000x
 *
 *           Lower Page 00h (128 bytes)
 *           ======================
 *           |                     |
 *           |Page Select Byte(127)|
 *           ======================
 *                    |
 *                    V
 *	     ----------------------------------------
 *	    |             |            |             |
 *	    V             V            V             V
 *	 ----------   ----------   ---------    ------------
 *	| Upper    | | Upper    | | Upper    | | Upper      |
 *	| Page 00h | | Page 01h | | Page 02h | | Page 03h   |
 *	|          | |(Optional)| |(Optional)| | (Optional) |
 *	|          | |          | |          | |            |
 *	|          | |          | |          | |            |
 *	|    ID    | |   AST    | |  User    | |  For       |
 *	|  Fields  | |  Table   | | EEPROM   | |  Cable     |
 *	|          | |          | | Data     | | Assemblies |
 *	|          | |          | |          | |            |
 *	|          | |          | |          | |            |
 *	-----------  -----------   ----------  --------------
 *
 *
 **/
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "module-common.h"
#include "qsfp.h"
#include "qsfp-ext.h"
#include "i2c.h"
#include "sffcmis.h"
#include "cmis.h"

struct sff8636_memory_map {
	const __u8 *lower_memory;
	const __u8 *upper_memory[4];
#define page_00h upper_memory[0x0]
#define page_03h upper_memory[0x3]
};

#define SFF8636_PAGE_SIZE	0x80
#define SFF8636_I2C_ADDRESS	0x50
#define SFF8636_MAX_CHANNEL_NUM	4

#define MAX_DESC_SIZE	42

static void sff8636_show_identifier(const struct sff8636_memory_map *map)
{
	module_show_identifier(map->lower_memory, SFF8636_ID_OFFSET);
}

static void sff8636_show_ext_identifier(const struct sff8636_memory_map *map)
{
	static const char *pfx =
		"\tExtended identifier description           :";
	char description[64];

	if (is_json_context()) {
		open_json_object("extended_identifier");
		print_uint(PRINT_JSON, "value", "%u",
			   map->page_00h[SFF8636_EXT_ID_OFFSET]);
	} else {
		printf("\t%-41s : 0x%02x\n", "Extended identifier",
		       map->page_00h[SFF8636_EXT_ID_OFFSET]);
	}

	open_json_array("description", "");
	switch (map->page_00h[SFF8636_EXT_ID_OFFSET] &
		SFF8636_EXT_ID_PWR_CLASS_MASK) {
	case SFF8636_EXT_ID_PWR_CLASS_1:
		strncpy(description, "1.5W max. Power consumption", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_2:
		strncpy(description, "1.5W max. Power consumption", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_3:
		strncpy(description, "2.5W max. Power consumption", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_4:
		strncpy(description, "3.5W max. Power consumption", 64);
		break;
	}

	if (is_json_context())
		print_string(PRINT_JSON, NULL, "%s", description);
	else
		printf("%s %s\n", pfx, description);

	if (map->page_00h[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_CDR_TX_MASK)
		strncpy(description, "CDR present in TX,", 64);
	else
		strncpy(description, "No CDR in TX,", 64);

	if (map->page_00h[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_CDR_RX_MASK)
		strcat(description, " CDR present in RX");
	else
		strcat(description, " No CDR in RX");

	if (is_json_context())
		print_string(PRINT_JSON, NULL, "%s", description);
	else
		printf("%s %s\n", pfx, description);

	switch (map->page_00h[SFF8636_EXT_ID_OFFSET] &
		SFF8636_EXT_ID_EPWR_CLASS_MASK) {
	case SFF8636_EXT_ID_PWR_CLASS_LEGACY:
		strncpy(description, "", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_5:
		strncpy(description, "4.0W max. Power consumption,", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_6:
		strncpy(description, "4.5W max. Power consumption,", 64);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_7:
		strncpy(description, "5.0W max. Power consumption,", 64);
		break;
	}

	if (map->lower_memory[SFF8636_PWR_MODE_OFFSET] &
	    SFF8636_HIGH_PWR_ENABLE)
		strcat(description, " High Power Class (> 3.5 W) enabled");
	else
		strcat(description,
		       " High Power Class (> 3.5 W) not enabled");

	if (is_json_context())
		print_string(PRINT_JSON, NULL, "%s", description);
	else
		printf("%s %s\n", pfx, description);

	close_json_array("");
	close_json_object();

	bool value = map->lower_memory[SFF8636_PWR_MODE_OFFSET] &
			SFF8636_LOW_PWR_SET;
	module_print_any_bool("Power set", "power_set", value, ONOFF(value));
	value = map->lower_memory[SFF8636_PWR_MODE_OFFSET] &
			SFF8636_PWR_OVERRIDE;
	module_print_any_bool("Power override", "power_override", value,
			      ONOFF(value));
}

static void sff8636_show_connector(const struct sff8636_memory_map *map)
{
	module_show_connector(map->page_00h, SFF8636_CTOR_OFFSET);
}

static const char *sff8636_ext_compliance_desc(__u8 code)
{
	const char *name = sff8024_ext_spec_compliance_name(code);

	if (name)
		return name;
	if (code == 0x00)
		return "Unspecified";
	return "Reserved";
}

static void sff8636_show_transceiver(const struct sff8636_memory_map *map)
{
	static const char *pfx = "Transceiver type";

	if (is_json_context()) {
		open_json_array("transceiver_codes", "");
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_ETHERNET_COMP_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_SONET_COMP_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_SAS_COMP_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_GIGE_COMP_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_FC_LEN_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_FC_TECH_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET]);
		print_uint(PRINT_JSON, NULL, "%u",
			   map->page_00h[SFF8636_FC_SPEED_OFFSET]);
		close_json_array("");
	}

	/* 10G/40G Ethernet Compliance Codes */
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_10G_LRM)
		module_print_any_string(pfx, "10G Ethernet: 10G Base-LRM");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_10G_LR)
		module_print_any_string(pfx, "10G Ethernet: 10G Base-LR");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_10G_SR)
		module_print_any_string(pfx, "10G Ethernet: 10G Base-SR");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_40G_CR4)
		module_print_any_string(pfx, "40G Ethernet: 40G Base-CR4");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_40G_SR4)
		module_print_any_string(pfx, "40G Ethernet: 40G Base-SR4");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_40G_LR4)
		module_print_any_string(pfx, "40G Ethernet: 40G Base-LR4");
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_40G_ACTIVE)
		module_print_any_string(pfx,
					"40G Ethernet: 40G Active Cable (XLPPI)");
	/* Extended Specification Compliance Codes from SFF-8024 */
	if (map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
	    SFF8636_ETHERNET_RSRVD) {
		__u8 ext_code = map->page_00h[SFF8636_OPTION_1_OFFSET];

		sff_print_any_hex_field("Extended compliance",
					"extended_compliance", ext_code,
					sff8636_ext_compliance_desc(ext_code));
	}

	/* SONET Compliance Codes */
	if (map->page_00h[SFF8636_SONET_COMP_OFFSET] &
	    SFF8636_SONET_40G_OTN)
		module_print_any_string(pfx, "40G OTN (OTU3B/OTU3C)");
	if (map->page_00h[SFF8636_SONET_COMP_OFFSET] & SFF8636_SONET_OC48_LR)
		module_print_any_string(pfx, "SONET: OC-48, long reach");
	if (map->page_00h[SFF8636_SONET_COMP_OFFSET] & SFF8636_SONET_OC48_IR)
		module_print_any_string(pfx, "SONET: OC-48, intermediate reach");
	if (map->page_00h[SFF8636_SONET_COMP_OFFSET] & SFF8636_SONET_OC48_SR)
		module_print_any_string(pfx, "SONET: OC-48, short reach");

	/* SAS/SATA Compliance Codes */
	if (map->page_00h[SFF8636_SAS_COMP_OFFSET] & SFF8636_SAS_24G)
		module_print_any_string(pfx, "SAS 24.0G");
	if (map->page_00h[SFF8636_SAS_COMP_OFFSET] & SFF8636_SAS_12G)
		module_print_any_string(pfx, "SAS 12.0G");
	if (map->page_00h[SFF8636_SAS_COMP_OFFSET] & SFF8636_SAS_6G)
		module_print_any_string(pfx, "SAS 6.0G");
	if (map->page_00h[SFF8636_SAS_COMP_OFFSET] & SFF8636_SAS_3G)
		module_print_any_string(pfx, "SAS 3.0G");

	/* Ethernet Compliance Codes */
	if (map->page_00h[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_T)
		module_print_any_string(pfx, "Ethernet: 1000BASE-T");
	if (map->page_00h[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_CX)
		module_print_any_string(pfx, "Ethernet: 1000BASE-CX");
	if (map->page_00h[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_LX)
		module_print_any_string(pfx, "Ethernet: 1000BASE-LX");
	if (map->page_00h[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_SX)
		module_print_any_string(pfx, "Ethernet: 1000BASE-SX");

	/* Fibre Channel link length */
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_VERY_LONG)
		module_print_any_string(pfx, "FC: very long distance (V)");
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_SHORT)
		module_print_any_string(pfx, "FC: short distance (S)");
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_INT)
		module_print_any_string(pfx, "FC: intermediate distance (I)");
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_LONG)
		module_print_any_string(pfx, "FC: long distance (L)");
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_MED)
		module_print_any_string(pfx, "FC: medium distance (M)");

	/* Fibre Channel transmitter technology */
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_TECH_LONG_LC)
		module_print_any_string(pfx, "FC: Longwave laser (LC)");
	if (map->page_00h[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_TECH_ELEC_INTER)
		module_print_any_string(pfx,
					"FC: Electrical inter-enclosure (EL)");
	if (map->page_00h[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_ELEC_INTRA)
		module_print_any_string(pfx,
					"FC: Electrical intra-enclosure (EL)");
	if (map->page_00h[SFF8636_FC_TECH_OFFSET] &
	    SFF8636_FC_TECH_SHORT_WO_OFC)
		module_print_any_string(pfx,
					"FC: Shortwave laser w/o OFC (SN)");
	if (map->page_00h[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_SHORT_W_OFC)
		module_print_any_string(pfx,
					"FC: Shortwave laser with OFC (SL)");
	if (map->page_00h[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_LONG_LL)
		module_print_any_string(pfx, "FC: Longwave laser (LL)");

	/* Fibre Channel transmission media */
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_TW)
		module_print_any_string(pfx, "FC: Twin Axial Pair (TW)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_TP)
		module_print_any_string(pfx, "FC: Twisted Pair (TP)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_MI)
		module_print_any_string(pfx, "FC: Miniature Coax (MI)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_TV)
		module_print_any_string(pfx, "FC: Video Coax (TV)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_M6)
		module_print_any_string(pfx, "FC: Multimode, 62.5m (M6)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_M5)
		module_print_any_string(pfx, "FC: Multimode, 50m (M5)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_OM3)
		module_print_any_string(pfx, "FC: Multimode, 50um (OM3)");
	if (map->page_00h[SFF8636_FC_TRANS_MEDIA_OFFSET] &
	    SFF8636_FC_TRANS_MEDIA_SM)
		module_print_any_string(pfx, "FC: Single Mode (SM)");

	/* Fibre Channel speed */
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_1200_MBPS)
		module_print_any_string(pfx, "FC: 1200 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_800_MBPS)
		module_print_any_string(pfx, "FC: 800 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_1600_MBPS)
		module_print_any_string(pfx, "FC: 1600 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_400_MBPS)
		module_print_any_string(pfx, "FC: 400 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_3200_MBPS)
		module_print_any_string(pfx, "FC: 3200 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_200_MBPS)
		module_print_any_string(pfx, "FC: 200 MBytes/sec");
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_EXTENDED) {
		/* Only show if not already displayed from byte 131 bit 7 */
		if (!(map->page_00h[SFF8636_ETHERNET_COMP_OFFSET] &
		      SFF8636_ETHERNET_RSRVD)) {
			__u8 ext_code =
				map->page_00h[SFF8636_OPTION_1_OFFSET];

			sff_print_any_hex_field("Extended compliance",
						"extended_compliance",
						ext_code,
						sff8636_ext_compliance_desc(
							ext_code));
		}
	}
	if (map->page_00h[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_100_MBPS)
		module_print_any_string(pfx, "FC: 100 MBytes/sec");

	/* Secondary Extended Specification Compliance (byte 116) */
	if (map->page_00h[SFF8636_SEC_EXT_COMP_OFFSET]) {
		__u8 sec_code = map->page_00h[SFF8636_SEC_EXT_COMP_OFFSET];

		sff_print_any_hex_field("Secondary ext. compliance",
					"secondary_ext_compliance", sec_code,
					sff8636_ext_compliance_desc(sec_code));
	}
}

static void sff8636_show_encoding(const struct sff8636_memory_map *map)
{
	sff8024_show_encoding(map->page_00h, SFF8636_ENCODING_OFFSET,
			      ETH_MODULE_SFF_8636);
}

static void sff8636_show_rate_identifier(const struct sff8636_memory_map *map)
{
	/* TODO: Need to fix rate select logic */
	sff_print_any_hex_field("Rate identifier", "rate_identifier",
				map->page_00h[SFF8636_EXT_RS_OFFSET], NULL);


}

static void
sff8636_show_wavelength_or_copper_compliance(const struct sff8636_memory_map *map)
{
	u16 value = map->page_00h[SFF8636_DEVICE_TECH_OFFSET] &
			SFF8636_TRANS_TECH_MASK;

	module_show_mit_compliance(value, MODULE_TYPE_SFF8636);

	/* Device technology sub-fields (byte 147, bits 3-0) */
	{
		__u8 tech = map->page_00h[SFF8636_DEVICE_TECH_OFFSET];
		bool v;

		v = tech & SFF8636_DEV_TECH_ACTIVE_WAVE_LEN;
		module_print_any_bool("Active wavelength control",
				      NULL, v, YESNO(v));
		v = tech & SFF8636_DEV_TECH_COOL_TRANS;
		module_print_any_bool("Cooled transmitter", NULL, v, YESNO(v));
		v = tech & SFF8636_DEV_TECH_APD_DETECTOR;
		module_print_any_bool("APD/Pin detector", NULL, v, YESNO(v));
		v = tech & SFF8636_DEV_TECH_TUNABLE;
		module_print_any_bool("Transmitter tunable", NULL, v, YESNO(v));
	}

	if (value >= SFF8636_TRANS_COPPER_PAS_UNEQUAL) {
		module_print_any_uint("Attenuation at 2.5GHz",
				      map->page_00h[SFF8636_WAVELEN_HIGH_BYTE_OFFSET],
				      "db");
		module_print_any_uint("Attenuation at 5.0GHz",
				      map->page_00h[SFF8636_WAVELEN_LOW_BYTE_OFFSET],
				      "db");
		module_print_any_uint("Attenuation at 7.0GHz",
				      map->page_00h[SFF8636_WAVELEN_HIGH_BYTE_OFFSET],
				      "db");
		module_print_any_uint("Attenuation at 12.9GHz",
				      map->page_00h[SFF8636_WAVELEN_LOW_BYTE_OFFSET],
				      "db");
	} else {
		module_print_any_float("Laser wavelength",
				       (((map->page_00h[SFF8636_WAVELEN_HIGH_BYTE_OFFSET] << 8) |
					map->page_00h[SFF8636_WAVELEN_LOW_BYTE_OFFSET]) * 0.05),
				       "nm");
		module_print_any_float("Laser wavelength tolerance",
				       (((map->page_00h[SFF8636_WAVE_TOL_HIGH_BYTE_OFFSET] << 8) |
					map->page_00h[SFF8636_WAVE_TOL_LOW_BYTE_OFFSET]) * 0.05),
				       "nm");
	}
}

static void sff8636_show_revision_compliance(const __u8 *id, int rev_offset)
{
	const char *pfx = "Revision Compliance";
	char value[64] = "";

	switch (id[rev_offset]) {
	case SFF8636_REV_UNSPECIFIED:
		sprintf(value, "%s", "Revision not specified");
		break;
	case SFF8636_REV_8436_48:
		sprintf(value, "%s", "SFF-8436 Rev 4.8 or earlier");
		break;
	case SFF8636_REV_8436_8636:
		sprintf(value, "%s", "SFF-8436 Rev 4.8 or earlier");
		break;
	case SFF8636_REV_8636_13:
		sprintf(value, "%s", "SFF-8636 Rev 1.3 or earlier");
		break;
	case SFF8636_REV_8636_14:
		sprintf(value, "%s", "SFF-8636 Rev 1.4");
		break;
	case SFF8636_REV_8636_15:
		sprintf(value, "%s", "SFF-8636 Rev 1.5");
		break;
	case SFF8636_REV_8636_20:
		sprintf(value, "%s", "SFF-8636 Rev 2.0");
		break;
	case SFF8636_REV_8636_27:
		sprintf(value, "%s", "SFF-8636 Rev 2.5/2.6/2.7");
		break;
	default:
		sprintf(value, "%s", "Unallocated");
		break;
	}
	module_print_any_string(pfx, value);
}

/*
 * 2-byte internal temperature conversions:
 * First byte is a signed 8-bit integer, which is the temp decimal part
 * Second byte are 1/256th of degree, which are added to the dec part.
 */
#define SFF8636_OFFSET_TO_TEMP(offset) ((__s16)OFFSET_TO_U16(offset))

static void sff8636_dom_parse(const struct sff8636_memory_map *map,
			      struct sff_diags *sd)
{
	const __u8 *id = map->lower_memory;
	int i = 0;

	/* Monitoring Thresholds for Alarms and Warnings */
	sd->sfp_voltage[MCURR] = OFFSET_TO_U16_PTR(id, SFF8636_VCC_CURR);
	sd->sfp_temp[MCURR] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_CURR);

	if (!map->page_03h)
		goto out;

	sd->sfp_voltage[HALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						   SFF8636_VCC_HALRM);
	sd->sfp_voltage[LALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						   SFF8636_VCC_LALRM);
	sd->sfp_voltage[HWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						   SFF8636_VCC_HWARN);
	sd->sfp_voltage[LWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						   SFF8636_VCC_LWARN);

	sd->sfp_temp[HALRM] = (__s16)OFFSET_TO_U16_PTR(map->page_03h,
						       SFF8636_TEMP_HALRM);
	sd->sfp_temp[LALRM] = (__s16)OFFSET_TO_U16_PTR(map->page_03h,
						       SFF8636_TEMP_LALRM);
	sd->sfp_temp[HWARN] = (__s16)OFFSET_TO_U16_PTR(map->page_03h,
						       SFF8636_TEMP_HWARN);
	sd->sfp_temp[LWARN] = (__s16)OFFSET_TO_U16_PTR(map->page_03h,
						       SFF8636_TEMP_LWARN);

	sd->bias_cur[HALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_BIAS_HALRM);
	sd->bias_cur[LALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_BIAS_LALRM);
	sd->bias_cur[HWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_BIAS_HWARN);
	sd->bias_cur[LWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_BIAS_LWARN);

	sd->tx_power[HALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_PWR_HALRM);
	sd->tx_power[LALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_PWR_LALRM);
	sd->tx_power[HWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_PWR_HWARN);
	sd->tx_power[LWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_TX_PWR_LWARN);

	sd->rx_power[HALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_RX_PWR_HALRM);
	sd->rx_power[LALRM] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_RX_PWR_LALRM);
	sd->rx_power[HWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_RX_PWR_HWARN);
	sd->rx_power[LWARN] = OFFSET_TO_U16_PTR(map->page_03h,
						SFF8636_RX_PWR_LWARN);

out:
	/* Channel Specific Data */
	for (i = 0; i < SFF8636_MAX_CHANNEL_NUM; i++) {
		u8 rx_power_offset, tx_bias_offset;
		u8 tx_power_offset;

		switch (i) {
		case 0:
			rx_power_offset = SFF8636_RX_PWR_1_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_1_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_1_OFFSET;
			break;
		case 1:
			rx_power_offset = SFF8636_RX_PWR_2_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_2_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_2_OFFSET;
			break;
		case 2:
			rx_power_offset = SFF8636_RX_PWR_3_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_3_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_3_OFFSET;
			break;
		case 3:
			rx_power_offset = SFF8636_RX_PWR_4_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_4_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_4_OFFSET;
			break;
		}
		sd->scd[i].bias_cur = OFFSET_TO_U16(tx_bias_offset);
		sd->scd[i].rx_power = OFFSET_TO_U16(rx_power_offset);
		sd->scd[i].tx_power = OFFSET_TO_U16(tx_power_offset);
	}
}

static void sff8636_show_dom_chan_lvl_tx_bias(const struct sff_diags *sd)
{
	char power_string[MAX_DESC_SIZE];
	int i;

	open_json_array("laser_tx_bias_current", "");
	for (i = 0; i < SFF8636_MAX_CHANNEL_NUM; i++) {
		if (is_json_context()) {
			print_float(PRINT_JSON, NULL, "%.3f",
				    (double)sd->scd[i].bias_cur / 500.);
		} else {
			snprintf(power_string, MAX_DESC_SIZE, "%s (Channel %d)",
				 "Laser tx bias current", i+1);
			PRINT_BIAS(power_string, sd->scd[i].bias_cur);
		}
	}
	close_json_array("");
}

static void sff8636_show_dom_chan_lvl_tx_power(const struct sff_diags *sd)
{
	char power_string[MAX_DESC_SIZE];
	int i;

	open_json_array("transmit_avg_optical_power", "");
	for (i = 0; i < SFF8636_MAX_CHANNEL_NUM; i++) {
		if (is_json_context()) {
			print_float(PRINT_JSON, NULL, "%.4f",
				    (double)sd->scd[i].tx_power / 10000.);
		} else {
			snprintf(power_string, MAX_DESC_SIZE, "%s (Channel %d)",
				 "Transmit avg optical power", i+1);
			PRINT_xX_PWR(power_string, sd->scd[i].tx_power);
		}
	}
	close_json_array("");
}

static void sff8636_show_dom_chan_lvl_rx_power(const struct sff_diags *sd)
{
	char *rx_power_type_string = NULL;
	char power_string[MAX_DESC_SIZE];
	int i;

	if (!sd->rx_power_type)
		rx_power_type_string = "Receiver signal OMA";
	else
		rx_power_type_string = "Rcvr signal avg optical power";

	open_json_object("rx_power");

	open_json_array("values", "");
	for (i = 0; i < SFF8636_MAX_CHANNEL_NUM; i++) {
		if (is_json_context()) {
			print_float(PRINT_JSON, NULL, "%.4f",
				    (double)sd->scd[i].rx_power / 10000.);
		} else {
			snprintf(power_string, MAX_DESC_SIZE, "%s (Channel %d)",
				 rx_power_type_string, i+1);
			PRINT_xX_PWR(power_string, sd->scd[i].rx_power);
		}
	}
	close_json_array("");

	if (is_json_context())
		module_print_any_string("type", rx_power_type_string);
	close_json_object();
}

static void
sff8636_show_dom_chan_lvl_flags(const struct sff8636_memory_map *map)
{
	bool value;
	int i;

	for (i = 0; module_aw_chan_flags[i].fmt_str; ++i) {
		bool is_start = (i % SFF8636_MAX_CHANNEL_NUM == 0);
		bool is_end = (i % SFF8636_MAX_CHANNEL_NUM ==
			       SFF8636_MAX_CHANNEL_NUM - 1);
		char json_str[80] = {};
		char str[80] = {};

		if (module_aw_chan_flags[i].type != MODULE_TYPE_SFF8636)
			continue;

		convert_json_field_name(module_aw_chan_flags[i].fmt_str,
					json_str);

		value = map->lower_memory[module_aw_chan_flags[i].offset] &
			module_aw_chan_flags[i].adver_value;
		if (is_json_context()) {
			if (is_start)
				open_json_array(json_str, "");

			print_bool(PRINT_JSON, NULL, NULL, value);

			if (is_end)
				close_json_array("");
		} else {
			snprintf(str, 80, "%s (Chan %d)",
				 module_aw_chan_flags[i].fmt_str,
				 (i % SFF8636_MAX_CHANNEL_NUM) + 1);
			printf("\t%-41s : %s\n", str, ONOFF(value));
		}

	}
}

static void
sff8636_show_dom_mod_lvl_flags(const struct sff8636_memory_map *map)
{
	bool value;
	int i;

	for (i = 0; module_aw_mod_flags[i].str; ++i) {
		if (module_aw_mod_flags[i].type != MODULE_TYPE_SFF8636)
			continue;

		value = map->lower_memory[module_aw_mod_flags[i].offset] &
			module_aw_mod_flags[i].value;

		module_print_any_bool(module_aw_mod_flags[i].str, NULL,
				      value, ONOFF(value));
	}
}

static void sff8636_show_dom(const struct sff8636_memory_map *map)
{
	struct sff_diags sd = {0};

	/*
	 * There is no clear identifier to signify the existence of
	 * optical diagnostics similar to SFF-8472. So checking existence
	 * of page 3, will provide the gurantee for existence of alarms
	 * and thresholds
	 * If pagging support exists, then supports_alarms is marked as 1
	 */
	if (map->page_03h)
		sd.supports_alarms = 1;

	sd.rx_power_type = map->page_00h[SFF8636_DIAG_TYPE_OFFSET] &
			   SFF8636_RX_PWR_TYPE_MASK;
	sd.tx_power_type = map->page_00h[SFF8636_DIAG_TYPE_OFFSET] &
			   SFF8636_TX_PWR_TYPE_MASK;

	sff8636_dom_parse(map, &sd);

	module_show_dom_mod_lvl_monitors(&sd);

	/*
	 * SFF-8636/8436 spec is not clear whether RX power/ TX bias
	 * current fields are supported or not. A valid temperature
	 * reading is used as existence for TX/RX power.
	 */
	if ((sd.sfp_temp[MCURR] == 0x0) ||
	    (sd.sfp_temp[MCURR] == (__s16)0xFFFF))
		return;

	module_print_any_bool("Alarm/warning flags implemented",
			      "alarm/warning_flags_implemented",
			      sd.supports_alarms, YESNO(sd.supports_alarms));

	sff8636_show_dom_chan_lvl_tx_bias(&sd);
	sff8636_show_dom_chan_lvl_tx_power(&sd);
	sff8636_show_dom_chan_lvl_rx_power(&sd);

	if (sd.supports_alarms) {
		sff8636_show_dom_chan_lvl_flags(map);
		sff8636_show_dom_mod_lvl_flags(map);

		if (is_json_context())
			sff_show_thresholds_json(sd);
		else
			sff_show_thresholds(sd);
	}
}

static void sff8636_show_ext_module_codes(const struct sff8636_memory_map *map)
{
	static const char *pfx = "Extended module code";
	__u8 code = map->page_00h[SFF8636_EXT_MOD_CODE_OFFSET];

	if (!code)
		return;

	if (code & SFF8636_EXT_MOD_INFINIBAND_HDR)
		module_print_any_string(pfx, "InfiniBand HDR");
	if (code & SFF8636_EXT_MOD_INFINIBAND_EDR)
		module_print_any_string(pfx, "InfiniBand EDR");
	if (code & SFF8636_EXT_MOD_INFINIBAND_FDR)
		module_print_any_string(pfx, "InfiniBand FDR");
	if (code & SFF8636_EXT_MOD_INFINIBAND_QDR)
		module_print_any_string(pfx, "InfiniBand QDR");
	if (code & SFF8636_EXT_MOD_INFINIBAND_DDR)
		module_print_any_string(pfx, "InfiniBand DDR");
	if (code & SFF8636_EXT_MOD_INFINIBAND_SDR)
		module_print_any_string(pfx, "InfiniBand SDR");
}

static void sff8636_show_max_case_temp(const struct sff8636_memory_map *map)
{
	__u8 temp = map->page_00h[SFF8636_MAXCASE_TEMP_OFFSET];

	if (temp == 0x00)
		temp = 70;
	module_print_any_uint("Max case temperature", temp, " degrees C");
}

static void sff8636_show_checksums(const struct sff8636_memory_map *map)
{
	__u8 sum = 0;
	int i;

	/* CC_BASE: sum bytes 128-190, compare with byte 191 */
	for (i = 0x80; i <= 0xBE; i++)
		sum += map->page_00h[i];
	module_print_any_string("CC_BASE",
				(sum == map->page_00h[SFF8636_CC_BASE_OFFSET]) ?
				"pass" : "fail");

	/* CC_EXT: sum bytes 192-222, compare with byte 223 */
	sum = 0;
	for (i = 0xC0; i <= 0xDE; i++)
		sum += map->page_00h[i];
	module_print_any_string("CC_EXT",
				(sum == map->page_00h[SFF8636_CC_EXT_OFFSET]) ?
				"pass" : "fail");
}

static void sff8636_show_signals(const struct sff8636_memory_map *map)
{
	unsigned int v;

	/* There appears to be no Rx LOS support bit, use Tx for both */
	if (map->page_00h[SFF8636_OPTION_4_OFFSET] & SFF8636_O4_TX_LOS) {
		v = map->lower_memory[SFF8636_LOS_AW_OFFSET] & 0xf;
		module_show_lane_status("Rx loss of signal", 4, "Yes", "No", v);
		v = map->lower_memory[SFF8636_LOS_AW_OFFSET] >> 4;
		module_show_lane_status("Tx loss of signal", 4, "Yes", "No", v);
	}

	v = map->lower_memory[SFF8636_LOL_AW_OFFSET] & 0xf;
	if (map->page_00h[SFF8636_OPTION_3_OFFSET] & SFF8636_O3_RX_LOL)
		module_show_lane_status("Rx loss of lock", 4, "Yes", "No", v);

	v = map->lower_memory[SFF8636_LOL_AW_OFFSET] >> 4;
	if (map->page_00h[SFF8636_OPTION_3_OFFSET] & SFF8636_O3_TX_LOL)
		module_show_lane_status("Tx loss of lock", 4, "Yes", "No", v);

	v = map->lower_memory[SFF8636_FAULT_AW_OFFSET] & 0xf;
	if (map->page_00h[SFF8636_OPTION_4_OFFSET] & SFF8636_O4_TX_FAULT)
		module_show_lane_status("Tx fault", 4, "Yes", "No", v);

	v = map->lower_memory[SFF8636_FAULT_AW_OFFSET] >> 4;
	if (map->page_00h[SFF8636_OPTION_2_OFFSET] & SFF8636_O2_TX_EQ_AUTO)
		module_show_lane_status("Tx adaptive eq fault", 4, "Yes", "No",
					v);
}

static void sff8636_show_page_zero(const struct sff8636_memory_map *map)
{
	sff8636_show_ext_identifier(map);
	sff8636_show_connector(map);
	sff8636_show_transceiver(map);
	sff8636_show_ext_module_codes(map);
	sff8636_show_encoding(map);
	if (map->page_00h[SFF8636_BR_NOMINAL_OFFSET] == 0xFF)
		module_print_any_uint("BR Nominal",
				      map->page_00h[SFF8636_EXT_BAUD_RATE_OFFSET] * 250,
				      " Mbps");
	else
		module_show_value_with_unit(map->page_00h,
					    SFF8636_BR_NOMINAL_OFFSET,
					    "BR Nominal", 100, "Mbps");
	sff8636_show_rate_identifier(map);
	module_show_value_with_unit(map->page_00h, SFF8636_SM_LEN_OFFSET,
				    "Length (SMF)", 1, "km");
	module_show_value_with_unit(map->page_00h, SFF8636_OM3_LEN_OFFSET,
				    "Length (OM3)", 2, "m");
	module_show_value_with_unit(map->page_00h, SFF8636_OM2_LEN_OFFSET,
				    "Length (OM2)", 1, "m");
	module_show_value_with_unit(map->page_00h, SFF8636_OM1_LEN_OFFSET,
				    "Length (OM1)", 1, "m");
	module_show_value_with_unit(map->page_00h, SFF8636_CBL_LEN_OFFSET,
				    "Length (Copper or Active cable)", 1, "m");
	sff8636_show_wavelength_or_copper_compliance(map);
	module_show_ascii(map->page_00h, SFF8636_VENDOR_NAME_START_OFFSET,
			  SFF8636_VENDOR_NAME_END_OFFSET, "Vendor name");
	module_show_oui(map->page_00h, SFF8636_VENDOR_OUI_OFFSET);
	module_show_ascii(map->page_00h, SFF8636_VENDOR_PN_START_OFFSET,
			  SFF8636_VENDOR_PN_END_OFFSET, "Vendor PN");
	module_show_ascii(map->page_00h, SFF8636_VENDOR_REV_START_OFFSET,
			  SFF8636_VENDOR_REV_END_OFFSET, "Vendor rev");
	module_show_ascii(map->page_00h, SFF8636_VENDOR_SN_START_OFFSET,
			  SFF8636_VENDOR_SN_END_OFFSET, "Vendor SN");
	module_show_date_code(map->page_00h, SFF8636_DATE_YEAR_OFFSET);
	sff8636_show_revision_compliance(map->lower_memory,
					 SFF8636_REV_COMPLIANCE_OFFSET);
	sff8636_show_max_case_temp(map);
	sff8636_show_checksums(map);
	sff8636_show_signals(map);
}

static void sff8636_show_all_common(const struct sff8636_memory_map *map)
{
	sff8636_show_identifier(map);
	switch (map->lower_memory[SFF8636_ID_OFFSET]) {
	case MODULE_ID_QSFP:
	case MODULE_ID_QSFP_PLUS:
	case MODULE_ID_QSFP28:
		sff8636_show_page_zero(map);
		sff8636_show_dom(map);
		break;
	}
}

static void sff8636_memory_map_init_buf(struct sff8636_memory_map *map,
					const __u8 *id, __u32 eeprom_len)
{
	/* Lower Memory and Page 00h are always present.
	 *
	 * Offset into Upper Memory is between page size and twice the page
	 * size. Therefore, set the base address of each page to base address
	 * plus page size multiplied by the page number.
	 */
	map->lower_memory = id;
	map->page_00h = id;

	/* Page 03h is only present when the module memory model is paged and
	 * not flat and when we got a big enough buffer from the kernel.
	 */
	if (map->lower_memory[SFF8636_STATUS_2_OFFSET] &
	    SFF8636_STATUS_PAGE_3_PRESENT ||
	    eeprom_len != ETH_MODULE_SFF_8636_MAX_LEN)
		return;

	map->page_03h = id + 3 * SFF8636_PAGE_SIZE;
}

void sff8636_show_all_ioctl(const __u8 *id, __u32 eeprom_len)
{
	struct sff8636_memory_map map = {};

	switch (id[SFF8636_ID_OFFSET]) {
	case MODULE_ID_QSFP_DD:
	case MODULE_ID_OSFP:
	case MODULE_ID_DSFP:
	case MODULE_ID_QSFP_PLUS_CMIS:
	case MODULE_ID_SFP_DD_CMIS:
	case MODULE_ID_SFP_PLUS_CMIS:
		cmis_show_all_ioctl(id);
		break;
	default:
		sff8636_memory_map_init_buf(&map, id, eeprom_len);
		sff8636_show_all_common(&map);
		break;
	}
}

static void sff8636_request_init(struct module_eeprom *request, u8 page,
				 u32 offset)
{
	request->offset = offset;
	request->length = SFF8636_PAGE_SIZE;
	request->page = page;
	request->bank = 0;
	request->i2c_address = SFF8636_I2C_ADDRESS;
	request->data = NULL;
}

static int
sff8636_memory_map_init_pages(struct cmd_context *ctx,
			      struct sff8636_memory_map *map)
{
	struct module_eeprom request;
	int ret;

	/* Lower Memory and Page 00h are always present.
	 *
	 * Offset into Upper Memory is between page size and twice the page
	 * size. Therefore, set the base address of each page to its base
	 * address minus page size.
	 */
	sff8636_request_init(&request, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map->lower_memory = request.data;

	sff8636_request_init(&request, 0x0, SFF8636_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map->page_00h = request.data - SFF8636_PAGE_SIZE;

	/* Page 03h is only present when the module memory model is paged and
	 * not flat.
	 */
	if (map->lower_memory[SFF8636_STATUS_2_OFFSET] &
	    SFF8636_STATUS_PAGE_3_PRESENT)
		return 0;

	sff8636_request_init(&request, 0x3, SFF8636_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0) {
		/* Page 03h is not available due to a bug in the driver.
		 * This is a non-fatal error and sff8636_dom_parse()
		 * handles this correctly.
		 */
		fprintf(stderr, "Failed to read Upper Page 03h, driver error?\n");
		return 0;
	}

	map->page_03h = request.data - SFF8636_PAGE_SIZE;

	return 0;
}

int sff8636_show_all_nl(struct cmd_context *ctx)
{
	struct sff8636_memory_map map = {};
	int ret;

	new_json_obj(ctx->json);
	open_json_object(NULL);

	ret = sff8636_memory_map_init_pages(ctx, &map);
	if (ret < 0)
		return ret;
	sff8636_show_all_common(&map);

	close_json_object();
	delete_json_obj();

	return 0;
}
