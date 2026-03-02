/*
 * cmis-ext.c: CMIS extension display functions.
 *
 * New libsffcmis features added on top of the base ethtool CMIS parser.
 * Includes: module type display, application descriptors, data path state,
 * module characteristics, extended capabilities, page initialization for
 * tunable/coherent/VDM/diagnostics pages, and the cmis_show_all_nl
 * orchestrator.
 *
 * Copyright (C) 2025 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <err.h>
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
#include "i2c.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-tunable.h"
#include "cmis-coherent.h"
#include "cmis-datapath.h"
#include "cmis-diag.h"
#include "cmis-netpath.h"
#include "cmis-vdm.h"

/* Page convenience macros for struct cmis_memory_map */
#define page_00h upper_memory[0x0][0x0]
#define page_01h upper_memory[0x0][0x1]
#define page_02h upper_memory[0x0][0x2]
#define page_04h upper_memory[0x0][0x4]
#define page_33h upper_memory[0x0][0x33]
#define page_34h upper_memory[0x0][0x34]
#define page_35h upper_memory[0x0][0x35]
#define page_3Bh upper_memory[0x0][0x3B]
#define page_40h upper_memory[0x0][0x40]
#define page_42h upper_memory[0x0][0x42]
#define page_44h upper_memory[0x0][0x44]

/* Quirk flags -- set when buggy modules are detected at runtime */
int cmis_quirk_coherent_adver;

/* Determine total media lane count from the first application descriptor.
 * Returns 0 if unable to determine (safe fallback to CMIS_CHANNELS_PER_BANK).
 */
static int cmis_media_lane_count(const struct cmis_memory_map *map)
{
	__u8 id, host_id, lane_info, host_lanes, media_lanes;
	int max_host;

	if (!map->lower_memory)
		return 0;

	/* Max host lanes depends on module form factor */
	id = map->lower_memory[CMIS_ID_OFFSET];
	switch (id) {
	case MODULE_ID_QSFP_PLUS_CMIS:	/* 0x1E: QSFP+ CMIS — 4 host lanes */
		max_host = 4;
		break;
	case MODULE_ID_QSFP_DD:		/* 0x18: QSFP-DD — 8 host lanes */
	case MODULE_ID_OSFP:		/* 0x19: OSFP — 8 host lanes */
		max_host = 8;
		break;
	case MODULE_ID_DSFP:		/* 0x1B: DSFP — 2 host lanes */
	case MODULE_ID_SFP_DD_CMIS:	/* 0x1F: SFP-DD CMIS — 2 host lanes */
		max_host = 2;
		break;
	case MODULE_ID_SFP_PLUS_CMIS:	/* 0x20: SFP+ CMIS — 1 host lane */
		max_host = 1;
		break;
	default:
		return 0;
	}

	/* Read first valid application descriptor */
	host_id = map->lower_memory[CMIS_APP_DESC_START_OFFSET];
	if (host_id == 0 || host_id == 0xFF)
		return 0;

	lane_info = map->lower_memory[CMIS_APP_DESC_START_OFFSET + 2];
	host_lanes = (lane_info >> 4) & 0x0F;
	media_lanes = lane_info & 0x0F;

	if (host_lanes == 0 || media_lanes == 0)
		return 0;

	return (max_host / host_lanes) * media_lanes;
}

static void cmis_show_module_type(const struct cmis_memory_map *map)
{
	__u8 type = map->lower_memory[CMIS_MODULE_TYPE_OFFSET];
	const char *desc;

	switch (type) {
	case CMIS_MT_MMF:
		desc = "MMF";
		break;
	case CMIS_MT_SMF:
		desc = "SMF";
		break;
	case CMIS_MT_PASSIVE_COPPER:
		desc = "Passive copper";
		break;
	case CMIS_MT_ACTIVE_CABLE:
		desc = "Active cable";
		break;
	case CMIS_MT_BASE_T:
		desc = "BASE-T";
		break;
	default:
		desc = NULL;
		break;
	}

	sff_print_any_hex_field("Module type", "module_type", type, desc);
}

/**
 * Print application descriptors. Each descriptor is 4 bytes:
 * [Host Interface ID, Media Interface ID, Host Lane Count/Options,
 *  Media Lane Count/Options]. Up to 8 descriptors in lower memory
 * starting at byte 0x56.
 * Relevant documents:
 * [1] CMIS Rev. 5, section 8.2.12
 * [2] SFF-8024 Rev 4.13, Tables 4-5 through 4-10
 */
static void cmis_show_app_descriptors(const struct cmis_memory_map *map)
{
	__u8 module_type = map->lower_memory[CMIS_MODULE_TYPE_OFFSET];
	int i;

	if (is_json_context())
		open_json_array("application_descriptors", "");

	for (i = 0; i < CMIS_MAX_APP_DESCS; i++) {
		int base = CMIS_APP_DESC_START_OFFSET + i * CMIS_APP_DESC_SIZE;
		__u8 host_id = map->lower_memory[base];
		__u8 media_id = map->lower_memory[base + 1];
		__u8 lane_info = map->lower_memory[base + 2];
		__u8 host_lanes = (lane_info >> 4) & 0x0F;
		__u8 media_lanes = lane_info & 0x0F;
		const char *host_name, *media_name;
		char host_buf[48], media_buf[48];

		/* Stop at empty or invalid descriptor */
		if ((host_id == 0 && media_id == 0) || host_id == 0xFF)
			break;

		host_name = sff8024_host_id_name(host_id);
		media_name = sff8024_media_id_name(module_type, media_id);

		if (!host_name) {
			snprintf(host_buf, sizeof(host_buf),
				 "Host 0x%02x", host_id);
			host_name = host_buf;
		}
		if (!media_name) {
			snprintf(media_buf, sizeof(media_buf),
				 "Media 0x%02x", media_id);
			media_name = media_buf;
		}

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "host_interface_id", "%u",
				   host_id);
			print_string(PRINT_JSON, "host_interface_name",
				     "%s", host_name);
			print_uint(PRINT_JSON, "media_interface_id", "%u",
				   media_id);
			print_string(PRINT_JSON, "media_interface_name",
				     "%s", media_name);
			print_uint(PRINT_JSON, "host_lane_count", "%u",
				   host_lanes);
			print_uint(PRINT_JSON, "media_lane_count", "%u",
				   media_lanes);
			close_json_object();
		} else {
			char name[48];

			snprintf(name, sizeof(name), "Application %d (%ux%u)",
				 i + 1, host_lanes, media_lanes);
			printf("\t%-41s : %s / %s\n",
			       name, host_name, media_name);
		}
	}

	if (is_json_context())
		close_json_array("");
}

static const char *cmis_dp_state_str(__u8 state)
{
	switch (state) {
	case CMIS_DP_STATE_DEACTIVATED:
		return "DPDeactivated";
	case CMIS_DP_STATE_INIT:
		return "DPInit";
	case CMIS_DP_STATE_DEINIT:
		return "DPDeinit";
	case CMIS_DP_STATE_ACTIVATED:
		return "DPActivated";
	case CMIS_DP_STATE_TX_TURN_ON:
		return "DPTxTurnOn";
	case CMIS_DP_STATE_TX_TURN_OFF:
		return "DPTxTurnOff";
	case CMIS_DP_STATE_INITIALIZED:
		return "DPInitialized";
	default:
		return "reserved";
	}
}

/**
 * Print the Data Path State for each data path. Per CMIS 5.3 Table 8-6,
 * byte 0x4D bits 3-0 = DP1, bits 7-4 = DP2; byte 0x4C bits 3-0 = DP3,
 * bits 7-4 = DP4 (bytes are swapped within each 2-byte group).
 * Relevant documents:
 * [1] CMIS Rev. 5, section 6.3.2.3, section 8.2.2
 */
static void cmis_show_dp_state(const struct cmis_memory_map *map)
{
	/* Byte offsets for DP pairs: [DP1/DP2, DP3/DP4, DP5/DP6, DP7/DP8].
	 * Within each 2-byte group the bytes are swapped (big-endian).
	 */
	static const int dp_byte_off[] = { 1, 0, 3, 2 };
	int pair;

	for (pair = 0; pair < 4; pair++) {
		__u8 byte = map->lower_memory[CMIS_DP_STATE_OFFSET +
					      dp_byte_off[pair]];
		__u8 state_lo = byte & CMIS_DP_STATE_MASK;
		__u8 state_hi = (byte >> 4) & CMIS_DP_STATE_MASK;
		int dp_odd = pair * 2 + 1;   /* lo nibble */
		int dp_even = pair * 2 + 2;  /* hi nibble */
		char name[48];

		if (state_lo) {
			snprintf(name, sizeof(name),
				 "Data path state (DP %d)", dp_odd);
			sff_print_any_hex_field(name, "dp_state",
						state_lo,
						cmis_dp_state_str(state_lo));
		}

		if (state_hi) {
			snprintf(name, sizeof(name),
				 "Data path state (DP %d)", dp_even);
			sff_print_any_hex_field(name, "dp_state",
						state_hi,
						cmis_dp_state_str(state_hi));
		}
	}
}

static void cmis_show_cdb_autopaging(const struct cmis_memory_map *map)
{
	__u8 autopaging = map->page_01h[CMIS_CDB_ADVER_OFFSET] &
			  CMIS_CDB_ADVER_AUTOPAGING_MASK;

	module_print_any_string("CDB auto-paging",
				autopaging ? "Supported" : "Not supported");
}

static void cmis_show_cdb_max_busy(const struct cmis_memory_map *map)
{
	__u8 byte_165 = map->page_01h[CMIS_CDB_ADVER_TRIGGER_OFFSET];
	__u8 byte_166 = map->page_01h[CMIS_CDB_ADVER_BUSY_OFFSET];
	int timeout_ms;
	char buf[64];

	if (byte_166 & CMIS_CDB_BUSY_SPEC_METHOD_MASK) {
		int x = byte_165 & CMIS_CDB_ADVER_EXT_BUSY_MASK;

		if (x < 1)
			x = 1;
		timeout_ms = x * 160;
		snprintf(buf, sizeof(buf), "%d ms (extended method)", timeout_ms);
	} else {
		int x = byte_166 & CMIS_CDB_BUSY_TIME_MASK;

		if (x > 80)
			x = 80;
		timeout_ms = 80 - x;
		snprintf(buf, sizeof(buf), "%d ms (short method)", timeout_ms);
	}

	module_print_any_string("CDB max busy time", buf);
}

static void cmis_show_cdb_adver_ext(const struct cmis_memory_map *map)
{
	if (!map->page_01h)
		return;

	/* Only show extensions if CDB is supported */
	__u8 cdb_instances = (map->page_01h[CMIS_CDB_ADVER_OFFSET] &
			      CMIS_CDB_ADVER_INSTANCES_MASK) >> 6;
	if (cdb_instances != 1 && cdb_instances != 2)
		return;

	cmis_show_cdb_autopaging(map);
	cmis_show_cdb_max_busy(map);
}

/* Table 8-48: 4-bit duration encoding */
static const char *cmis_duration_str(unsigned int code)
{
	static const char *const table[] = {
		[0x0] = "< 1 ms",
		[0x1] = "1-5 ms",
		[0x2] = "5-10 ms",
		[0x3] = "10-50 ms",
		[0x4] = "50-100 ms",
		[0x5] = "100-500 ms",
		[0x6] = "500-1000 ms",
		[0x7] = "1-5 s",
		[0x8] = "5-10 s",
		[0x9] = "10-60 s",
		[0xA] = "1-5 min",
		[0xB] = "5-10 min",
		[0xC] = "10-50 min",
		[0xD] = ">= 50 min",
	};

	if (code <= 0xD)
		return table[code];
	return "Reserved";
}

/* Tx input clock synchronization capability (byte 145, bits 6-5) */
static const char *cmis_tx_clk_cap_str(unsigned int code)
{
	switch (code) {
	case 0: return "Lanes 1-8";
	case 1: return "1-4 + 5-8";
	case 2: return "Pairs";
	case 3: return "Asynchronous";
	default: return "Unknown";
	}
}

/* Rx output EQ type (byte 151, bits 6-5) */
static const char *cmis_rx_out_eq_type_str(unsigned int code)
{
	switch (code) {
	case 0: return "Peak-to-peak";
	case 1: return "Steady-state";
	case 2: return "Average";
	default: return "Reserved";
	}
}

/* Show module characteristics and durations (Page 01h) */
static void cmis_show_module_chars(const struct cmis_memory_map *map)
{
	__u8 dur, dur2, dur3, chars, det, rx_levels, rx_eq;
	__u8 hw_major, hw_minor;
	__u8 modsel;
	__s8 temp_max, temp_min;
	__u16 prop_delay;
	__u8 vmin, cdr_pwr, tx_eq_max;
	char buf[64];

	if (!map->page_01h)
		return;

	hw_major = map->page_01h[CMIS_MODULE_HW_MAJOR_OFFSET];
	hw_minor = map->page_01h[CMIS_MODULE_HW_MINOR_OFFSET];

	/* Hardware revision — skip if both zero */
	if (hw_major || hw_minor) {
		if (is_json_context()) {
			open_json_object("hardware_version");
			print_uint(PRINT_JSON, "major", "%u", hw_major);
			print_uint(PRINT_JSON, "minor", "%u", hw_minor);
			close_json_object();
		} else {
			printf("\t%-41s : %u.%u\n",
			       "Hardware version", hw_major, hw_minor);
		}
	}

	/* ModSelWaitTime (byte 143) — m * 2^e µs */
	modsel = map->page_01h[CMIS_MODSEL_WAIT_TIME_OFFSET];
	if (modsel) {
		unsigned int exp = (modsel & CMIS_MODSEL_WAIT_EXP_MASK) >>
				   CMIS_MODSEL_WAIT_EXP_SHIFT;
		unsigned int mant = modsel & CMIS_MODSEL_WAIT_MANT_MASK;
		unsigned int us = mant * (1u << exp);

		snprintf(buf, sizeof(buf), "%u us", us);
		module_print_any_string("ModSel wait time", buf);
	}

	/* MaxDuration DPInit/DPDeinit (byte 144) */
	dur = map->page_01h[CMIS_STATE_DURATION_OFFSET];
	module_print_any_string("MaxDuration DPInit",
				cmis_duration_str(dur & CMIS_MAX_DUR_DPINIT_MASK));
	module_print_any_string("MaxDuration DPDeinit",
				cmis_duration_str((dur & CMIS_MAX_DUR_DPDEINIT_MASK) >>
						  CMIS_MAX_DUR_DPDEINIT_SHIFT));

	/* Additional durations (bytes 167-169) */
	dur = map->page_01h[CMIS_ADD_DUR1_OFFSET];
	module_print_any_string("MaxDuration ModulePwrUp",
				cmis_duration_str(dur & CMIS_MAX_DUR_MOD_PWRUP_MASK));
	module_print_any_string("MaxDuration ModulePwrDn",
				cmis_duration_str((dur & CMIS_MAX_DUR_MOD_PWRDN_MASK) >>
						  CMIS_MAX_DUR_MOD_PWRDN_SHIFT));

	dur2 = map->page_01h[CMIS_ADD_DUR2_OFFSET];
	module_print_any_string("MaxDuration TxTurnOn",
				cmis_duration_str(dur2 & CMIS_MAX_DUR_TXTURNON_MASK));
	module_print_any_string("MaxDuration TxTurnOff",
				cmis_duration_str((dur2 & CMIS_MAX_DUR_TXTURNOFF_MASK) >>
						  CMIS_MAX_DUR_TXTURNOFF_SHIFT));

	dur3 = map->page_01h[CMIS_ADD_DUR3_OFFSET];
	module_print_any_string("MaxDuration BankPageChange",
				cmis_duration_str((dur3 & CMIS_MAX_DUR_BPC_MASK) >>
						  CMIS_MAX_DUR_BPC_SHIFT));

	/* Module characteristics (byte 145) */
	chars = map->page_01h[CMIS_MOD_CHARS_OFFSET];
	module_print_any_string("Cooling",
				(chars & CMIS_MOD_COOLING_IMPLEMENTED) ?
				"Cooled" : "Uncooled");
	module_print_any_string("Tx input clock sync",
				cmis_tx_clk_cap_str((chars & CMIS_MOD_TX_CLK_CAP_MASK) >>
						    CMIS_MOD_TX_CLK_CAP_SHIFT));
	module_print_any_bool("ePPS", NULL,
			      !!(chars & CMIS_MOD_EPPS_SUPPORTED),
			      (chars & CMIS_MOD_EPPS_SUPPORTED) ?
			      "Supported" : "Not supported");

	/* Temperature range (bytes 146-147) — skip if both zero */
	temp_max = (__s8)map->page_01h[CMIS_MOD_TEMP_MAX_OFFSET];
	temp_min = (__s8)map->page_01h[CMIS_MOD_TEMP_MIN_OFFSET];
	if (temp_max || temp_min) {
		snprintf(buf, sizeof(buf), "%d to %d C", temp_min, temp_max);
		module_print_any_string("Module temperature range", buf);
	}

	/* Propagation delay (bytes 148-149) — U16 x 10 ns, skip if 0 */
	prop_delay = (map->page_01h[CMIS_PROPAGATION_DELAY_OFFSET] << 8) |
		     map->page_01h[CMIS_PROPAGATION_DELAY_OFFSET + 1];
	if (prop_delay) {
		snprintf(buf, sizeof(buf), "%u ns", prop_delay * 10);
		module_print_any_string("Propagation delay", buf);
	}

	/* Min operating voltage (byte 150) — U8 x 20 mV, skip if 0 */
	vmin = map->page_01h[CMIS_OPERATING_VOLTAGE_MIN_OFFSET];
	if (vmin) {
		snprintf(buf, sizeof(buf), "%.2f V", vmin * 0.02);
		module_print_any_string("Min operating voltage", buf);
	}

	/* Optical detector and signal characteristics (byte 151) */
	det = map->page_01h[CMIS_OPTICAL_DET_OFFSET];
	module_print_any_string("Optical detector",
				(det & CMIS_OPTICAL_DET_APD) ? "APD" : "PIN");
	module_print_any_string("Rx output EQ type",
				cmis_rx_out_eq_type_str((det & CMIS_RX_OUT_EQ_TYPE_MASK) >>
							CMIS_RX_OUT_EQ_TYPE_SHIFT));
	module_print_any_string("Rx power measurement",
				(det & CMIS_RX_PWR_MEAS_TYPE) ?
				"Average power" : "OMA");

	snprintf(buf, sizeof(buf), "%s%s",
		 (det & CMIS_RX_LOS_TYPE) ? "Average power" : "OMA",
		 (det & CMIS_RX_LOS_FAST) ? " (fast)" : "");
	module_print_any_string("Rx LOS type", buf);

	snprintf(buf, sizeof(buf), "%s%s",
		 (det & CMIS_TX_DISABLE_MODULE_WIDE) ? "Module-wide" : "Per-lane",
		 (det & CMIS_TX_DISABLE_FAST) ? " (fast)" : "");
	module_print_any_string("Tx disable", buf);

	/* CDR bypass power saved (byte 152) — U8 x 0.01 W, skip if 0 */
	cdr_pwr = map->page_01h[CMIS_CDR_PWR_SAVED_OFFSET];
	if (cdr_pwr) {
		snprintf(buf, sizeof(buf), "%.2f W/lane", cdr_pwr * 0.01);
		module_print_any_string("CDR bypass power saved", buf);
	}

	/* Tx input EQ max (byte 153, bits 3-0) — skip if 0 */
	rx_levels = map->page_01h[CMIS_RX_OUT_LEVELS_OFFSET];
	tx_eq_max = rx_levels & CMIS_TX_INPUT_EQ_MAX_MASK;
	if (tx_eq_max) {
		snprintf(buf, sizeof(buf), "%u dB", tx_eq_max);
		module_print_any_string("Tx input EQ max", buf);
	}

	/* Rx output EQ max (byte 154) — post/pre cursor, skip if both 0 */
	rx_eq = map->page_01h[CMIS_RX_OUT_EQ_MAX_OFFSET];
	if (rx_eq) {
		__u8 post = (rx_eq & CMIS_RX_OUT_EQ_POST_MAX_MASK) >>
			    CMIS_RX_OUT_EQ_POST_MAX_SHIFT;
		__u8 pre = rx_eq & CMIS_RX_OUT_EQ_PRE_MAX_MASK;

		snprintf(buf, sizeof(buf), "post %u dB, pre %u dB",
			 post, pre);
		module_print_any_string("Rx output EQ max", buf);
	}
}

/* Show extended module capability bits (Page 01h, bytes 155-162) */
static void cmis_show_extended_caps(const struct cmis_memory_map *map)
{
	__u8 b155, b159, b160;
	__u8 eq_bufs;

	if (!map->page_01h)
		return;

	b155 = map->page_01h[CMIS_EXT_CAPS_OFFSET];
	b159 = map->page_01h[CMIS_ECAP_MONITORS_OFFSET];
	b160 = map->page_01h[CMIS_ECAP_CDR_EQ_OFFSET];

	if (is_json_context()) {
		open_json_object("extended_capabilities");
		print_bool(PRINT_JSON, "forced_squelch_tx", NULL,
			   !!(b155 & CMIS_ECAP_FORCED_SQUELCH));
		print_bool(PRINT_JSON, "auto_squelch_disable_tx", NULL,
			   !!(b155 & CMIS_ECAP_AUTO_SQUELCH_DIS));
		print_bool(PRINT_JSON, "output_disable_tx", NULL,
			   !!(b155 & CMIS_ECAP_OUTPUT_DIS_TX));
		print_bool(PRINT_JSON, "input_polarity_flip", NULL,
			   !!(b155 & CMIS_ECAP_INPUT_POL_FLIP));
		print_bool(PRINT_JSON, "output_polarity_flip", NULL,
			   !!(b155 & CMIS_ECAP_OUTPUT_POL_FLIP));
		print_bool(PRINT_JSON, "bank_broadcast", NULL,
			   !!(b155 & CMIS_ECAP_BANK_BROADCAST));
		print_bool(PRINT_JSON, "custom_monitors_tx", NULL,
			   !!(b159 & CMIS_ECAP_CUSTOM_MON_TX));
		print_bool(PRINT_JSON, "custom_monitors_rx", NULL,
			   !!(b159 & CMIS_ECAP_CUSTOM_MON_RX));
		print_bool(PRINT_JSON, "cdr_bypass_tx", NULL,
			   !!(b160 & CMIS_ECAP_CDR_BYPASS_TX));
		print_bool(PRINT_JSON, "cdr_bypass_rx", NULL,
			   !!(b160 & CMIS_ECAP_CDR_BYPASS_RX));
		print_bool(PRINT_JSON, "adaptive_eq_tx", NULL,
			   !!(b160 & CMIS_ECAP_ADAPT_EQ_TX));
		print_bool(PRINT_JSON, "fixed_eq_tx", NULL,
			   !!(b160 & CMIS_ECAP_FIXED_EQ_TX));
		eq_bufs = (b160 & CMIS_ECAP_EQ_RECALL_BUF_TX) >>
			  CMIS_ECAP_EQ_RECALL_SHIFT_TX;
		print_uint(PRINT_JSON, "eq_recall_buffers_tx", "%u",
			   eq_bufs);
		close_json_object();
	} else {
		printf("\n\t%-41s :\n",
		       "Extended Module Capabilities (01h:155)");
		printf("\t  %-39s : %s\n",
		       "Forced Squelch Tx",
		       (b155 & CMIS_ECAP_FORCED_SQUELCH) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Auto Squelch Disable Tx",
		       (b155 & CMIS_ECAP_AUTO_SQUELCH_DIS) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Output Disable Tx",
		       (b155 & CMIS_ECAP_OUTPUT_DIS_TX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Input Polarity Flip",
		       (b155 & CMIS_ECAP_INPUT_POL_FLIP) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Output Polarity Flip",
		       (b155 & CMIS_ECAP_OUTPUT_POL_FLIP) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Bank Broadcast",
		       (b155 & CMIS_ECAP_BANK_BROADCAST) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Custom Monitors Tx",
		       (b159 & CMIS_ECAP_CUSTOM_MON_TX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Custom Monitors Rx",
		       (b159 & CMIS_ECAP_CUSTOM_MON_RX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "CDR Bypass Tx",
		       (b160 & CMIS_ECAP_CDR_BYPASS_TX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "CDR Bypass Rx",
		       (b160 & CMIS_ECAP_CDR_BYPASS_RX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Adaptive Input EQ Tx",
		       (b160 & CMIS_ECAP_ADAPT_EQ_TX) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "Fixed Input EQ Tx",
		       (b160 & CMIS_ECAP_FIXED_EQ_TX) ?
		       "Supported" : "Not supported");

		eq_bufs = (b160 & CMIS_ECAP_EQ_RECALL_BUF_TX) >>
			  CMIS_ECAP_EQ_RECALL_SHIFT_TX;
		printf("\t  %-39s : %u\n",
		       "EQ Recall Buffers Tx", eq_bufs);
	}
}

/*
 * Quirk: some coherent modules (e.g. Finisar FTLC3355) implement C-CMIS
 * pages but do not set bit 4 (CoherentPagesSupported) in 01h:142.
 *
 * Detect C-band/L-band tunable laser technology, probe Page 40h for a
 * valid C-CMIS revision, and force-set the advertisement bit so that
 * downstream code can access coherent pages.  Page 40h is stashed in
 * the memory map to avoid a duplicate I2C read later.
 */
void
cmis_quirk_check_coherent(struct cmd_context *ctx,
			  struct cmis_memory_map *map)
{
	struct module_eeprom request;
	__u8 tech, *page;
	int ret;

	cmis_quirk_coherent_adver = 0;

	if (map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	    CMIS_PAGES_ADVER_COHERENT)
		return;

	tech = map->page_00h[CMIS_MEDIA_INTF_TECH_OFFSET];
	if (tech != CMIS_CBAND_TUNABLE && tech != CMIS_LBAND_TUNABLE)
		return;

	cmis_request_init(&request, 0, 0x40, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return;

	page = request.data - CMIS_PAGE_SIZE;

	if (page[CCMIS_REV_OFFSET] == 0x00 ||
	    page[CCMIS_REV_OFFSET] == 0xFF) {
		free(request.data);
		return;
	}

	cmis_quirk_coherent_adver = 1;
	warnx("buggy module: C-CMIS pages present (rev %d.%d) "
	      "but 01h:142 bit 4 not set -- forcing coherent advertisement",
	      (page[CCMIS_REV_OFFSET] >> 4) & 0x0F,
	      page[CCMIS_REV_OFFSET] & 0x0F);

	((__u8 *)map->page_01h)[CMIS_PAGES_ADVER_OFFSET] |=
		CMIS_PAGES_ADVER_COHERENT;

	/* Stash Page 40h so the later fetch can skip the duplicate read */
	map->page_40h = page;
}

/**
 * Initialize additional CMIS pages beyond the base set (00h, 01h, 02h, 11h).
 * Fetches: 10h, 04h/12h (tunable), 13h-14h (diagnostics), 15h (latency),
 * 16h-17h (network path), coherent pages, VDM pages.
 */
static int
cmis_memory_map_init_pages_ext(struct cmd_context *ctx,
			       struct cmis_memory_map *map)
{
	struct module_eeprom request;
	int num_banks, i, ret;

	/* Compute media lane count now that page_00h is available */
	map->media_lane_count = cmis_media_lane_count(map);

	/* Determine number of banks */
	ret = cmis_num_banks_get(map, &num_banks);
	if (ret < 0)
		return ret;

	/* Fetch Page 10h (data path control/status) per bank */
	for (i = 0; i < num_banks; i++) {
		cmis_request_init(&request, i, 0x10, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->upper_memory[i][0x10] =
				request.data - CMIS_PAGE_SIZE;
	}

	/* Fetch Page 04h and 12h if tunable laser is advertised */
	if (map->page_01h[CMIS_TUNABLE_ADVER_OFFSET] &
	    CMIS_TUNABLE_ADVER_MASK) {
		cmis_request_init(&request, 0, 0x4, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->page_04h = request.data - CMIS_PAGE_SIZE;

		for (i = 0; i < num_banks; i++) {
			cmis_request_init(&request, i, 0x12, CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x12] =
					request.data - CMIS_PAGE_SIZE;
		}
	}

	/* Fetch diagnostics pages if advertised (01h:142 bit 5) */
	if (map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	    CMIS_PAGES_ADVER_DIAG) {
		cmis_request_init(&request, 0, 0x13, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->upper_memory[0][0x13] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, 0, 0x14, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->upper_memory[0][0x14] =
				request.data - CMIS_PAGE_SIZE;
	}

	/* Fetch Page 15h data path latency if advertised (01h:145 bit 3) */
	if (map->page_01h[CMIS_DP_LATENCY_ADVER_OFFSET] &
	    CMIS_DP_LATENCY_ADVER_MASK) {
		for (i = 0; i < num_banks; i++) {
			cmis_request_init(&request, i, 0x15,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x15] =
					request.data - CMIS_PAGE_SIZE;
		}
	}

	/* Fetch Network Path pages if advertised (01h:142 bit 7) */
	if (map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	    CMIS_PAGES_ADVER_NETWORK_PATH) {
		for (i = 0; i < num_banks; i++) {
			cmis_request_init(&request, i, 0x16,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x16] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i, 0x17,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x17] =
					request.data - CMIS_PAGE_SIZE;
		}
	}

	cmis_quirk_check_coherent(ctx, map);

	/* Fetch C-CMIS coherent pages if advertised (01h:142 bit 4) */
	if (map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	    CMIS_PAGES_ADVER_COHERENT) {
		cmis_request_init(&request, 0, 0x35, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->page_35h = request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, 0, 0x34, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->page_34h = request.data - CMIS_PAGE_SIZE;

		/* Page 40h may already be stashed by the quirk probe */
		if (!map->page_40h) {
			cmis_request_init(&request, 0, 0x40, CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->page_40h = request.data - CMIS_PAGE_SIZE;
		}

		cmis_request_init(&request, 0, 0x42, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->page_42h = request.data - CMIS_PAGE_SIZE;

		/* Page 44h: alarm advertisement (non-banked) */
		cmis_request_init(&request, 0, 0x44, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->page_44h = request.data - CMIS_PAGE_SIZE;

		/* Page 41h: Rx signal power advertisement (non-banked) */
		cmis_request_init(&request, 0, CCMIS_RX_PWR_ADVER_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->upper_memory[0][CCMIS_RX_PWR_ADVER_PAGE] =
				request.data - CMIS_PAGE_SIZE;

		/* Page 43h: provisioning advertisement (non-banked) */
		cmis_request_init(&request, 0, CCMIS_PROV_ADVER_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map->upper_memory[0][CCMIS_PROV_ADVER_PAGE] =
				request.data - CMIS_PAGE_SIZE;

		/* Pages 33h, 3Bh, 30h, 31h: coherent flags/thresholds/prov (banked) */
		for (i = 0; i < num_banks; i++) {
			cmis_request_init(&request, i, 0x33,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x33] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i, 0x3B,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][0x3B] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i, CCMIS_HOST_FEC_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][CCMIS_HOST_FEC_PAGE] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i, CCMIS_THRESH_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][CCMIS_THRESH_PAGE] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i, CCMIS_PROV_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][CCMIS_PROV_PAGE] =
					request.data - CMIS_PAGE_SIZE;

			cmis_request_init(&request, i,
					  CCMIS_HOST_THRESH_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[i][CCMIS_HOST_THRESH_PAGE] =
					request.data - CMIS_PAGE_SIZE;
		}
	}

	/* Fetch VDM pages if advertised (01h:142 bit 6) */
	if (map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	    CMIS_PAGES_ADVER_VDM) {
		int vdm_groups, g;

		/* Page 2Fh: advertisement/control */
		cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0) {
			map->upper_memory[0][CMIS_VDM_ADVER_PAGE] =
				request.data - CMIS_PAGE_SIZE;

			vdm_groups =
				(map->upper_memory[0][CMIS_VDM_ADVER_PAGE]
				 [CMIS_VDM_SUPPORT_OFFSET] &
				 CMIS_VDM_SUPPORT_MASK) + 1;
			if (vdm_groups > CMIS_VDM_MAX_GROUPS)
				vdm_groups = CMIS_VDM_MAX_GROUPS;

			/* Freeze VDM samples for consistent snapshot */
			cmis_vdm_freeze(ctx);

			for (g = 0; g < vdm_groups; g++) {
				/* Descriptor page */
				cmis_request_init(&request, 0,
						  CMIS_VDM_DESC_PAGE_BASE + g,
						  CMIS_PAGE_SIZE);
				ret = get_eeprom_page(ctx, &request);
				if (ret == 0)
					map->upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g] =
						request.data - CMIS_PAGE_SIZE;

				/* Sample page */
				cmis_request_init(&request, 0,
						  CMIS_VDM_SAMPLE_PAGE_BASE + g,
						  CMIS_PAGE_SIZE);
				ret = get_eeprom_page(ctx, &request);
				if (ret == 0)
					map->upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g] =
						request.data - CMIS_PAGE_SIZE;

				/* Threshold page */
				cmis_request_init(&request, 0,
						  CMIS_VDM_THRESH_PAGE_BASE + g,
						  CMIS_PAGE_SIZE);
				ret = get_eeprom_page(ctx, &request);
				if (ret == 0)
					map->upper_memory[0][CMIS_VDM_THRESH_PAGE_BASE + g] =
						request.data - CMIS_PAGE_SIZE;
			}

			/* Unfreeze VDM samples */
			cmis_vdm_unfreeze(ctx);

			/* Flag page 2Ch */
			cmis_request_init(&request, 0, CMIS_VDM_FLAGS_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[0][CMIS_VDM_FLAGS_PAGE] =
					request.data - CMIS_PAGE_SIZE;

			/* Mask page 2Dh */
			cmis_request_init(&request, 0, CMIS_VDM_MASKS_PAGE,
					  CMIS_PAGE_SIZE);
			ret = get_eeprom_page(ctx, &request);
			if (ret == 0)
				map->upper_memory[0][CMIS_VDM_MASKS_PAGE] =
					request.data - CMIS_PAGE_SIZE;
		}
	}

	return 0;
}

int cmis_show_all_nl(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	int ret;

	new_json_obj(ctx->json);
	open_json_object(NULL);

	/* Base page initialization (00h, 01h, 02h, 11h) */
	ret = cmis_memory_map_init_pages(ctx, &map);
	if (ret < 0)
		return ret;

	/* Extension page initialization (10h, 04h/12h, 13h-17h, coherent, VDM) */
	cmis_memory_map_init_pages_ext(ctx, &map);

	/* Base ethtool display (17 functions) */
	cmis_show_all_common(&map);

	/* Extension displays */
	cmis_show_module_type(&map);
	cmis_show_app_descriptors(&map);
	cmis_show_dp_state(&map);
	cmis_show_module_chars(&map);
	cmis_show_cdb_adver_ext(&map);
	cmis_show_extended_caps(&map);
	cmis_show_tunable_laser(&map);
	cmis_show_datapath(&map);
	cmis_show_netpath(&map);
	cmis_show_diag(&map);
	cmis_show_coherent_pm(&map);
	cmis_show_coherent_flags(&map);
	cmis_show_vdm(&map);

	close_json_object();
	delete_json_obj();

	return 0;
}
