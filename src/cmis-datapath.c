/**
 * cmis-datapath.c: CMIS Data Path Control and Status (Pages 10h/11h)
 *
 * Displays per-lane data path state, active application selection,
 * CDR status, output status, and configuration status per
 * OIF-CMIS-05.3 Tables 8-47 and 8-50.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-datapath.h"

/* Page convenience macros for struct cmis_memory_map */
#define page_00h upper_memory[0x0][0x0]
#define page_01h upper_memory[0x0][0x1]

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

static const char *cmis_config_status_str(__u8 cs)
{
	switch (cs) {
	case CMIS_CFGSTAT_UNDEFINED:
		return "Undefined";
	case CMIS_CFGSTAT_SUCCESS:
		return "Success";
	case CMIS_CFGSTAT_REJECTED:
		return "Rejected";
	case CMIS_CFGSTAT_REJECTED_INVALID:
		return "RejectedInvalidAppSel";
	case CMIS_CFGSTAT_IN_PROGRESS:
		return "InProgress";
	case CMIS_CFGSTAT_REJECTED_DPID:
		return "RejectedInvalidDPID";
	case CMIS_CFGSTAT_REJECTED_APPSEL:
		return "RejectedInvalidSI";
	case CMIS_CFGSTAT_REJECTED_LANES_IN_USE:
		return "RejectedLanesInUse";
	case CMIS_CFGSTAT_REJECTED_PARTIAL:
		return "RejectedPartialDP";
	default:
		if (cs >= CMIS_CFGSTAT_CUSTOM_START)
			return "CustomRejected";
		return "Reserved";
	}
}

/*
 * Get data path state for a host lane (1-8) from Page 11h.
 * 4-bit nibbles packed two per byte, low nibble = odd lane, high = even.
 * Bytes 0x80-0x83 hold lanes 1-8.
 */
static __u8 dp_state_for_lane(const __u8 *page_11h, int lane)
{
	int byte_off = CMIS_DP_STATE_HOST_OFFSET + lane / 2;
	__u8 byte = page_11h[byte_off];

	if (lane & 1)
		return (byte >> 4) & 0x0F;
	return byte & 0x0F;
}

/*
 * Get config status for a host lane (0-7) from Page 11h.
 * 4-bit nibbles packed two per byte.
 * Bytes 0xCA-0xCD hold lanes 1-8.
 */
static __u8 config_status_for_lane(const __u8 *page_11h, int lane)
{
	int byte_off = CMIS_CONFIG_STATUS_LANE_OFFSET + lane / 2;
	__u8 byte = page_11h[byte_off];

	if (lane & 1)
		return (byte >> 4) & CMIS_CONFIG_STATUS_MASK;
	return byte & CMIS_CONFIG_STATUS_MASK;
}

/*
 * Look up application descriptor name from Page 00h.
 * appsel is 1-based (1-15); 0 means unused/NULL.
 * Returns a description like "CAUI-4 C2M w/o FEC / 100G-ZR".
 */
static const char *
app_name_for_appsel(const struct cmis_memory_map *map, int appsel,
		    char *buf, int buflen)
{
	int base;
	__u8 host_id, media_id, module_type;
	const char *host_name, *media_name;

	if (appsel < 1 || appsel > CMIS_MAX_APP_DESCS || !map->page_00h) {
		snprintf(buf, buflen, "(AppSel %d)", appsel);
		return buf;
	}

	base = CMIS_APP_DESC_START_OFFSET + (appsel - 1) * CMIS_APP_DESC_SIZE;
	host_id = map->lower_memory[base];
	media_id = map->lower_memory[base + 1];
	module_type = map->lower_memory[CMIS_MODULE_TYPE_OFFSET];

	if (host_id == 0 && media_id == 0) {
		snprintf(buf, buflen, "(unused)");
		return buf;
	}

	host_name = sff8024_host_id_name(host_id);
	media_name = sff8024_media_id_name(module_type, media_id);

	snprintf(buf, buflen, "%s / %s",
		 host_name ? host_name : "?",
		 media_name ? media_name : "?");
	return buf;
}

static void
cmis_show_dp_lanes(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_11h = map->upper_memory[bank][0x11];
	int lanes = cmis_lanes_per_bank(map, bank);
	int i;

	if (!page_11h)
		return;

	if (is_json_context()) {
		char key[32];

		snprintf(key, sizeof(key), "datapath_bank_%d", bank);
		open_json_array(key, NULL);
	} else {
		if (bank > 0)
			printf("\t%-41s :\n",
			       "Data Path Status (continued)");
		printf("\t  %-6s %-15s %-6s %-30s %-6s %-6s %-8s\n",
		       "Lane", "DPState", "AppSel", "Application",
		       "TxOut", "RxOut", "Config");
		printf("\t  %-6s %-15s %-6s %-30s %-6s %-6s %-8s\n",
		       "----", "---------------", "------",
		       "------------------------------",
		       "------", "------", "--------");
	}

	for (i = 0; i < lanes; i++) {
		int global_lane = bank * CMIS_CHANNELS_PER_BANK + i + 1;
		__u8 dp_state = dp_state_for_lane(page_11h, i);
		__u8 appsel = 0;
		__u8 dpid = 0;
		__u8 cfg_status = config_status_for_lane(page_11h, i);
		bool tx_out = page_11h[CMIS_OUTPUT_STATUS_TX] & (1 << i);
		bool rx_out = page_11h[CMIS_OUTPUT_STATUS_RX] & (1 << i);
		char app_buf[80];

		/* Active Control Set DPConfig if available */
		appsel = (page_11h[CMIS_ACS_DPCONFIG_LANE(i)] &
			  CMIS_DPCONFIG_APPSEL_MASK) >>
			 CMIS_DPCONFIG_APPSEL_SHIFT;
		dpid = (page_11h[CMIS_ACS_DPCONFIG_LANE(i)] &
			CMIS_DPCONFIG_DPID_MASK) >>
		       CMIS_DPCONFIG_DPID_SHIFT;

		if (appsel > 0)
			app_name_for_appsel(map, appsel, app_buf,
					    sizeof(app_buf));
		else
			snprintf(app_buf, sizeof(app_buf), "(unused)");

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u", global_lane);
			print_string(PRINT_JSON, "dp_state", "%s",
				     cmis_dp_state_str(dp_state));
			print_uint(PRINT_JSON, "app_sel", "%u", appsel);
			print_uint(PRINT_JSON, "dp_id", "%u", dpid);
			print_string(PRINT_JSON, "application", "%s",
				     app_buf);
			print_bool(PRINT_JSON, "tx_output_valid",
				   NULL, tx_out);
			print_bool(PRINT_JSON, "rx_output_valid",
				   NULL, rx_out);
			print_string(PRINT_JSON, "config_status", "%s",
				     cmis_config_status_str(cfg_status));
			close_json_object();
		} else {
			printf("\t  %-6d %-15s %-6d %-30.30s %-6s %-6s %s\n",
			       global_lane,
			       cmis_dp_state_str(dp_state),
			       appsel,
			       app_buf,
			       tx_out ? "Valid" : "--",
			       rx_out ? "Valid" : "--",
			       cmis_config_status_str(cfg_status));
		}
	}

	if (is_json_context())
		close_json_array(NULL);
}

static void
cmis_show_dp_controls(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_10h = map->upper_memory[bank][0x10];
	const __u8 *page_11h = map->upper_memory[bank][0x11];
	int lanes = cmis_lanes_per_bank(map, bank);
	int i;

	if (!page_10h && !page_11h)
		return;

	if (is_json_context()) {
		char key[48];

		snprintf(key, sizeof(key), "datapath_control_bank_%d", bank);
		open_json_array(key, NULL);
	} else {
		printf("\n");
		if (bank == 0)
			printf("\t%-41s :\n",
			       "Staged/Active Control (Page 10h/11h)");
		printf("\t  %-6s %-8s %-5s %-5s %-8s %-8s %-8s %-8s\n",
		       "Lane", "DPDeinit", "ExCtl", "DPID",
		       "CDRTx", "CDRRx", "DPInit?", "TxDis");
		printf("\t  %-6s %-8s %-5s %-5s %-8s %-8s %-8s %-8s\n",
		       "----", "--------", "-----", "-----",
		       "--------", "--------", "--------", "--------");
	}

	for (i = 0; i < lanes; i++) {
		int global_lane = bank * CMIS_CHANNELS_PER_BANK + i + 1;
		bool dp_deinit = false;
		bool explicit_ctl = false;
		__u8 dpid = 0;
		bool cdr_tx = false, cdr_rx = false;
		bool dp_init_pending = false;
		bool tx_disable = false;
		/* SI control fields */
		bool pol_tx = false, pol_rx = false;
		bool rx_disable = false;
		bool auto_sq_dis_tx = false, auto_sq_dis_rx = false;
		bool sq_force_tx = false;
		bool eq_freeze_tx = false;
		bool eq_enable_tx = false;
		__u8 eq_target_tx = 0;
		__u8 rx_eq_pre = 0, rx_eq_post = 0, rx_amplitude = 0;

		if (page_10h) {
			dp_deinit = page_10h[CMIS_DP_DEINIT_OFFSET] &
				    (1 << i);
			explicit_ctl = page_10h[CMIS_SCS0_DPCONFIG_LANE(i)] &
				       CMIS_DPCONFIG_EXPLICIT_MASK;
			dpid = (page_10h[CMIS_SCS0_DPCONFIG_LANE(i)] &
				CMIS_DPCONFIG_DPID_MASK) >>
			       CMIS_DPCONFIG_DPID_SHIFT;
			tx_disable = page_10h[CMIS_OUTPUT_DIS_TX] & (1 << i);

			/* SI control registers */
			pol_tx = !!(page_10h[CMIS_INPUT_POL_FLIP_TX] &
				    (1 << i));
			pol_rx = !!(page_10h[CMIS_OUTPUT_POL_FLIP_RX] &
				    (1 << i));
			rx_disable = !!(page_10h[CMIS_OUTPUT_DIS_RX] &
					(1 << i));
			auto_sq_dis_tx = !!(page_10h[CMIS_AUTO_SQUELCH_DIS_TX] &
					    (1 << i));
			auto_sq_dis_rx = !!(page_10h[CMIS_AUTO_SQUELCH_DIS_RX] &
					    (1 << i));
			sq_force_tx = !!(page_10h[CMIS_OUTPUT_SQUELCH_FORCE_TX] &
					 (1 << i));
			eq_freeze_tx = !!(page_10h[CMIS_ADAPT_INPUT_EQ_FREEZE_TX] &
					  (1 << i));
			eq_enable_tx = !!(page_10h[CMIS_SCS0_ADAPT_EQ_TX] &
					  (1 << i));

			/* Nibble fields: even lanes = low nibble, odd = high */
			{
				__u8 byte_val, shift;

				shift = (i & 1) ? 4 : 0;

				byte_val = page_10h[0x9C + i / 2];
				eq_target_tx = (byte_val >> shift) & 0x0F;

				byte_val = page_10h[0xA2 + i / 2];
				rx_eq_pre = (byte_val >> shift) & 0x0F;

				byte_val = page_10h[0xA6 + i / 2];
				rx_eq_post = (byte_val >> shift) & 0x0F;

				byte_val = page_10h[0xAA + i / 2];
				rx_amplitude = (byte_val >> shift) & 0x0F;
			}
		}

		/* CDR from Active Control Set (Page 11h) if available */
		if (page_11h) {
			cdr_tx = page_11h[CMIS_ACS_CDR_ENABLE_TX] & (1 << i);
			cdr_rx = page_11h[CMIS_ACS_CDR_ENABLE_RX] & (1 << i);
			dp_init_pending = page_11h[CMIS_DP_INIT_PENDING] &
					  (1 << i);
		}

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u", global_lane);
			print_bool(PRINT_JSON, "dp_deinit",
				   NULL, dp_deinit);
			print_bool(PRINT_JSON, "explicit_control",
				   NULL, explicit_ctl);
			print_uint(PRINT_JSON, "dp_id", "%u", dpid);
			print_bool(PRINT_JSON, "cdr_tx", NULL, cdr_tx);
			print_bool(PRINT_JSON, "cdr_rx", NULL, cdr_rx);
			print_bool(PRINT_JSON, "dp_init_pending",
				   NULL, dp_init_pending);
			print_bool(PRINT_JSON, "tx_disable",
				   NULL, tx_disable);
			/* SI control fields */
			print_bool(PRINT_JSON, "polarity_tx",
				   NULL, pol_tx);
			print_bool(PRINT_JSON, "polarity_rx",
				   NULL, pol_rx);
			print_bool(PRINT_JSON, "rx_disable",
				   NULL, rx_disable);
			print_bool(PRINT_JSON, "auto_squelch_dis_tx",
				   NULL, auto_sq_dis_tx);
			print_bool(PRINT_JSON, "auto_squelch_dis_rx",
				   NULL, auto_sq_dis_rx);
			print_bool(PRINT_JSON, "squelch_force_tx",
				   NULL, sq_force_tx);
			print_bool(PRINT_JSON, "eq_freeze_tx",
				   NULL, eq_freeze_tx);
			print_bool(PRINT_JSON, "eq_enable_tx",
				   NULL, eq_enable_tx);
			print_uint(PRINT_JSON, "eq_target_tx",
				   "%u", eq_target_tx);
			print_uint(PRINT_JSON, "rx_eq_pre",
				   "%u", rx_eq_pre);
			print_uint(PRINT_JSON, "rx_eq_post",
				   "%u", rx_eq_post);
			print_uint(PRINT_JSON, "rx_amplitude",
				   "%u", rx_amplitude);
			close_json_object();
		} else {
			printf("\t  %-6d %-8s %-5s %-5d %-8s %-8s %-8s %-8s\n",
			       global_lane,
			       dp_deinit ? "Yes" : "No",
			       explicit_ctl ? "Yes" : "No",
			       dpid,
			       cdr_tx ? "On" : "Off",
			       cdr_rx ? "On" : "Off",
			       dp_init_pending ? "Yes" : "No",
			       tx_disable ? "Yes" : "No");
		}
	}

	if (is_json_context())
		close_json_array(NULL);
}

/* Per-lane flag descriptor */
struct cmis_lane_flag {
	const char *name;
	const char *json_name;
	__u8 offset;  /* Page 11h byte offset */
};

static const struct cmis_lane_flag cmis_lane_flags[] = {
	{ "Tx Fault",           "tx_fault",           CMIS_FLAG_TX_FAULT },
	{ "Tx LOS",             "tx_los",             CMIS_FLAG_TX_LOS },
	{ "Tx CDR LOL",         "tx_cdr_lol",         CMIS_FLAG_TX_CDR_LOL },
	{ "Tx Adapt EQ Fault",  "tx_adapt_eq_fault",  CMIS_FLAG_TX_ADAPT_EQ_FAULT },
	{ "Tx Power Hi Alarm",  "tx_pwr_hi_alarm",    CMIS_FLAG_TX_PWR_HI_ALARM },
	{ "Tx Power Lo Alarm",  "tx_pwr_lo_alarm",    CMIS_FLAG_TX_PWR_LO_ALARM },
	{ "Tx Power Hi Warn",   "tx_pwr_hi_warn",     CMIS_FLAG_TX_PWR_HI_WARN },
	{ "Tx Power Lo Warn",   "tx_pwr_lo_warn",     CMIS_FLAG_TX_PWR_LO_WARN },
	{ "Tx Bias Hi Alarm",   "tx_bias_hi_alarm",   CMIS_FLAG_TX_BIAS_HI_ALARM },
	{ "Tx Bias Lo Alarm",   "tx_bias_lo_alarm",   CMIS_FLAG_TX_BIAS_LO_ALARM },
	{ "Tx Bias Hi Warn",    "tx_bias_hi_warn",    CMIS_FLAG_TX_BIAS_HI_WARN },
	{ "Tx Bias Lo Warn",    "tx_bias_lo_warn",    CMIS_FLAG_TX_BIAS_LO_WARN },
	{ "Rx LOS",             "rx_los",             CMIS_FLAG_RX_LOS },
	{ "Rx CDR LOL",         "rx_cdr_lol",         CMIS_FLAG_RX_CDR_LOL },
	{ "Rx Power Hi Alarm",  "rx_pwr_hi_alarm",    CMIS_FLAG_RX_PWR_HI_ALARM },
	{ "Rx Power Lo Alarm",  "rx_pwr_lo_alarm",    CMIS_FLAG_RX_PWR_LO_ALARM },
	{ "Rx Power Hi Warn",   "rx_pwr_hi_warn",     CMIS_FLAG_RX_PWR_HI_WARN },
	{ "Rx Power Lo Warn",   "rx_pwr_lo_warn",     CMIS_FLAG_RX_PWR_LO_WARN },
};

#define NUM_LANE_FLAGS ARRAY_SIZE(cmis_lane_flags)

static void
cmis_show_dp_lane_flags(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_11h = map->upper_memory[bank][0x11];
	int lanes = cmis_lanes_per_bank(map, bank);
	unsigned int f;

	if (!page_11h)
		return;

	if (is_json_context()) {
		char key[48];

		snprintf(key, sizeof(key), "lane_flags_bank_%d", bank);
		open_json_object(key);

		for (f = 0; f < NUM_LANE_FLAGS; f++) {
			const struct cmis_lane_flag *fl = &cmis_lane_flags[f];
			__u8 byte = page_11h[fl->offset];
			int i;

			open_json_array(fl->json_name, NULL);
			for (i = 0; i < lanes; i++) {
				bool on = !!(byte & (1 << i));

				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u",
					   bank * CMIS_CHANNELS_PER_BANK +
					   i + 1);
				print_bool(PRINT_JSON, "on", NULL, on);
				close_json_object();
			}
			close_json_array(NULL);
		}
		close_json_object();
	} else {
		int i;

		if (bank == 0)
			printf("\n\t%-41s :\n",
			       "Lane Flags (Page 11h, COR)");
		printf("\t  %-22s", "Flag");
		for (i = 0; i < lanes; i++)
			printf("  L%-2d",
			       bank * CMIS_CHANNELS_PER_BANK + i + 1);
		printf("\n");
		printf("\t  %-22s", "----------------------");
		for (i = 0; i < lanes; i++)
			printf("  ---");
		printf("\n");

		for (f = 0; f < NUM_LANE_FLAGS; f++) {
			const struct cmis_lane_flag *fl = &cmis_lane_flags[f];
			__u8 byte = page_11h[fl->offset];

			printf("\t  %-22s", fl->name);
			for (i = 0; i < lanes; i++) {
				bool on = !!(byte & (1 << i));

				printf("  %-3s", on ? "On" : "Off");
			}
			printf("\n");
		}
	}
}

static void
cmis_show_dp_latency(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_15h = map->upper_memory[bank][0x15];
	int lanes = cmis_lanes_per_bank(map, bank);
	int i;

	if (!page_15h)
		return;

	if (is_json_context()) {
		char key[48];

		snprintf(key, sizeof(key), "dp_latency_bank_%d", bank);
		open_json_array(key, NULL);
		for (i = 0; i < lanes; i++) {
			__u16 rx_lat = OFFSET_TO_U16_PTR(page_15h,
						CMIS_DP_RX_LATENCY_LANE(i));
			__u16 tx_lat = OFFSET_TO_U16_PTR(page_15h,
						CMIS_DP_TX_LATENCY_LANE(i));

			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u",
				   bank * CMIS_CHANNELS_PER_BANK + i + 1);
			print_uint(PRINT_JSON, "rx_latency_ns", "%u", rx_lat);
			print_uint(PRINT_JSON, "tx_latency_ns", "%u", tx_lat);
			close_json_object();
		}
		close_json_array(NULL);
	} else {
		if (bank == 0)
			printf("\n\t%-41s :\n",
			       "Data Path Latency (Page 15h)");
		printf("\t  %-6s %10s %10s\n",
		       "Lane", "Rx (ns)", "Tx (ns)");
		printf("\t  %-6s %10s %10s\n",
		       "----", "----------", "----------");

		for (i = 0; i < lanes; i++) {
			int global_lane = bank * CMIS_CHANNELS_PER_BANK +
					  i + 1;
			__u16 rx_lat = OFFSET_TO_U16_PTR(page_15h,
						CMIS_DP_RX_LATENCY_LANE(i));
			__u16 tx_lat = OFFSET_TO_U16_PTR(page_15h,
						CMIS_DP_TX_LATENCY_LANE(i));

			printf("\t  %-6d %10u %10u\n",
			       global_lane, rx_lat, tx_lat);
		}
	}
}

void cmis_show_datapath(const struct cmis_memory_map *map)
{
	int num_banks, bank;
	__u8 banks_bits;

	if (!map->page_01h)
		return;

	/* Page 10h/11h are always present for paged modules (no separate
	 * advertisement bit). Page 11h is already fetched by the core.
	 * Check if we have at least Page 11h.
	 */
	if (!map->upper_memory[0][0x11])
		return;

	banks_bits = map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
		     CMIS_BANKS_SUPPORTED_MASK;
	if (banks_bits == CMIS_BANK_0_3_SUPPORTED)
		num_banks = 4;
	else if (banks_bits == CMIS_BANK_0_1_SUPPORTED)
		num_banks = 2;
	else
		num_banks = 1;

	if (is_json_context()) {
		open_json_object("datapath_status");
	} else {
		printf("\n\t%-41s :\n",
		       "Data Path Control/Status (Pages 10h/11h)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		cmis_show_dp_lanes(map, bank);
		cmis_show_dp_controls(map, bank);
		cmis_show_dp_lane_flags(map, bank);
		cmis_show_dp_latency(map, bank);
	}

	if (is_json_context())
		close_json_object();
}

/*----------------------------------------------------------------------
 * Shared DP State Management Functions
 * Used by cmis-tunable.c and test CLI for DP state transitions.
 *----------------------------------------------------------------------*/

int cmis_dp_read_state(struct cmd_context *ctx, int bank, int lane)
{
	struct module_eeprom request;
	int ret;
	__u8 byte_val, nibble;
	int byte_idx = lane / 2;

	cmis_request_init(&request, bank, 0x11,
			  CMIS_DP_STATE_HOST_OFFSET + byte_idx);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	byte_val = request.data[0];
	if (lane & 1)
		nibble = byte_val & 0x0F;
	else
		nibble = (byte_val >> 4) & 0x0F;

	return nibble;
}

const char *cmis_dp_state_name(int state)
{
	switch (state) {
	case CMIS_DP_STATE_DEACTIVATED:	return "DPDeactivated";
	case CMIS_DP_STATE_INIT:	return "DPInit";
	case CMIS_DP_STATE_DEINIT:	return "DPDeinit";
	case CMIS_DP_STATE_ACTIVATED:	return "DPActivated";
	case CMIS_DP_STATE_TX_TURN_ON:	return "DPTxTurnOn";
	case CMIS_DP_STATE_TX_TURN_OFF:	return "DPTxTurnOff";
	case CMIS_DP_STATE_INITIALIZED:	return "DPInitialized";
	default:			return "Unknown";
	}
}

int cmis_dp_deinit_lane(struct cmd_context *ctx, int bank, int lane)
{
	struct module_eeprom request;
	__u8 buf;
	int ret, elapsed, state;

	/* Read current DPDeinit register */
	cmis_request_init(&request, bank, 0x10, CMIS_DP_DEINIT_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Set the DPDeinit bit for this lane */
	buf = request.data[0] | (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_DP_DEINIT_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Write ApplyDPInit for this lane */
	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_DPINIT);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Poll until DPDeactivated (timeout 5s) */
	for (elapsed = 0; elapsed < 5000; elapsed += 100) {
		usleep(100000);
		state = cmis_dp_read_state(ctx, bank, lane);
		if (state == CMIS_DP_STATE_DEACTIVATED)
			return 0;
	}

	fprintf(stderr, "Timeout waiting for DPDeactivated on bank %d lane %d "
		"(state=0x%x %s)\n", bank, lane, state,
		cmis_dp_state_name(state));
	return -1;
}

int cmis_dp_init_lane(struct cmd_context *ctx, int bank, int lane)
{
	struct module_eeprom request;
	__u8 buf;
	int ret, elapsed, state;

	/* Read current DPDeinit register */
	cmis_request_init(&request, bank, 0x10, CMIS_DP_DEINIT_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Clear the DPDeinit bit for this lane */
	buf = request.data[0] & ~(1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_DP_DEINIT_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Write ApplyDPInit for this lane */
	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_DPINIT);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	/* Poll until Activated or TxTurnOn (timeout 5s) */
	for (elapsed = 0; elapsed < 5000; elapsed += 100) {
		usleep(100000);
		state = cmis_dp_read_state(ctx, bank, lane);
		if (state == CMIS_DP_STATE_ACTIVATED ||
		    state == CMIS_DP_STATE_TX_TURN_ON)
			return 0;
	}

	fprintf(stderr, "Timeout waiting for DPActivated on bank %d lane %d "
		"(state=0x%x %s)\n", bank, lane, state,
		cmis_dp_state_name(state));
	return -1;
}

/*----------------------------------------------------------------------
 * DP Write Operations
 *----------------------------------------------------------------------*/

int cmis_dp_tx_disable(struct cmd_context *ctx, int bank, __u8 lane_mask)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x10, CMIS_OUTPUT_DIS_TX);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = old | lane_mask;

	cmis_request_init(&request, bank, 0x10, CMIS_OUTPUT_DIS_TX);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "OutputDisableTx");
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  OutputDisableTx: 0x%02x -> 0x%02x\n", old, buf);
	}
	return 0;
}

int cmis_dp_tx_enable(struct cmd_context *ctx, int bank, __u8 lane_mask)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x10, CMIS_OUTPUT_DIS_TX);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = old & ~lane_mask;

	cmis_request_init(&request, bank, 0x10, CMIS_OUTPUT_DIS_TX);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "OutputDisableTx");
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  OutputDisableTx: 0x%02x -> 0x%02x\n", old, buf);
	}
	return 0;
}

int cmis_dp_configure_lane(struct cmd_context *ctx, int bank, int lane,
			   const struct cmis_dp_config *cfg)
{
	struct module_eeprom request;
	__u8 buf;
	int ret, state;

	if (lane < 0 || lane >= CMIS_CHANNELS_PER_BANK)
		return -EINVAL;

	/* Step 1: Read current DP state */
	state = cmis_dp_read_state(ctx, bank, lane);
	if (state < 0) {
		fprintf(stderr, "Cannot read DP state for bank %d lane %d\n",
			bank, lane);
		return -1;
	}

	if (!is_json_context())
		printf("  Lane %d: current DP state = 0x%x (%s)\n",
		       lane, state, cmis_dp_state_name(state));

	/* Step 2: Deinit if not already DPDeactivated */
	if (state != CMIS_DP_STATE_DEACTIVATED) {
		if (!is_json_context())
			printf("  Lane %d: transitioning to DPDeactivated...\n",
			       lane);
		ret = cmis_dp_deinit_lane(ctx, bank, lane);
		if (ret < 0)
			return ret;
		if (!is_json_context())
			printf("  Lane %d: DPDeactivated\n", lane);
	}

	/* Step 3: Write DPConfigLane: AppSel<<4 | DPID<<1 | Explicit */
	buf = 0;
	if (cfg->appsel > 0)
		buf |= (cfg->appsel & 0x0F) << CMIS_DPCONFIG_APPSEL_SHIFT;
	buf |= (cfg->dpid & 0x07) << CMIS_DPCONFIG_DPID_SHIFT;
	if (cfg->explicit_ctl)
		buf |= CMIS_DPCONFIG_EXPLICIT_MASK;

	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_DPCONFIG_LANE(lane));
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (!is_json_context())
		printf("  Lane %d: DPConfig written (AppSel=%d DPID=%d Explicit=%d)\n",
		       lane, cfg->appsel, cfg->dpid, cfg->explicit_ctl);

	/* Step 4: Re-activate */
	if (!is_json_context())
		printf("  Lane %d: re-activating data path...\n", lane);
	ret = cmis_dp_init_lane(ctx, bank, lane);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "DPConfig");
		print_uint(PRINT_JSON, "lane", "%u", lane);
		print_uint(PRINT_JSON, "app_sel", "%u", cfg->appsel);
		print_uint(PRINT_JSON, "dp_id", "%u", cfg->dpid);
		print_bool(PRINT_JSON, "explicit_control", NULL,
			   cfg->explicit_ctl);
		close_json_object();
	} else {
		printf("  Lane %d: data path activated\n", lane);
	}
	return 0;
}

int cmis_dp_set_cdr(struct cmd_context *ctx, int bank, __u8 lane_mask,
		    bool tx_on, bool rx_on)
{
	struct module_eeprom request;
	__u8 buf, old_tx, old_rx;
	int ret;

	/* Read current CDR Tx */
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_CDR_ENABLE_TX);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;
	old_tx = request.data[0];

	/* Write CDR Tx */
	if (tx_on)
		buf = old_tx | lane_mask;
	else
		buf = old_tx & ~lane_mask;
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_CDR_ENABLE_TX);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;
	if (!is_json_context())
		printf("  CDREnableTx: 0x%02x -> 0x%02x\n", old_tx, buf);

	/* Read current CDR Rx */
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_CDR_ENABLE_RX);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;
	old_rx = request.data[0];

	/* Write CDR Rx */
	if (rx_on)
		buf = old_rx | lane_mask;
	else
		buf = old_rx & ~lane_mask;
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_CDR_ENABLE_RX);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;
	if (!is_json_context())
		printf("  CDREnableRx: 0x%02x -> 0x%02x\n", old_rx, buf);

	/* Apply immediately */
	buf = lane_mask;
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_IMMEDIATE);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "CDREnable");
		print_uint(PRINT_JSON, "old_value_tx", "0x%02x", old_tx);
		print_uint(PRINT_JSON, "new_value_tx", "0x%02x",
			   tx_on ? (old_tx | lane_mask) :
				   (old_tx & ~lane_mask));
		print_uint(PRINT_JSON, "old_value_rx", "0x%02x", old_rx);
		print_uint(PRINT_JSON, "new_value_rx", "0x%02x",
			   rx_on ? (old_rx | lane_mask) :
				   (old_rx & ~lane_mask));
		close_json_object();
	} else {
		printf("  ApplyImmediate: 0x%02x\n", lane_mask);
	}
	return 0;
}

/*----------------------------------------------------------------------
 * Generic read-modify-write helper for 1-byte Page 10h lane registers.
 * If 'set' is true, bits in lane_mask are set; otherwise cleared.
 */
static int
dp_rmw_byte(struct cmd_context *ctx, int bank, __u8 offset,
	    __u8 lane_mask, bool set, const char *name)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x10, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = set ? (old | lane_mask) : (old & ~lane_mask);

	cmis_request_init(&request, bank, 0x10, offset);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", name);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  %s: 0x%02x -> 0x%02x\n", name, old, buf);
	}
	return 0;
}

/*----------------------------------------------------------------------
 * Generic read-modify-write for 4-bit nibble fields (EQ targets, etc.)
 * lane: 0-7, target: 0-15.
 */
static int
dp_rmw_nibble(struct cmd_context *ctx, int bank, __u8 base_offset,
	      int lane, __u8 target, const char *name)
{
	struct module_eeprom request;
	__u8 byte_off, buf, old, shift;
	int ret;

	if (lane < 0 || lane >= CMIS_CHANNELS_PER_BANK || target > 15)
		return -EINVAL;

	byte_off = base_offset + lane / 2;
	shift = (lane & 1) ? 4 : 0;

	cmis_request_init(&request, bank, 0x10, byte_off);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = (old & ~(0x0F << shift)) | ((target & 0x0F) << shift);

	cmis_request_init(&request, bank, 0x10, byte_off);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", name);
		print_uint(PRINT_JSON, "lane", "%u", lane);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		print_uint(PRINT_JSON, "target", "%u", target);
		close_json_object();
	} else {
		printf("  %s lane %d: 0x%02x -> 0x%02x (target=%u)\n",
		       name, lane, old, buf, target);
	}
	return 0;
}

/*----------------------------------------------------------------------
 * Page 10h Direct-Effect Lane Controls
 */

int cmis_dp_set_polarity_tx(struct cmd_context *ctx, int bank,
			    __u8 lane_mask, bool flip)
{
	return dp_rmw_byte(ctx, bank, CMIS_INPUT_POL_FLIP_TX,
			   lane_mask, flip, "InputPolarityFlipTx");
}

int cmis_dp_set_polarity_rx(struct cmd_context *ctx, int bank,
			    __u8 lane_mask, bool flip)
{
	return dp_rmw_byte(ctx, bank, CMIS_OUTPUT_POL_FLIP_RX,
			   lane_mask, flip, "OutputPolarityFlipRx");
}

int cmis_dp_rx_disable(struct cmd_context *ctx, int bank, __u8 lane_mask)
{
	return dp_rmw_byte(ctx, bank, CMIS_OUTPUT_DIS_RX,
			   lane_mask, true, "OutputDisableRx");
}

int cmis_dp_rx_enable(struct cmd_context *ctx, int bank, __u8 lane_mask)
{
	return dp_rmw_byte(ctx, bank, CMIS_OUTPUT_DIS_RX,
			   lane_mask, false, "OutputDisableRx");
}

int cmis_dp_set_auto_squelch_tx(struct cmd_context *ctx, int bank,
				__u8 lane_mask, bool disable)
{
	return dp_rmw_byte(ctx, bank, CMIS_AUTO_SQUELCH_DIS_TX,
			   lane_mask, disable, "AutoSquelchDisableTx");
}

int cmis_dp_set_squelch_force_tx(struct cmd_context *ctx, int bank,
				 __u8 lane_mask, bool force)
{
	return dp_rmw_byte(ctx, bank, CMIS_OUTPUT_SQUELCH_FORCE_TX,
			   lane_mask, force, "OutputSquelchForceTx");
}

int cmis_dp_set_auto_squelch_rx(struct cmd_context *ctx, int bank,
				__u8 lane_mask, bool disable)
{
	return dp_rmw_byte(ctx, bank, CMIS_AUTO_SQUELCH_DIS_RX,
			   lane_mask, disable, "AutoSquelchDisableRx");
}

int cmis_dp_set_adapt_eq_freeze_tx(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool freeze)
{
	return dp_rmw_byte(ctx, bank, CMIS_ADAPT_INPUT_EQ_FREEZE_TX,
			   lane_mask, freeze, "AdaptiveInputEqFreezeTx");
}

/*----------------------------------------------------------------------
 * Page 10h SCS0 Staged Controls
 */

int cmis_dp_set_adapt_eq_enable_tx(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool enable)
{
	int ret;

	ret = dp_rmw_byte(ctx, bank, CMIS_SCS0_ADAPT_EQ_TX,
			  lane_mask, enable, "AdaptiveInputEqEnableTx");
	if (ret < 0)
		return ret;

	/* Apply immediately */
	return dp_rmw_byte(ctx, bank, CMIS_SCS0_APPLY_IMMEDIATE,
			   lane_mask, true, "ApplyImmediate");
}

int cmis_dp_set_fixed_eq_tx(struct cmd_context *ctx, int bank,
			    int lane, __u8 target)
{
	struct module_eeprom request;
	__u8 buf;
	int ret;

	ret = dp_rmw_nibble(ctx, bank, 0x9C, lane, target,
			    "FixedInputTargetTx");
	if (ret < 0)
		return ret;

	/* Apply immediately for this lane */
	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_IMMEDIATE);
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_dp_set_rx_eq_pre(struct cmd_context *ctx, int bank,
			  int lane, __u8 target)
{
	struct module_eeprom request;
	__u8 buf;
	int ret;

	ret = dp_rmw_nibble(ctx, bank, 0xA2, lane, target,
			    "OutputEqPreCursorTargetRx");
	if (ret < 0)
		return ret;

	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_IMMEDIATE);
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_dp_set_rx_eq_post(struct cmd_context *ctx, int bank,
			   int lane, __u8 target)
{
	struct module_eeprom request;
	__u8 buf;
	int ret;

	ret = dp_rmw_nibble(ctx, bank, 0xA6, lane, target,
			    "OutputEqPostCursorTargetRx");
	if (ret < 0)
		return ret;

	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_IMMEDIATE);
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_dp_set_rx_amplitude(struct cmd_context *ctx, int bank,
			     int lane, __u8 target)
{
	struct module_eeprom request;
	__u8 buf;
	int ret;

	ret = dp_rmw_nibble(ctx, bank, 0xAA, lane, target,
			    "OutputAmplitudeTargetRx");
	if (ret < 0)
		return ret;

	buf = (1 << lane);
	cmis_request_init(&request, bank, 0x10, CMIS_SCS0_APPLY_IMMEDIATE);
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

/*----------------------------------------------------------------------
 * Page 10h Lane-Specific Masks
 */

int cmis_dp_set_lane_mask(struct cmd_context *ctx, int bank,
			  __u8 offset, __u8 lane_mask, bool enable)
{
	char name[48];

	snprintf(name, sizeof(name), "LaneMask[0x%02x]", offset);
	return dp_rmw_byte(ctx, bank, offset, lane_mask, enable, name);
}

/*----------------------------------------------------------------------
 * Module Global Controls (Lower Memory, Page 00h)
 */

static int
module_rmw_byte(struct cmd_context *ctx, __u8 offset, __u8 mask, bool set,
		const char *name)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, 0, 0x0, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = set ? (old | mask) : (old & ~mask);

	cmis_request_init(&request, 0, 0x0, offset);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", name);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  %s: 0x%02x -> 0x%02x\n", name, old, buf);
	}
	return 0;
}

int cmis_module_sw_reset(struct cmd_context *ctx)
{
	struct module_eeprom request;
	__u8 buf;
	int ret;

	/* Read control register, set SW reset bit (self-clearing) */
	cmis_request_init(&request, 0, 0x0, CMIS_MODULE_CONTROL_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	buf = request.data[0] | CMIS_MODULE_CTL_SW_RESET;
	cmis_request_init(&request, 0, 0x0, CMIS_MODULE_CONTROL_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "SoftwareReset");
		print_string(PRINT_JSON, "result", "%s", "triggered");
		close_json_object();
	} else {
		printf("  Software reset triggered\n");
	}
	return 0;
}

int cmis_module_set_bank_broadcast(struct cmd_context *ctx, bool enable)
{
	return module_rmw_byte(ctx, CMIS_MODULE_CONTROL_OFFSET,
			       CMIS_MODULE_CTL_BANK_BROADCAST, enable,
			       "BankBroadcastEnable");
}

int cmis_module_set_squelch_method(struct cmd_context *ctx, bool pav)
{
	return module_rmw_byte(ctx, CMIS_MODULE_CONTROL_OFFSET,
			       CMIS_MODULE_CTL_SQUELCH_METHOD, pav,
			       "SquelchMethodSelect");
}

int cmis_module_set_mask(struct cmd_context *ctx, __u8 offset,
			 __u8 mask, bool enable)
{
	char name[48];

	snprintf(name, sizeof(name), "ModuleMask[0x%02x]", offset);
	return module_rmw_byte(ctx, offset, mask, enable, name);
}

int cmis_module_password_entry(struct cmd_context *ctx, __u32 password)
{
	struct module_eeprom request;
	__u8 buf[4];
	int ret;

	buf[0] = (password >> 24) & 0xFF;
	buf[1] = (password >> 16) & 0xFF;
	buf[2] = (password >> 8) & 0xFF;
	buf[3] = password & 0xFF;

	cmis_request_init(&request, 0, 0x0, CMIS_PASSWORD_ENTRY_OFFSET);
	request.length = 4;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("password_entry");
		print_string(PRINT_JSON, "result", "%s", "success");
		close_json_object();
	} else {
		printf("  Password entered\n");
	}
	return 0;
}

int cmis_module_password_change(struct cmd_context *ctx,
				__u32 current_pw, __u32 new_pw)
{
	struct module_eeprom request;
	__u8 buf[4];
	int ret;

	/* First enter current password */
	ret = cmis_module_password_entry(ctx, current_pw);
	if (ret < 0)
		return ret;

	/* Then write new password */
	buf[0] = (new_pw >> 24) & 0xFF;
	buf[1] = (new_pw >> 16) & 0xFF;
	buf[2] = (new_pw >> 8) & 0xFF;
	buf[3] = new_pw & 0xFF;

	cmis_request_init(&request, 0, 0x0, CMIS_PASSWORD_CHANGE_OFFSET);
	request.length = 4;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("password_change");
		print_string(PRINT_JSON, "result", "%s", "success");
		close_json_object();
	} else {
		printf("  Password changed\n");
	}
	return 0;
}

/*----------------------------------------------------------------------
 * User EEPROM (Page 03h)
 */

int cmis_user_eeprom_read(struct cmd_context *ctx, int offset,
			  __u8 *buf, int len)
{
	struct module_eeprom request;
	int ret;

	if (offset < 0 || len < 1 ||
	    offset + len > CMIS_USER_EEPROM_SIZE)
		return -EINVAL;

	cmis_request_init(&request, 0, CMIS_USER_EEPROM_PAGE,
			  CMIS_USER_EEPROM_OFFSET + offset);
	request.length = len;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	memcpy(buf, request.data, len);
	return 0;
}

int cmis_user_eeprom_write(struct cmd_context *ctx, int offset,
			   const __u8 *data, int len)
{
	struct module_eeprom request;
	__u8 buf[CMIS_USER_EEPROM_MAX_WRITE];

	if (offset < 0 || len < 1 ||
	    len > CMIS_USER_EEPROM_MAX_WRITE ||
	    offset + len > CMIS_USER_EEPROM_SIZE)
		return -EINVAL;

	memcpy(buf, data, len);
	cmis_request_init(&request, 0, CMIS_USER_EEPROM_PAGE,
			  CMIS_USER_EEPROM_OFFSET + offset);
	request.length = len;
	request.data = buf;
	return set_eeprom_page(ctx, &request);
}

/*----------------------------------------------------------------------
 * Host Lane Switching (Page 1Dh)
 */

int cmis_lane_switch_set_redir(struct cmd_context *ctx, int bank,
			       int lane, __u8 redir_lane)
{
	struct module_eeprom request;
	__u8 buf;

	if (lane < 0 || lane >= 8 || redir_lane > 7)
		return -EINVAL;

	buf = redir_lane;
	cmis_request_init(&request, bank, 0x1D,
			  CMIS_LANE_SWITCH_REDIR_LANE(lane));
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_lane_switch_enable(struct cmd_context *ctx, int bank, bool enable)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x1D, CMIS_LANE_SWITCH_ENABLE);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = enable ? (old | 0x01) : (old & ~0x01);

	cmis_request_init(&request, bank, 0x1D, CMIS_LANE_SWITCH_ENABLE);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s",
			     "LaneSwitchEnable");
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  LaneSwitchEnable: 0x%02x -> 0x%02x\n", old, buf);
	}
	return 0;
}

int cmis_lane_switch_commit(struct cmd_context *ctx, int bank)
{
	struct module_eeprom request;
	__u8 buf = 0x01;

	cmis_request_init(&request, bank, 0x1D, CMIS_LANE_SWITCH_COMMIT);
	request.length = 1;
	request.data = &buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_lane_switch_result(struct cmd_context *ctx, int bank)
{
	struct module_eeprom request;
	int ret;

	cmis_request_init(&request, bank, 0x1D, CMIS_LANE_SWITCH_RESULT);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	return request.data[0];
}
