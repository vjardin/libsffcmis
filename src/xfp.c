/*
 * xfp.c: Implements INF-8077i Rev 4.5 XFP module parsing.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 *
 * XFP modules use a single I2C address (0x50) with a page-select byte
 * at offset 0x7F.  Lower memory (0x00-0x7F) contains diagnostics;
 * upper memory (0x80-0xFF) is table-selected, defaulting to Table 01h
 * (Serial ID).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "module-common.h"
#include "i2c.h"
#include "sffcmis.h"
#include "xfp.h"

/* Helper: 16-bit big-endian read from a byte buffer */
#define XFP_U16(buf, off)  ((__u16)((buf)[(off)] << 8 | (buf)[(off) + 1]))
#define XFP_S16(buf, off)  ((__s16)XFP_U16(buf, off))

/*-----------------------------------------------------------------------
 * Raw hex dump of a 128-byte memory region
 *-----------------------------------------------------------------------*/
static void xfp_hex_dump(const __u8 *buf, unsigned int base_offset,
			  const char *label)
{
	if (is_json_context()) {
		char json_fn[100];

		convert_json_field_name(label, json_fn);
		open_json_array(json_fn, "");
		for (unsigned int i = 0; i < XFP_PAGE_SIZE; i++)
			print_uint(PRINT_JSON, NULL, "%u", buf[i]);
		close_json_array("");
		return;
	}

	printf("\n\t%s:\n", label);
	for (unsigned int row = 0; row < XFP_PAGE_SIZE; row += 16) {
		printf("\t  %04x: ", base_offset + row);
		for (unsigned int col = 0; col < 16; col++) {
			if (col == 8)
				printf(" ");
			printf(" %02x", buf[row + col]);
		}
		printf("  |");
		for (unsigned int col = 0; col < 16; col++) {
			unsigned char c = buf[row + col];

			putchar(isprint(c) ? c : '.');
		}
		printf("|\n");
	}
}

/*-----------------------------------------------------------------------
 * Extended Identifier (byte 129, Table 47)
 *-----------------------------------------------------------------------*/
static void xfp_show_ext_identifier(const __u8 *id)
{
	__u8 val = id[XFP_EXT_ID];
	unsigned int power_level = (val >> XFP_EXT_ID_POWER_SHIFT) & 0x03;
	const char *power_str;

	switch (power_level) {
	case 0:  power_str = "Power Level 1 (1.5 W max)"; break;
	case 1:  power_str = "Power Level 2 (2.5 W max)"; break;
	case 2:  power_str = "Power Level 3 (3.5 W max)"; break;
	case 3:  power_str = "Power Level 4 (>3.5 W max)"; break;
	default: power_str = "unknown"; break;
	}

	sff_print_any_hex_field("Extended identifier", "extended_identifier",
				val, power_str);

	module_print_any_bool("CDR",
			      "cdr",
			      !(val & XFP_EXT_ID_CDR),
			      (val & XFP_EXT_ID_CDR) ? "No" : "Yes");

	module_print_any_bool("CLEI code present",
			      "clei_code_present",
			      !!(val & XFP_EXT_ID_CLEI),
			      (val & XFP_EXT_ID_CLEI) ? "Yes" : "No");
}

/*-----------------------------------------------------------------------
 * Transceiver Codes (bytes 131-138, Table 49)
 * XFP-specific — NOT compatible with SFP/SFF-8472.
 *-----------------------------------------------------------------------*/
static void xfp_show_transceiver(const __u8 *id)
{
	static const char *pfx = "Transceiver type";

	if (is_json_context()) {
		open_json_array("transceiver_codes", "");
		for (int i = XFP_TRANSCEIVER_START; i <= XFP_TRANSCEIVER_END; i++)
			print_uint(PRINT_JSON, NULL, "%u", id[i]);
		close_json_array("");
	} else {
		printf("\t%-41s : 0x%02x 0x%02x 0x%02x "
		       "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		       "Transceiver codes",
		       id[3], id[4], id[5], id[6],
		       id[7], id[8], id[9], id[10]);
	}

	/* Byte 131 (id[3]): 10 Gigabit Ethernet Compliance */
	if (id[3] & (1 << 7))
		module_print_any_string(pfx, "10GBASE-SR");
	if (id[3] & (1 << 6))
		module_print_any_string(pfx, "10GBASE-LR");
	if (id[3] & (1 << 5))
		module_print_any_string(pfx, "10GBASE-ER");
	if (id[3] & (1 << 4))
		module_print_any_string(pfx, "10GBASE-LRM");
	if (id[3] & (1 << 3))
		module_print_any_string(pfx, "10GBASE-SW");
	if (id[3] & (1 << 2))
		module_print_any_string(pfx, "10GBASE-LW");
	if (id[3] & (1 << 1))
		module_print_any_string(pfx, "10GBASE-EW");

	/* Byte 132 (id[4]): 10 Gigabit Fibre Channel Compliance */
	if (id[4] & (1 << 7))
		module_print_any_string(pfx, "10GFC: 1200-MX-SN-I");
	if (id[4] & (1 << 6))
		module_print_any_string(pfx, "10GFC: 1200-SM-LL-L");
	if (id[4] & (1 << 5))
		module_print_any_string(pfx, "10GFC: Extended Reach 1550 nm");
	if (id[4] & (1 << 4))
		module_print_any_string(pfx, "10GFC: Intermediate Reach 1300 nm FP");

	/* Byte 133 (id[5]): 10 Gigabit Copper Links — reserved in rev 4.5 */

	/* Byte 134 (id[6]): Lower Speed Links */
	if (id[6] & (1 << 7))
		module_print_any_string(pfx, "1000BASE-SX / 1xFC MMF");
	if (id[6] & (1 << 6))
		module_print_any_string(pfx, "1000BASE-LX / 1xFC SMF");
	if (id[6] & (1 << 5))
		module_print_any_string(pfx, "2xFC MMF");
	if (id[6] & (1 << 4))
		module_print_any_string(pfx, "2xFC SMF");
	if (id[6] & (1 << 3))
		module_print_any_string(pfx, "OC-48 SR");
	if (id[6] & (1 << 2))
		module_print_any_string(pfx, "OC-48 IR");
	if (id[6] & (1 << 1))
		module_print_any_string(pfx, "OC-48 LR");

	/* Byte 135 (id[7]): SONET/SDH Interconnect */
	if (id[7] & (1 << 7))
		module_print_any_string(pfx, "SONET: I-64.1r");
	if (id[7] & (1 << 6))
		module_print_any_string(pfx, "SONET: I-64.1");
	if (id[7] & (1 << 5))
		module_print_any_string(pfx, "SONET: I-64.2r");
	if (id[7] & (1 << 4))
		module_print_any_string(pfx, "SONET: I-64.2");
	if (id[7] & (1 << 3))
		module_print_any_string(pfx, "SONET: I-64.3");
	if (id[7] & (1 << 2))
		module_print_any_string(pfx, "SONET: I-64.5");

	/* Byte 136 (id[8]): SONET/SDH Short Haul */
	if (id[8] & (1 << 7))
		module_print_any_string(pfx, "SONET: S-64.1");
	if (id[8] & (1 << 6))
		module_print_any_string(pfx, "SONET: S-64.2a");
	if (id[8] & (1 << 5))
		module_print_any_string(pfx, "SONET: S-64.2b");
	if (id[8] & (1 << 4))
		module_print_any_string(pfx, "SONET: S-64.3a");
	if (id[8] & (1 << 3))
		module_print_any_string(pfx, "SONET: S-64.3b");
	if (id[8] & (1 << 2))
		module_print_any_string(pfx, "SONET: S-64.5a");
	if (id[8] & (1 << 1))
		module_print_any_string(pfx, "SONET: S-64.5b");

	/* Byte 137 (id[9]): SONET/SDH Long Haul */
	if (id[9] & (1 << 7))
		module_print_any_string(pfx, "SONET: L-64.1");
	if (id[9] & (1 << 6))
		module_print_any_string(pfx, "SONET: L-64.2a");
	if (id[9] & (1 << 5))
		module_print_any_string(pfx, "SONET: L-64.2b");
	if (id[9] & (1 << 4))
		module_print_any_string(pfx, "SONET: L-64.2c");
	if (id[9] & (1 << 3))
		module_print_any_string(pfx, "SONET: L-64.3");
	if (id[9] & (1 << 2))
		module_print_any_string(pfx, "G.959.1 P1L1-2D2");

	/* Byte 138 (id[10]): SONET/SDH Very Long Haul */
	if (id[10] & (1 << 7))
		module_print_any_string(pfx, "SONET: V-64.2a");
	if (id[10] & (1 << 6))
		module_print_any_string(pfx, "SONET: V-64.2b");
	if (id[10] & (1 << 5))
		module_print_any_string(pfx, "SONET: V-64.3");

	/* If all transceiver code bytes are zero, note it */
	{
		bool all_zero = true;

		for (int i = XFP_TRANSCEIVER_START; i <= XFP_TRANSCEIVER_END; i++) {
			if (id[i]) {
				all_zero = false;
				break;
			}
		}
		if (all_zero)
			module_print_any_string(pfx,
				"[unspecified - vendor specific application]");
	}
}

/*-----------------------------------------------------------------------
 * Encoding (byte 139, Table 50) — bit-significant
 *-----------------------------------------------------------------------*/
static void xfp_show_encoding(const __u8 *id)
{
	__u8 enc = id[XFP_ENCODING];
	char desc[SFF_MAX_DESC_LEN] = "";
	int pos = 0;

	static const struct {
		__u8 bit;
		const char *name;
	} enc_bits[] = {
		{ 7, "64B/66B" },
		{ 6, "8B/10B" },
		{ 5, "SONET Scrambled" },
		{ 4, "NRZ" },
		{ 3, "RZ" },
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(enc_bits); i++) {
		if (enc & (1 << enc_bits[i].bit)) {
			if (pos)
				pos += snprintf(desc + pos,
						sizeof(desc) - pos, ", ");
			pos += snprintf(desc + pos, sizeof(desc) - pos,
					"%s", enc_bits[i].name);
		}
	}

	if (!pos)
		snprintf(desc, sizeof(desc), "unspecified");

	sff_print_any_hex_field("Encoding", "encoding", enc, desc);
}

/*-----------------------------------------------------------------------
 * Wavelength (bytes 186-187, 188-189)
 *-----------------------------------------------------------------------*/
static void xfp_show_wavelength(const __u8 *id)
{
	unsigned int wl_raw = OFFSET_TO_U16_PTR(id, XFP_WAVELENGTH);
	unsigned int tol_raw = OFFSET_TO_U16_PTR(id, XFP_WAVELENGTH_TOL);

	/* Wavelength = value / 20 nm */
	module_print_any_float("Laser wavelength",
			       (float)wl_raw / 20.0f, " nm");

	/* Wavelength tolerance = value / 200 nm */
	module_print_any_float("Laser wavelength tolerance (+/-)",
			       (float)tol_raw / 200.0f, " nm");
}

/*-----------------------------------------------------------------------
 * Diagnostic Monitoring Type (byte 220, Table 56)
 *-----------------------------------------------------------------------*/
static void xfp_show_diag_mon_type(const __u8 *id)
{
	__u8 val = id[XFP_DIAG_MON_TYPE];

	sff_print_any_hex_field("Diagnostic monitoring type",
				"diagnostic_monitoring_type", val, NULL);

	module_print_any_bool("BER support",
			      "ber_support",
			      !!(val & (1 << 4)),
			      (val & (1 << 4)) ? "Yes" : "No");

	module_print_any_bool("RX power measurement",
			      "rx_power_measurement",
			      !!(val & (1 << 3)),
			      (val & (1 << 3)) ? "Average power" : "OMA");
}

/*-----------------------------------------------------------------------
 * Enhanced Options (byte 221, Table 57)
 *-----------------------------------------------------------------------*/
static void xfp_show_enhanced_options(const __u8 *id)
{
	__u8 val = id[XFP_ENHANCED_OPTIONS];

	sff_print_any_hex_field("Enhanced options",
				"enhanced_options", val, NULL);

	static const struct {
		__u8 bit;
		const char *name;
		const char *json_name;
	} opts[] = {
		{ 7, "VPS support",               "vps_support" },
		{ 6, "Soft TX_DISABLE",            "soft_tx_disable" },
		{ 5, "Soft P_down",                "soft_p_down" },
		{ 4, "VPS LV regulator mode",      "vps_lv_regulator_mode" },
		{ 3, "VPS bypassed regulator mode", "vps_bypassed_regulator_mode" },
		{ 2, "Active FEC control",          "active_fec_control" },
		{ 1, "Wavelength tunability",       "wavelength_tunability" },
		{ 0, "CMU support",                 "cmu_support" },
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(opts); i++) {
		bool set = !!(val & (1 << opts[i].bit));

		module_print_any_bool(opts[i].name,
				      (char *)opts[i].json_name,
				      set, set ? "Yes" : "No");
	}
}

/*-----------------------------------------------------------------------
 * AUX Monitoring (byte 222, Tables 58-59)
 *-----------------------------------------------------------------------*/
static const char *xfp_aux_type_name(unsigned int type)
{
	switch (type) {
	case 0:  return "Not implemented";
	case 1:  return "APD bias voltage";
	case 2:  return "Reserved";
	case 3:  return "TEC current";
	case 4:  return "Laser temperature";
	case 5:  return "Laser wavelength";
	case 6:  return "+5V supply voltage";
	case 7:  return "+3.3V supply voltage";
	case 8:  return "+1.8V supply voltage";
	case 9:  return "-5.2V supply voltage";
	case 10: return "+5V supply current";
	case 11: return "Reserved";
	case 12: return "Reserved";
	case 13: return "+3.3V supply current";
	case 14: return "Reserved";
	case 15: return "Reserved";
	default: return "Unknown";
	}
}

static void xfp_show_aux_monitoring(const __u8 *id)
{
	__u8 val = id[XFP_AUX_MONITORING];
	unsigned int aux1 = (val >> 4) & 0x0F;
	unsigned int aux2 = val & 0x0F;
	char desc[SFF_MAX_DESC_LEN];

	snprintf(desc, sizeof(desc), "AUX1=%s, AUX2=%s",
		 xfp_aux_type_name(aux1), xfp_aux_type_name(aux2));

	sff_print_any_hex_field("AUX monitoring",
				"aux_monitoring", val, desc);

	if (is_json_context()) {
		print_string(PRINT_JSON, "aux1_type", "%s",
			     xfp_aux_type_name(aux1));
		print_string(PRINT_JSON, "aux2_type", "%s",
			     xfp_aux_type_name(aux2));
	}
}

/*-----------------------------------------------------------------------
 * Serial ID display (upper memory, Table 01h)
 *-----------------------------------------------------------------------*/
static void xfp_show_serial_id(const __u8 *id)
{
	module_show_identifier(id, XFP_ID_UPPER);
	xfp_show_ext_identifier(id);
	module_show_connector(id, XFP_CONNECTOR);
	xfp_show_transceiver(id);

	/* Encoding — XFP Table 50 is bit-significant, not value-based */
	xfp_show_encoding(id);

	/* Bit rate: min/max in units of 100 Mbits/s */
	module_print_any_uint("BR Min", id[XFP_BR_MIN] * 100, " MBd");
	module_print_any_uint("BR Max", id[XFP_BR_MAX] * 100, " MBd");

	/* Link lengths */
	module_show_value_with_unit(id, XFP_LENGTH_SMF_KM,
				    "Length (SMF)", 1, "km");
	module_show_value_with_unit(id, XFP_LENGTH_EBW_50UM,
				    "Length (EBW 50/125 um)", 2, "m");
	module_show_value_with_unit(id, XFP_LENGTH_50UM,
				    "Length (50/125 um)", 1, "m");
	module_show_value_with_unit(id, XFP_LENGTH_625UM,
				    "Length (62.5/125 um)", 1, "m");
	module_show_value_with_unit(id, XFP_LENGTH_COPPER,
				    "Length (Copper)", 1, "m");

	xfp_show_wavelength(id);

	/* Max case temperature */
	module_print_any_uint("Max case temperature", id[XFP_MAX_CASE_TEMP],
			      " degrees C");

	/* Vendor fields */
	module_show_ascii(id, XFP_VENDOR_NAME_START, XFP_VENDOR_NAME_END,
			  "Vendor name");
	module_show_oui(id, XFP_VENDOR_OUI);
	module_show_ascii(id, XFP_VENDOR_PN_START, XFP_VENDOR_PN_END,
			  "Vendor PN");
	module_show_ascii(id, XFP_VENDOR_REV_START, XFP_VENDOR_REV_END,
			  "Vendor rev");
	module_show_ascii(id, XFP_VENDOR_SN_START, XFP_VENDOR_SN_END,
			  "Vendor SN");
	module_show_date_code(id, XFP_DATE_CODE_START);

	/* Bytes 220-222: diagnostic capabilities and AUX inputs */
	xfp_show_diag_mon_type(id);
	xfp_show_enhanced_options(id);
	xfp_show_aux_monitoring(id);
}

/*-----------------------------------------------------------------------
 * Alarm/warning flags (Table 39, lower memory bytes 80-83)
 *-----------------------------------------------------------------------*/
static const struct {
	const char *str;
	int offset;		/* byte offset within lower memory */
	__u8 mask;
} xfp_aw_flags[] = {
	{ "Laser bias current high alarm",   80, XFP_BIAS_HALRM_FLG },
	{ "Laser bias current low alarm",    80, XFP_BIAS_LALRM_FLG },
	{ "Laser output power high alarm",   80, XFP_TX_PWR_HALRM_FLG },
	{ "Laser output power low alarm",    80, XFP_TX_PWR_LALRM_FLG },

	{ "Module temperature high alarm",   80, XFP_TEMP_HALRM_FLG },
	{ "Module temperature low alarm",    80, XFP_TEMP_LALRM_FLG },

	{ "Laser rx power high alarm",       81, XFP_RX_PWR_HALRM_FLG },
	{ "Laser rx power low alarm",        81, XFP_RX_PWR_LALRM_FLG },

	{ "Laser bias current high warning", 82, XFP_BIAS_HWARN_FLG },
	{ "Laser bias current low warning",  82, XFP_BIAS_LWARN_FLG },
	{ "Laser output power high warning", 82, XFP_TX_PWR_HWARN_FLG },
	{ "Laser output power low warning",  82, XFP_TX_PWR_LWARN_FLG },

	{ "Module temperature high warning", 82, XFP_TEMP_HWARN_FLG },
	{ "Module temperature low warning",  82, XFP_TEMP_LWARN_FLG },

	{ "Laser rx power high warning",     83, XFP_RX_PWR_HWARN_FLG },
	{ "Laser rx power low warning",      83, XFP_RX_PWR_LWARN_FLG },

	{ NULL, 0, 0 },
};

/*-----------------------------------------------------------------------
 * Thresholds — like sff_show_thresholds but without VCC (not in XFP)
 *-----------------------------------------------------------------------*/
static void xfp_show_thresholds(struct sff_diags sd)
{
	if (is_json_context()) {
		open_json_object("laser_bias_current");
		PRINT_BIAS_JSON("high_alarm_threshold", sd.bias_cur[HALRM]);
		PRINT_BIAS_JSON("low_alarm_threshold", sd.bias_cur[LALRM]);
		PRINT_BIAS_JSON("high_warning_threshold", sd.bias_cur[HWARN]);
		PRINT_BIAS_JSON("low_warning_threshold", sd.bias_cur[LWARN]);
		close_json_object();

		open_json_object("laser_output_power");
		PRINT_xX_PWR_JSON("high_alarm_threshold", sd.tx_power[HALRM]);
		PRINT_xX_PWR_JSON("low_alarm_threshold", sd.tx_power[LALRM]);
		PRINT_xX_PWR_JSON("high_warning_threshold", sd.tx_power[HWARN]);
		PRINT_xX_PWR_JSON("low_warning_threshold", sd.tx_power[LWARN]);
		close_json_object();

		open_json_object("module_temperature");
		PRINT_TEMP_JSON("high_alarm_threshold", sd.sfp_temp[HALRM]);
		PRINT_TEMP_JSON("low_alarm_threshold", sd.sfp_temp[LALRM]);
		PRINT_TEMP_JSON("high_warning_threshold", sd.sfp_temp[HWARN]);
		PRINT_TEMP_JSON("low_warning_threshold", sd.sfp_temp[LWARN]);
		close_json_object();

		open_json_object("laser_rx_power");
		PRINT_xX_PWR_JSON("high_alarm_threshold", sd.rx_power[HALRM]);
		PRINT_xX_PWR_JSON("low_alarm_threshold", sd.rx_power[LALRM]);
		PRINT_xX_PWR_JSON("high_warning_threshold", sd.rx_power[HWARN]);
		PRINT_xX_PWR_JSON("low_warning_threshold", sd.rx_power[LWARN]);
		close_json_object();
	} else {
		PRINT_BIAS("Laser bias current high alarm threshold",
			   sd.bias_cur[HALRM]);
		PRINT_BIAS("Laser bias current low alarm threshold",
			   sd.bias_cur[LALRM]);
		PRINT_BIAS("Laser bias current high warning threshold",
			   sd.bias_cur[HWARN]);
		PRINT_BIAS("Laser bias current low warning threshold",
			   sd.bias_cur[LWARN]);

		PRINT_xX_PWR("Laser output power high alarm threshold",
			      sd.tx_power[HALRM]);
		PRINT_xX_PWR("Laser output power low alarm threshold",
			      sd.tx_power[LALRM]);
		PRINT_xX_PWR("Laser output power high warning threshold",
			      sd.tx_power[HWARN]);
		PRINT_xX_PWR("Laser output power low warning threshold",
			      sd.tx_power[LWARN]);

		PRINT_TEMP("Module temperature high alarm threshold",
			   sd.sfp_temp[HALRM]);
		PRINT_TEMP("Module temperature low alarm threshold",
			   sd.sfp_temp[LALRM]);
		PRINT_TEMP("Module temperature high warning threshold",
			   sd.sfp_temp[HWARN]);
		PRINT_TEMP("Module temperature low warning threshold",
			   sd.sfp_temp[LWARN]);

		PRINT_xX_PWR("Laser rx power high alarm threshold",
			      sd.rx_power[HALRM]);
		PRINT_xX_PWR("Laser rx power low alarm threshold",
			      sd.rx_power[LALRM]);
		PRINT_xX_PWR("Laser rx power high warning threshold",
			      sd.rx_power[HWARN]);
		PRINT_xX_PWR("Laser rx power low warning threshold",
			      sd.rx_power[LWARN]);
	}
}

/*-----------------------------------------------------------------------
 * Diagnostics (lower memory: A/D bytes 96-109, thresholds 2-41)
 *-----------------------------------------------------------------------*/
static void xfp_show_diagnostics(const __u8 *lower, const __u8 *upper)
{
	struct sff_diags sd = { 0 };

	sd.supports_dom = 1;
	sd.supports_alarms = 1;
	sd.rx_power_type = !!(upper[XFP_DIAG_MON_TYPE] & XFP_DIAG_RX_PWR_TYPE);

	/* Current A/D readouts (Table 41) */
	sd.sfp_temp[MCURR]  = XFP_S16(lower, XFP_AD_TEMP);
	sd.bias_cur[MCURR]  = XFP_U16(lower, XFP_AD_BIAS);
	sd.tx_power[MCURR]  = XFP_U16(lower, XFP_AD_TX_PWR);
	sd.rx_power[MCURR]  = XFP_U16(lower, XFP_AD_RX_PWR);

	/* Thresholds (Table 35) */
	sd.sfp_temp[HALRM]  = XFP_S16(lower, XFP_TEMP_HALRM);
	sd.sfp_temp[LALRM]  = XFP_S16(lower, XFP_TEMP_LALRM);
	sd.sfp_temp[HWARN]  = XFP_S16(lower, XFP_TEMP_HWARN);
	sd.sfp_temp[LWARN]  = XFP_S16(lower, XFP_TEMP_LWARN);

	sd.bias_cur[HALRM]  = XFP_U16(lower, XFP_BIAS_HALRM);
	sd.bias_cur[LALRM]  = XFP_U16(lower, XFP_BIAS_LALRM);
	sd.bias_cur[HWARN]  = XFP_U16(lower, XFP_BIAS_HWARN);
	sd.bias_cur[LWARN]  = XFP_U16(lower, XFP_BIAS_LWARN);

	sd.tx_power[HALRM]  = XFP_U16(lower, XFP_TX_PWR_HALRM);
	sd.tx_power[LALRM]  = XFP_U16(lower, XFP_TX_PWR_LALRM);
	sd.tx_power[HWARN]  = XFP_U16(lower, XFP_TX_PWR_HWARN);
	sd.tx_power[LWARN]  = XFP_U16(lower, XFP_TX_PWR_LWARN);

	sd.rx_power[HALRM]  = XFP_U16(lower, XFP_RX_PWR_HALRM);
	sd.rx_power[LALRM]  = XFP_U16(lower, XFP_RX_PWR_LALRM);
	sd.rx_power[HWARN]  = XFP_U16(lower, XFP_RX_PWR_HWARN);
	sd.rx_power[LWARN]  = XFP_U16(lower, XFP_RX_PWR_LWARN);

	/* Current readings */
	PRINT_BIAS_ALL("Laser bias current", "laser_tx_bias_current",
		       sd.bias_cur[MCURR]);
	PRINT_xX_PWR_ALL("Laser output power", "transmit_avg_optical_power",
			  sd.tx_power[MCURR]);

	if (!sd.rx_power_type) {
		open_json_object("rx_power");
		PRINT_xX_PWR_ALL("Receiver signal OMA", "value",
				  sd.rx_power[MCURR]);
		if (is_json_context())
			module_print_any_string("type", "Receiver signal OMA");
		close_json_object();
	} else {
		open_json_object("rx_power");
		PRINT_xX_PWR_ALL("Receiver signal average optical power",
				  "value", sd.rx_power[MCURR]);
		if (is_json_context())
			module_print_any_string("type",
				"Receiver signal average optical power");
		close_json_object();
	}

	/* XFP has no VCC in A/D area (bytes 98-99 reserved), show temp only */
	PRINT_TEMP_ALL("Module temperature", "module_temperature_measurement",
		       sd.sfp_temp[MCURR]);

	/* Alarm/warning flags */
	for (int i = 0; xfp_aw_flags[i].str; i++) {
		bool value = lower[xfp_aw_flags[i].offset] &
			     xfp_aw_flags[i].mask;
		module_print_any_bool(xfp_aw_flags[i].str, NULL,
				      value, ONOFF(value));
	}

	/* Thresholds — XFP has no VCC thresholds (bytes 10-17 reserved) */
	xfp_show_thresholds(sd);
}

/*-----------------------------------------------------------------------
 * EEPROM page reader
 *-----------------------------------------------------------------------*/
static int xfp_get_eeprom_page(struct cmd_context *ctx, __u8 page,
				__u8 *buf)
{
	struct module_eeprom request = {
		.offset = XFP_PAGE_SIZE,
		.length = XFP_PAGE_SIZE,
		.page = page,
		.i2c_address = XFP_I2C_ADDRESS,
	};
	int ret;

	ret = get_eeprom_page(ctx, &request);
	if (!ret)
		memcpy(buf, request.data, XFP_PAGE_SIZE);

	return ret;
}

static int xfp_get_lower_memory(struct cmd_context *ctx, __u8 *buf)
{
	struct module_eeprom request = {
		.offset = 0,
		.length = XFP_PAGE_SIZE,
		.i2c_address = XFP_I2C_ADDRESS,
	};
	int ret;

	ret = get_eeprom_page(ctx, &request);
	if (!ret)
		memcpy(buf, request.data, XFP_PAGE_SIZE);

	return ret;
}

/*-----------------------------------------------------------------------
 * Entry point
 *-----------------------------------------------------------------------*/
int xfp_show_all_nl(struct cmd_context *ctx)
{
	__u8 lower[XFP_PAGE_SIZE];
	__u8 upper[XFP_PAGE_SIZE];
	int ret;

	/* Read lower memory (bytes 0x00-0x7F) — diagnostics, flags, A/D */
	ret = xfp_get_lower_memory(ctx, lower);
	if (ret)
		return ret;

	/* Read upper memory Table 01h (bytes 0x80-0xFF) — Serial ID */
	ret = xfp_get_eeprom_page(ctx, 0x01, upper);
	if (ret)
		return ret;

	new_json_obj(ctx->json);
	open_json_object(NULL);

	xfp_show_serial_id(upper);
	xfp_show_diagnostics(lower, upper);

	/* Raw memory dump */
	xfp_hex_dump(lower, 0x00, "Raw lower memory (0x00-0x7F)");
	xfp_hex_dump(upper, 0x80, "Raw upper memory (0x80-0xFF)");

	close_json_object();
	delete_json_obj();

	return 0;
}
