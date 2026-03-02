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

#define XFP_U16(buf, off)  ((__u16)((buf)[(off)] << 8 | (buf)[(off) + 1]))
#define XFP_S16(buf, off)  ((__s16)XFP_U16(buf, off))

static void xfp_hex_dump(const __u8 *buf, unsigned int base_offset,
			  const char *label)
{
	if (is_json_context()) {
		char json_fn[100] = "";

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

/* XFP transceiver codes are not compatible with SFP/SFF-8472 */
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

/* XFP encoding is bit-significant, not value-based like SFP */
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

static void xfp_show_device_tech(const __u8 *id)
{
	__u8 val = id[XFP_DEVICE_TECH];
	unsigned int tx_tech = (val & XFP_DEVTECH_TX_MASK) >> XFP_DEVTECH_TX_SHIFT;

	static const char *tx_tech_names[] = {
		[0]  = "850 nm VCSEL",
		[1]  = "1310 nm VCSEL",
		[2]  = "1550 nm VCSEL",
		[3]  = "1310 nm FP",
		[4]  = "1310 nm DFB",
		[5]  = "1550 nm DFB",
		[6]  = "1310 nm EML",
		[7]  = "1550 nm EML",
		[8]  = "Cu/AgC (no WDM)",
		[9]  = "1490 nm DFB",
		[10] = "Cu/AgC (WDM)",
	};

	const char *tx_str = (tx_tech < ARRAY_SIZE(tx_tech_names) &&
			      tx_tech_names[tx_tech])
			     ? tx_tech_names[tx_tech] : "Reserved";

	/* Build rich description: "TX tech, [Cooled, ][APD, ]..." */
	char desc[SFF_MAX_DESC_LEN];
	int pos = snprintf(desc, sizeof(desc), "TX: %s", tx_str);

	if (val & XFP_DEVTECH_WL_CTRL)
		pos += snprintf(desc + pos, sizeof(desc) - pos, ", WL ctrl");
	if (val & XFP_DEVTECH_COOLED)
		pos += snprintf(desc + pos, sizeof(desc) - pos, ", Cooled");
	if (val & XFP_DEVTECH_APD)
		pos += snprintf(desc + pos, sizeof(desc) - pos, ", APD");
	if (val & XFP_DEVTECH_TUNABLE)
		pos += snprintf(desc + pos, sizeof(desc) - pos, ", Tunable");

	sff_print_any_hex_field("Device technology",
				"device_technology", val, desc);

	module_print_any_bool("Wavelength control",
			      "wavelength_control",
			      !!(val & XFP_DEVTECH_WL_CTRL),
			      (val & XFP_DEVTECH_WL_CTRL) ? "Yes" : "No");

	module_print_any_bool("Cooled transmitter",
			      "cooled_transmitter",
			      !!(val & XFP_DEVTECH_COOLED),
			      (val & XFP_DEVTECH_COOLED) ? "Yes" : "No");

	module_print_any_bool("APD detector",
			      "apd_detector",
			      !!(val & XFP_DEVTECH_APD),
			      (val & XFP_DEVTECH_APD) ? "Yes" : "No");

	module_print_any_bool("Tunable transmitter",
			      "tunable_transmitter",
			      !!(val & XFP_DEVTECH_TUNABLE),
			      (val & XFP_DEVTECH_TUNABLE) ? "Yes" : "No");
}

static void xfp_show_cdr_support(const __u8 *id)
{
	__u8 val = id[XFP_CDR_SUPPORT];

	static const struct {
		__u8 bit;
		const char *name;
		const char *json_name;
	} cdr_bits[] = {
		{ 7, "9.95 Gb/s",          "9_95g" },
		{ 6, "10.3 Gb/s",          "10_3g" },
		{ 5, "10.5 Gb/s",          "10_5g" },
		{ 4, "10.7 Gb/s",          "10_7g" },
		{ 3, "11.1 Gb/s",          "11_1g" },
		{ 2, "Lineside loopback",   "lineside_loopback" },
		{ 1, "XFI loopback",        "xfi_loopback" },
	};

	char desc[SFF_MAX_DESC_LEN] = "";
	int pos = 0;

	for (unsigned int i = 0; i < ARRAY_SIZE(cdr_bits); i++) {
		if (val & (1 << cdr_bits[i].bit)) {
			if (pos)
				pos += snprintf(desc + pos,
						sizeof(desc) - pos, ", ");
			pos += snprintf(desc + pos, sizeof(desc) - pos,
					"%s", cdr_bits[i].name);
		}
	}

	sff_print_any_hex_field("CDR support", "cdr_support", val,
				pos ? desc : "None");

	for (unsigned int i = 0; i < ARRAY_SIZE(cdr_bits); i++) {
		bool set = !!(val & (1 << cdr_bits[i].bit));

		module_print_any_bool(cdr_bits[i].name,
				      (char *)cdr_bits[i].json_name,
				      set, set ? "Yes" : "No");
	}
}

static void xfp_show_power_supply(const __u8 *id)
{
	unsigned int pwr_max  = id[XFP_PWR_MAX] * 20;      /* mW */
	unsigned int pwr_down = id[XFP_PWR_MAX_PDOWN] * 10; /* mW */

	/* Byte 194: high nibble = +5V (x50mA), low = +3.3V (x100mA) */
	unsigned int i5v  = ((id[XFP_PWR_SUPPLY_CURRENT] >> 4) & 0x0F) * 50;
	unsigned int i3v3 = (id[XFP_PWR_SUPPLY_CURRENT] & 0x0F) * 100;
	/* Byte 195: high nibble = +1.8V (x100mA), low = -5.2V (x50mA) */
	unsigned int i1v8 = ((id[XFP_PWR_SUPPLY_CURRENT + 1] >> 4) & 0x0F) * 100;
	unsigned int in5v = (id[XFP_PWR_SUPPLY_CURRENT + 1] & 0x0F) * 50;

	open_json_object("power_supply");

	module_print_any_uint("Max total power", pwr_max, " mW");
	module_print_any_uint("Max P_Down power", pwr_down, " mW");
	module_print_any_uint("+5V supply current", i5v, " mA");
	module_print_any_uint("+3.3V supply current", i3v3, " mA");
	module_print_any_uint("+1.8V supply current", i1v8, " mA");
	module_print_any_uint("-5.2V supply current", in5v, " mA");

	close_json_object();
}

static void xfp_show_checksums(const __u8 *id)
{
	unsigned int cc_base_sum = 0;
	unsigned int cc_ext_sum = 0;

	/* CC_BASE: sum of bytes 128-190 (id[0]-id[62]) vs id[63] */
	for (int i = 0; i <= 62; i++)
		cc_base_sum += id[i];

	bool cc_base_ok = ((__u8)(cc_base_sum & 0xFF) == id[XFP_CC_BASE]);

	module_print_any_string("CC_BASE",
				cc_base_ok ? "pass" : "fail");

	/* CC_EXT: sum of bytes 192-222 (id[64]-id[94]) vs id[95] */
	for (int i = 64; i <= 94; i++)
		cc_ext_sum += id[i];

	bool cc_ext_ok = ((__u8)(cc_ext_sum & 0xFF) == id[XFP_CC_EXT]);

	module_print_any_string("CC_EXT",
				cc_ext_ok ? "pass" : "fail");
}

/* Some EPON XFP modules store additional wavelengths here */
static void xfp_show_vendor_wavelengths(const __u8 *id)
{
	unsigned int wl_25g = OFFSET_TO_U16_PTR(id, XFP_VENDOR_WL_25G);
	unsigned int wl_125g = OFFSET_TO_U16_PTR(id, XFP_VENDOR_WL_125G);

	open_json_object("vendor_wavelengths");

	if (wl_25g == 0x0000 || wl_25g == 0xFFFF)
		module_print_any_string("Wavelength 2.5G",
					wl_25g == 0xFFFF ? "unprogrammed" :
					"not supported");
	else
		module_print_any_float("Wavelength 2.5G",
				       (float)wl_25g / 20.0f, " nm");

	if (wl_125g == 0x0000 || wl_125g == 0xFFFF)
		module_print_any_string("Wavelength 1.25G",
					wl_125g == 0xFFFF ? "unprogrammed" :
					"not supported");
	else
		module_print_any_float("Wavelength 1.25G",
				       (float)wl_125g / 20.0f, " nm");

	close_json_object();
}

static const char *xfp_epon_wavelength_label(const __u8 *id, float wl_nm)
{
	for (int i = XFP_TRANSCEIVER_START; i <= XFP_TRANSCEIVER_END; i++) {
		if (id[i])
			return NULL;
	}

	if (wl_nm >= 1572.0f && wl_nm <= 1582.0f)
		return " nm (10G EPON downstream)";
	if (wl_nm >= 1485.0f && wl_nm <= 1495.0f)
		return " nm (1G EPON downstream)";

	return NULL;
}

static void xfp_show_wavelength(const __u8 *id)
{
	unsigned int wl_raw = OFFSET_TO_U16_PTR(id, XFP_WAVELENGTH);
	unsigned int tol_raw = OFFSET_TO_U16_PTR(id, XFP_WAVELENGTH_TOL);
	float wl_nm = (float)wl_raw / 20.0f;

	const char *unit = xfp_epon_wavelength_label(id, wl_nm);

	if (!unit)
		unit = " nm";

	/* Wavelength = value / 20 nm */
	module_print_any_float("Laser wavelength", wl_nm, unit);

	/* Wavelength tolerance = value / 200 nm */
	module_print_any_float("Laser wavelength tolerance (+/-)",
			       (float)tol_raw / 200.0f, " nm");
}

static void xfp_show_diag_mon_type(const __u8 *id)
{
	__u8 val = id[XFP_DIAG_MON_TYPE];

	char desc[SFF_MAX_DESC_LEN];
	int pos = snprintf(desc, sizeof(desc), "RX power: %s",
			   (val & (1 << 3)) ? "Average" : "OMA");

	if (val & (1 << 4))
		pos += snprintf(desc + pos, sizeof(desc) - pos, ", BER");

	sff_print_any_hex_field("Diagnostic monitoring type",
				"diagnostic_monitoring_type", val, desc);

	module_print_any_bool("BER support",
			      "ber_support",
			      !!(val & (1 << 4)),
			      (val & (1 << 4)) ? "Yes" : "No");

	module_print_any_bool("RX power measurement",
			      "rx_power_measurement",
			      !!(val & (1 << 3)),
			      (val & (1 << 3)) ? "Average power" : "OMA");
}

static void xfp_show_enhanced_options(const __u8 *id)
{
	__u8 val = id[XFP_ENHANCED_OPTIONS];

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

	char desc[SFF_MAX_DESC_LEN] = "";
	int pos = 0;

	for (unsigned int i = 0; i < ARRAY_SIZE(opts); i++) {
		if (val & (1 << opts[i].bit)) {
			if (pos)
				pos += snprintf(desc + pos,
						sizeof(desc) - pos, ", ");
			pos += snprintf(desc + pos, sizeof(desc) - pos,
					"%s", opts[i].name);
		}
	}

	sff_print_any_hex_field("Enhanced options",
				"enhanced_options", val,
				pos ? desc : "None");

	for (unsigned int i = 0; i < ARRAY_SIZE(opts); i++) {
		bool set = !!(val & (1 << opts[i].bit));

		module_print_any_bool(opts[i].name,
				      (char *)opts[i].json_name,
				      set, set ? "Yes" : "No");
	}
}

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

static void xfp_show_serial_id(const __u8 *id)
{
	module_show_identifier(id, XFP_ID_UPPER);
	xfp_show_ext_identifier(id);
	module_show_connector(id, XFP_CONNECTOR);
	xfp_show_transceiver(id);

	xfp_show_encoding(id);
	module_print_any_uint("BR Min", id[XFP_BR_MIN] * 100, " MBd");
	module_print_any_uint("BR Max", id[XFP_BR_MAX] * 100, " MBd");

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

	xfp_show_device_tech(id);
	xfp_show_cdr_support(id);
	xfp_show_wavelength(id);

	module_print_any_uint("Max case temperature", id[XFP_MAX_CASE_TEMP],
			      " degrees C");
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

	xfp_show_diag_mon_type(id);
	xfp_show_enhanced_options(id);
	xfp_show_aux_monitoring(id);
	xfp_show_power_supply(id);
	xfp_show_checksums(id);
	xfp_show_vendor_wavelengths(id);
}

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

		open_json_object("module_voltage");
		PRINT_VCC_JSON("high_alarm_threshold", sd.sfp_voltage[HALRM]);
		PRINT_VCC_JSON("low_alarm_threshold", sd.sfp_voltage[LALRM]);
		PRINT_VCC_JSON("high_warning_threshold", sd.sfp_voltage[HWARN]);
		PRINT_VCC_JSON("low_warning_threshold", sd.sfp_voltage[LWARN]);
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

		PRINT_VCC("Module voltage high alarm threshold",
			  sd.sfp_voltage[HALRM]);
		PRINT_VCC("Module voltage low alarm threshold",
			  sd.sfp_voltage[LALRM]);
		PRINT_VCC("Module voltage high warning threshold",
			  sd.sfp_voltage[HWARN]);
		PRINT_VCC("Module voltage low warning threshold",
			  sd.sfp_voltage[LWARN]);

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

static void xfp_show_ext_int_flags(const __u8 *lower)
{
	static const struct {
		const char *str;
		const char *json_name;
		int offset;
		__u8 mask;
	} flags[] = {
		{ "TX NR",              "tx_nr",              84, XFP_TX_NR_FLG },
		{ "TX Fault",           "tx_fault",           84, XFP_TX_FAULT_FLG },
		{ "TX CDR LOL",         "tx_cdr_lol",         84, XFP_TX_CDR_LOL_FLG },
		{ "RX NR",              "rx_nr",              84, XFP_RX_NR_FLG },
		{ "RX LOS",             "rx_los",             84, XFP_RX_LOS_FLG },
		{ "RX CDR LOL",         "rx_cdr_lol",         84, XFP_RX_CDR_LOL_FLG },
		{ "MOD NR",             "mod_nr",             84, XFP_MOD_NR_FLG },
		{ "Reset complete",     "reset_complete",     84, XFP_RESET_COMPLETE_FLG },
		{ "APD fault",          "apd_fault",          85, XFP_APD_FAULT_FLG },
		{ "TEC fault",          "tec_fault",          85, XFP_TEC_FAULT_FLG },
		{ "Wavelength unlocked", "wavelength_unlocked", 85, XFP_WL_UNLOCKED_FLG },
		{ "+5V high alarm",     "vcc5_high_alarm",    86, XFP_VCC5_HALRM_FLG },
		{ "+5V low alarm",      "vcc5_low_alarm",     86, XFP_VCC5_LALRM_FLG },
		{ "+3.3V high alarm",   "vcc3_high_alarm",    86, XFP_VCC3_HALRM_FLG },
		{ "+3.3V low alarm",    "vcc3_low_alarm",     86, XFP_VCC3_LALRM_FLG },
		{ "+1.8V high alarm",   "vcc2_high_alarm",    86, XFP_VCC2_HALRM_FLG },
		{ "+1.8V low alarm",    "vcc2_low_alarm",     86, XFP_VCC2_LALRM_FLG },
		{ "-5.2V high alarm",   "vee5_high_alarm",    86, XFP_VEE5_HALRM_FLG },
		{ "-5.2V low alarm",    "vee5_low_alarm",     86, XFP_VEE5_LALRM_FLG },
		{ "+5V high warning",   "vcc5_high_warning",  87, XFP_VCC5_HWARN_FLG },
		{ "+5V low warning",    "vcc5_low_warning",   87, XFP_VCC5_LWARN_FLG },
		{ "+3.3V high warning", "vcc3_high_warning",  87, XFP_VCC3_HWARN_FLG },
		{ "+3.3V low warning",  "vcc3_low_warning",   87, XFP_VCC3_LWARN_FLG },
		{ "+1.8V high warning", "vcc2_high_warning",  87, XFP_VCC2_HWARN_FLG },
		{ "+1.8V low warning",  "vcc2_low_warning",   87, XFP_VCC2_LWARN_FLG },
		{ "-5.2V high warning", "vee5_high_warning",  87, XFP_VEE5_HWARN_FLG },
		{ "-5.2V low warning",  "vee5_low_warning",   87, XFP_VEE5_LWARN_FLG },
		{ NULL, NULL, 0, 0 },
	};

	open_json_object("extended_interrupt_flags");

	for (int i = 0; flags[i].str; i++) {
		bool value = !!(lower[flags[i].offset] & flags[i].mask);

		module_print_any_bool(flags[i].str,
				      (char *)flags[i].json_name,
				      value, ONOFF(value));
	}

	close_json_object();
}

/* Same bit layout as flag bytes 80-87 */
static void xfp_show_int_masks(const __u8 *lower)
{
	static const struct {
		const char *str;
		const char *json_name;
		int offset;
		__u8 mask;
	} masks[] = {
		/* Byte 88 — masks for byte 80 */
		{ "Temp high alarm mask",       "temp_high_alarm",       88, XFP_TEMP_HALRM_FLG },
		{ "Temp low alarm mask",        "temp_low_alarm",        88, XFP_TEMP_LALRM_FLG },
		{ "Bias high alarm mask",       "bias_high_alarm",       88, XFP_BIAS_HALRM_FLG },
		{ "Bias low alarm mask",        "bias_low_alarm",        88, XFP_BIAS_LALRM_FLG },
		{ "TX power high alarm mask",   "tx_power_high_alarm",   88, XFP_TX_PWR_HALRM_FLG },
		{ "TX power low alarm mask",    "tx_power_low_alarm",    88, XFP_TX_PWR_LALRM_FLG },
		/* Byte 89 — masks for byte 81 */
		{ "RX power high alarm mask",   "rx_power_high_alarm",   89, XFP_RX_PWR_HALRM_FLG },
		{ "RX power low alarm mask",    "rx_power_low_alarm",    89, XFP_RX_PWR_LALRM_FLG },
		{ "AUX1 high alarm mask",       "aux1_high_alarm",       89, XFP_AUX1_HALRM_FLG },
		{ "AUX1 low alarm mask",        "aux1_low_alarm",        89, XFP_AUX1_LALRM_FLG },
		{ "AUX2 high alarm mask",       "aux2_high_alarm",       89, XFP_AUX2_HALRM_FLG },
		{ "AUX2 low alarm mask",        "aux2_low_alarm",        89, XFP_AUX2_LALRM_FLG },
		/* Byte 90 — masks for byte 82 */
		{ "Temp high warning mask",     "temp_high_warning",     90, XFP_TEMP_HWARN_FLG },
		{ "Temp low warning mask",      "temp_low_warning",      90, XFP_TEMP_LWARN_FLG },
		{ "Bias high warning mask",     "bias_high_warning",     90, XFP_BIAS_HWARN_FLG },
		{ "Bias low warning mask",      "bias_low_warning",      90, XFP_BIAS_LWARN_FLG },
		{ "TX power high warning mask", "tx_power_high_warning", 90, XFP_TX_PWR_HWARN_FLG },
		{ "TX power low warning mask",  "tx_power_low_warning",  90, XFP_TX_PWR_LWARN_FLG },
		/* Byte 91 — masks for byte 83 */
		{ "RX power high warning mask", "rx_power_high_warning", 91, XFP_RX_PWR_HWARN_FLG },
		{ "RX power low warning mask",  "rx_power_low_warning",  91, XFP_RX_PWR_LWARN_FLG },
		{ "AUX1 high warning mask",     "aux1_high_warning",     91, XFP_AUX1_HWARN_FLG },
		{ "AUX1 low warning mask",      "aux1_low_warning",      91, XFP_AUX1_LWARN_FLG },
		{ "AUX2 high warning mask",     "aux2_high_warning",     91, XFP_AUX2_HWARN_FLG },
		{ "AUX2 low warning mask",      "aux2_low_warning",      91, XFP_AUX2_LWARN_FLG },
		/* Byte 92 — masks for byte 84 */
		{ "TX NR mask",                 "tx_nr",                 92, XFP_TX_NR_FLG },
		{ "TX Fault mask",              "tx_fault",              92, XFP_TX_FAULT_FLG },
		{ "TX CDR LOL mask",            "tx_cdr_lol",            92, XFP_TX_CDR_LOL_FLG },
		{ "RX NR mask",                 "rx_nr",                 92, XFP_RX_NR_FLG },
		{ "RX LOS mask",               "rx_los",                92, XFP_RX_LOS_FLG },
		{ "RX CDR LOL mask",            "rx_cdr_lol",            92, XFP_RX_CDR_LOL_FLG },
		{ "MOD NR mask",                "mod_nr",                92, XFP_MOD_NR_FLG },
		{ "Reset complete mask",        "reset_complete",        92, XFP_RESET_COMPLETE_FLG },
		/* Byte 93 — masks for byte 85 */
		{ "APD fault mask",             "apd_fault",             93, XFP_APD_FAULT_FLG },
		{ "TEC fault mask",             "tec_fault",             93, XFP_TEC_FAULT_FLG },
		{ "Wavelength unlocked mask",   "wavelength_unlocked",   93, XFP_WL_UNLOCKED_FLG },
		/* Byte 94 — masks for byte 86 */
		{ "+5V high alarm mask",        "vcc5_high_alarm",       94, XFP_VCC5_HALRM_FLG },
		{ "+5V low alarm mask",         "vcc5_low_alarm",        94, XFP_VCC5_LALRM_FLG },
		{ "+3.3V high alarm mask",      "vcc3_high_alarm",       94, XFP_VCC3_HALRM_FLG },
		{ "+3.3V low alarm mask",       "vcc3_low_alarm",        94, XFP_VCC3_LALRM_FLG },
		{ "+1.8V high alarm mask",      "vcc2_high_alarm",       94, XFP_VCC2_HALRM_FLG },
		{ "+1.8V low alarm mask",       "vcc2_low_alarm",        94, XFP_VCC2_LALRM_FLG },
		{ "-5.2V high alarm mask",      "vee5_high_alarm",       94, XFP_VEE5_HALRM_FLG },
		{ "-5.2V low alarm mask",       "vee5_low_alarm",        94, XFP_VEE5_LALRM_FLG },
		/* Byte 95 — masks for byte 87 */
		{ "+5V high warning mask",      "vcc5_high_warning",     95, XFP_VCC5_HWARN_FLG },
		{ "+5V low warning mask",       "vcc5_low_warning",      95, XFP_VCC5_LWARN_FLG },
		{ "+3.3V high warning mask",    "vcc3_high_warning",     95, XFP_VCC3_HWARN_FLG },
		{ "+3.3V low warning mask",     "vcc3_low_warning",      95, XFP_VCC3_LWARN_FLG },
		{ "+1.8V high warning mask",    "vcc2_high_warning",     95, XFP_VCC2_HWARN_FLG },
		{ "+1.8V low warning mask",     "vcc2_low_warning",      95, XFP_VCC2_LWARN_FLG },
		{ "-5.2V high warning mask",    "vee5_high_warning",     95, XFP_VEE5_HWARN_FLG },
		{ "-5.2V low warning mask",     "vee5_low_warning",      95, XFP_VEE5_LWARN_FLG },
		{ NULL, NULL, 0, 0 },
	};

	open_json_object("interrupt_masks");

	for (int i = 0; masks[i].str; i++) {
		bool value = !!(lower[masks[i].offset] & masks[i].mask);

		module_print_any_bool(masks[i].str,
				      (char *)masks[i].json_name,
				      value,
				      value ? "Masked" : "Unmasked");
	}

	close_json_object();
}

static void xfp_show_signal_cond_ctrl(const __u8 *lower)
{
	__u8 val = lower[XFP_SIGNAL_COND_CTRL];
	unsigned int rate_sel = (val & XFP_SCC_RATE_SEL_MASK)
				>> XFP_SCC_RATE_SEL_SHIFT;

	/* Data rate = 9.5 + rate_sel * 0.2 Gb/s */
	float data_rate = 9.5f + rate_sel * 0.2f;

	open_json_object("signal_conditioner_control");

	module_print_any_float("Data rate select", data_rate, " Gb/s");

	module_print_any_bool("Lineside loopback",
			      "lineside_loopback",
			      !!(val & XFP_SCC_LINESIDE_LOOPBACK),
			      (val & XFP_SCC_LINESIDE_LOOPBACK) ?
			      "Enabled" : "Disabled");

	module_print_any_bool("XFI loopback",
			      "xfi_loopback",
			      !!(val & XFP_SCC_XFI_LOOPBACK),
			      (val & XFP_SCC_XFI_LOOPBACK) ?
			      "Enabled" : "Disabled");

	module_print_any_bool("REFCLK mode",
			      "refclk_mode",
			      !!(val & XFP_SCC_REFCLK_MODE),
			      (val & XFP_SCC_REFCLK_MODE) ?
			      "REFCLK required" : "REFCLK not required");

	close_json_object();
}

static void xfp_show_ber(const __u8 *lower, const __u8 *upper)
{
	if (!(upper[XFP_DIAG_MON_TYPE] & XFP_DIAG_BER_SUPPORT))
		return;

	open_json_object("ber");

	/* BER format: (hi_nibble/16) × 10^(-lo_nibble) */
	__u8 acceptable = lower[XFP_BER_ACCEPTABLE];
	__u8 actual     = lower[XFP_BER_ACTUAL];

	unsigned int acc_hi = (acceptable >> 4) & 0x0F;
	unsigned int acc_lo = acceptable & 0x0F;
	unsigned int act_hi = (actual >> 4) & 0x0F;
	unsigned int act_lo = actual & 0x0F;

	char ber_str[64];

	snprintf(ber_str, sizeof(ber_str), "%.4f x 10^(-%u)",
		 (float)acc_hi / 16.0f, acc_lo);
	module_print_any_string("BER acceptable", ber_str);

	snprintf(ber_str, sizeof(ber_str), "%.4f x 10^(-%u)",
		 (float)act_hi / 16.0f, act_lo);
	module_print_any_string("BER actual", ber_str);

	close_json_object();
}

static void xfp_show_fec(const __u8 *lower)
{
	open_json_object("fec_control");

	/* Byte 76: signed amplitude correction */
	__s8 amplitude = (__s8)lower[XFP_FEC_AMPLITUDE];
	/* Byte 77: unsigned phase correction */
	__u8 phase     = lower[XFP_FEC_PHASE];

	if (is_json_context()) {
		print_int(PRINT_JSON, "amplitude", "%d", amplitude);
		print_uint(PRINT_JSON, "phase", "%u", phase);
	} else {
		printf("\t%-41s : %d\n", "FEC amplitude correction", amplitude);
		printf("\t%-41s : %u\n", "FEC phase correction", phase);
	}

	close_json_object();
}

static void xfp_show_gen_ctrl_status(const __u8 *lower)
{
	static const struct {
		__u8 bit;
		const char *name;
		const char *json_name;
	} ctrl_110[] = {
		{ 7, "TX Disable state",   "tx_disable_state" },
		{ 6, "Soft TX Disable",    "soft_tx_disable" },
		{ 5, "MOD NR",             "mod_nr" },
		{ 4, "P_Down state",       "p_down_state" },
		{ 3, "Soft P_Down",        "soft_p_down" },
		{ 2, "Interrupt",          "interrupt" },
		{ 1, "RX LOS",             "rx_los" },
		{ 0, "Data Not Ready",     "data_not_ready" },
	};

	static const struct {
		__u8 bit;
		const char *name;
		const char *json_name;
	} ctrl_111[] = {
		{ 7, "TX NR",              "tx_nr" },
		{ 6, "TX Fault",           "tx_fault" },
		{ 5, "TX CDR not locked",  "tx_cdr_not_locked" },
		{ 3, "RX NR",              "rx_nr" },
		{ 1, "RX CDR not locked",  "rx_cdr_not_locked" },
	};

	__u8 val110 = lower[XFP_GEN_CTRL_STATUS];
	__u8 val111 = lower[XFP_GEN_CTRL_STATUS_2];

	open_json_object("general_control_status");

	for (unsigned int i = 0; i < ARRAY_SIZE(ctrl_110); i++) {
		bool set = !!(val110 & (1 << ctrl_110[i].bit));

		module_print_any_bool(ctrl_110[i].name,
				      (char *)ctrl_110[i].json_name,
				      set, ONOFF(set));
	}

	for (unsigned int i = 0; i < ARRAY_SIZE(ctrl_111); i++) {
		bool set = !!(val111 & (1 << ctrl_111[i].bit));

		module_print_any_bool(ctrl_111[i].name,
				      (char *)ctrl_111[i].json_name,
				      set, ONOFF(set));
	}

	close_json_object();
}

/* Infer EPON type from all-zero transceiver codes + wavelength range */
static void xfp_show_epon_detect(const __u8 *upper)
{
	for (int i = XFP_TRANSCEIVER_START; i <= XFP_TRANSCEIVER_END; i++) {
		if (upper[i])
			return;
	}

	/* Wavelength in nm (value / 20) */
	unsigned int wl_raw = OFFSET_TO_U16_PTR(upper, XFP_WAVELENGTH);
	float wl_nm = (float)wl_raw / 20.0f;

	/* 10G EPON OLT tri-plexer: TX 1577nm 10G downstream,
	 * RX 1270nm 10G upstream + RX 1310nm 1G upstream
	 * IEEE 802.3av (10G) + IEEE 802.3ah (1G)
	 */
	if (wl_nm >= 1572.0f && wl_nm <= 1582.0f) {
		open_json_object("epon_detection");
		module_print_any_string("EPON type",
			"10G EPON OLT tri-plexer (IEEE 802.3av/802.3ah)");
		module_print_any_string("Note",
			"wavelengths below are inferred from IEEE standard, not from EEPROM");
		module_print_any_float("TX 10G downstream (802.3av)",
				       1577.0f, " nm");
		module_print_any_float("RX 10G upstream (802.3av)",
				       1270.0f, " nm");
		module_print_any_float("RX 1G upstream (802.3ah)",
				       1310.0f, " nm");
		close_json_object();
		return;
	}

	/* 1G EPON OLT: TX 1490nm downstream, RX 1310nm upstream
	 * IEEE 802.3ah — 1000BASE-PX
	 */
	if (wl_nm >= 1485.0f && wl_nm <= 1495.0f) {
		open_json_object("epon_detection");
		module_print_any_string("EPON type",
			"1G EPON OLT (IEEE 802.3ah)");
		module_print_any_string("Note",
			"wavelengths below are inferred from IEEE standard, not from EEPROM");
		module_print_any_float("TX 1G downstream (802.3ah)",
				       1490.0f, " nm");
		module_print_any_float("RX 1G upstream (802.3ah)",
				       1310.0f, " nm");
		close_json_object();
	}
}

static void xfp_show_diagnostics(const __u8 *lower, const __u8 *upper)
{
	struct sff_diags sd = { 0 };

	sd.supports_dom = 1;
	sd.supports_alarms = 1;
	sd.rx_power_type = !!(upper[XFP_DIAG_MON_TYPE] & XFP_DIAG_RX_PWR_TYPE);

	sd.sfp_temp[MCURR]  = XFP_S16(lower, XFP_AD_TEMP);
	sd.sfp_voltage[MCURR] = XFP_U16(lower, XFP_AD_VCC);
	sd.bias_cur[MCURR]  = XFP_U16(lower, XFP_AD_BIAS);
	sd.tx_power[MCURR]  = XFP_U16(lower, XFP_AD_TX_PWR);
	sd.rx_power[MCURR]  = XFP_U16(lower, XFP_AD_RX_PWR);

	sd.sfp_temp[HALRM]  = XFP_S16(lower, XFP_TEMP_HALRM);
	sd.sfp_temp[LALRM]  = XFP_S16(lower, XFP_TEMP_LALRM);
	sd.sfp_temp[HWARN]  = XFP_S16(lower, XFP_TEMP_HWARN);
	sd.sfp_temp[LWARN]  = XFP_S16(lower, XFP_TEMP_LWARN);

	sd.sfp_voltage[HALRM] = XFP_U16(lower, XFP_VCC_HALRM);
	sd.sfp_voltage[LALRM] = XFP_U16(lower, XFP_VCC_LALRM);
	sd.sfp_voltage[HWARN] = XFP_U16(lower, XFP_VCC_HWARN);
	sd.sfp_voltage[LWARN] = XFP_U16(lower, XFP_VCC_LWARN);

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

	PRINT_TEMP_ALL("Module temperature", "module_temperature_measurement",
		       sd.sfp_temp[MCURR]);
	PRINT_VCC_ALL("Module voltage", "module_voltage_measurement",
		      sd.sfp_voltage[MCURR]);

	for (int i = 0; xfp_aw_flags[i].str; i++) {
		bool value = lower[xfp_aw_flags[i].offset] &
			     xfp_aw_flags[i].mask;
		module_print_any_bool(xfp_aw_flags[i].str, NULL,
				      value, ONOFF(value));
	}

	xfp_show_ext_int_flags(lower);
	xfp_show_int_masks(lower);

	xfp_show_thresholds(sd);

	xfp_show_signal_cond_ctrl(lower);
	xfp_show_ber(lower, upper);
	xfp_show_fec(lower);
	xfp_show_gen_ctrl_status(lower);
}

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

int xfp_show_all_nl(struct cmd_context *ctx)
{
	__u8 lower[XFP_PAGE_SIZE];
	__u8 upper[XFP_PAGE_SIZE];
	int ret;

	ret = xfp_get_lower_memory(ctx, lower);
	if (ret)
		return ret;

	ret = xfp_get_eeprom_page(ctx, 0x01, upper);
	if (ret)
		return ret;

	new_json_obj(ctx->json);
	open_json_object(NULL);

	xfp_show_serial_id(upper);
	xfp_show_diagnostics(lower, upper);
	xfp_show_epon_detect(upper);

	xfp_hex_dump(lower, 0x00, "Raw lower memory (0x00-0x7F)");
	xfp_hex_dump(upper, 0x80, "Raw upper memory (0x80-0xFF)");

	close_json_object();
	delete_json_obj();

	return 0;
}
