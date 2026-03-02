/**
 * cmis-netpath.c: CMIS Network Path Control and Status (Pages 16h-17h)
 *
 * Displays per-lane Network Path state, active NP configuration,
 * signal source selection, and NP capabilities per
 * OIF-CMIS-05.3 Tables 8-132 through 8-154.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-netpath.h"

/* Page convenience macros for struct cmis_memory_map */
#define page_01h upper_memory[0x0][0x1]

static const char *cmis_np_state_str(__u8 state)
{
	switch (state) {
	case CMIS_NP_STATE_DEACTIVATED:
		return "NPDeactivated";
	case CMIS_NP_STATE_INIT:
		return "NPInit";
	case CMIS_NP_STATE_DEINIT:
		return "NPDeinit";
	case CMIS_NP_STATE_ACTIVATED:
		return "NPActivated";
	case CMIS_NP_STATE_TX_TURN_ON:
		return "NPTxTurnOn";
	case CMIS_NP_STATE_TX_TURN_OFF:
		return "NPTxTurnOff";
	case CMIS_NP_STATE_INITIALIZED:
		return "NPInitialized";
	default:
		return "Reserved";
	}
}

static const char *cmis_np_config_status_str(__u8 cs)
{
	switch (cs) {
	case CMIS_NP_CFGSTAT_UNDEFINED:
		return "Undefined";
	case CMIS_NP_CFGSTAT_SUCCESS:
		return "Success";
	case CMIS_NP_CFGSTAT_REJECTED:
		return "Rejected";
	case CMIS_NP_CFGSTAT_REJECTED_INVALID_APPSEL:
		return "RejectedInvalidAppSel";
	case CMIS_NP_CFGSTAT_REJECTED_INVALID_NP:
		return "RejectedInvalidNP";
	case CMIS_NP_CFGSTAT_REJECTED_LANES_IN_USE:
		return "RejectedLanesInUse";
	case CMIS_NP_CFGSTAT_REJECTED_PARTIAL:
		return "RejectedPartialNP";
	case CMIS_NP_CFGSTAT_IN_PROGRESS:
		return "InProgress";
	default:
		return "Reserved";
	}
}

/*
 * Get NP state for a host lane (0-7) from Page 16h.
 * 4-bit nibbles packed two per byte at bytes 200-203.
 * Even lane index -> low nibble, odd lane index -> high nibble.
 */
static __u8 np_state_for_lane(const __u8 *page_16h, int lane)
{
	int byte_off = CMIS_NP_STATE_OFFSET + lane / 2;
	__u8 byte = page_16h[byte_off];

	if (lane & 1)
		return (byte >> 4) & 0x0F;
	return byte & 0x0F;
}

/*
 * Get NP config status for a host lane (0-7) from Page 16h.
 * 4-bit nibbles packed two per byte at bytes 178-181.
 */
static __u8 np_config_status_for_lane(const __u8 *page_16h, int lane)
{
	int byte_off = CMIS_NP_CONFIG_STATUS_OFFSET + lane / 2;
	__u8 byte = page_16h[byte_off];

	if (lane & 1)
		return (byte >> 4) & 0x0F;
	return byte & 0x0F;
}

/* Duration encoding per Table 8-48 */
static const char *cmis_duration_str(__u8 code)
{
	static const char * const dur[] = {
		"< 1 ms",    "< 5 ms",    "< 10 ms",   "< 50 ms",
		"< 100 ms",  "< 500 ms",  "< 1 s",     "< 5 s",
		"< 10 s",    "< 30 s",    "< 1 min",    "< 5 min",
		"< 10 min",  "< 30 min",  "< 1 hr",     "N/A",
	};

	return dur[code & 0x0F];
}

static void
cmis_show_np_lanes(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_16h = map->upper_memory[bank][0x16];
	const __u8 *page_17h = map->upper_memory[bank][0x17];
	int lanes = cmis_lanes_per_bank(map, bank);
	int i;

	if (!page_16h)
		return;

	if (is_json_context()) {
		char key[32];

		snprintf(key, sizeof(key), "netpath_bank_%d", bank);
		open_json_array(key, NULL);
	} else {
		if (bank > 0)
			printf("\t%-41s :\n",
			       "Network Path Status (continued)");
		printf("\t  %-6s %-15s %-5s %-7s %-10s %-6s %-6s %-8s\n",
		       "Lane", "NPState", "NPID", "InUse",
		       "Config", "HPSrc", "NPSrc", "Chg");
		printf("\t  %-6s %-15s %-5s %-7s %-10s %-6s %-6s %-8s\n",
		       "----", "---------------", "-----", "-------",
		       "----------", "------", "------", "--------");
	}

	for (i = 0; i < lanes; i++) {
		int global_lane = bank * CMIS_CHANNELS_PER_BANK + i + 1;
		__u8 np_state = np_state_for_lane(page_16h, i);
		__u8 cfg_status = np_config_status_for_lane(page_16h, i);
		__u8 acs_byte = page_16h[CMIS_NP_ACS_CONFIG_LANE(i)];
		__u8 npid = (acs_byte & CMIS_NP_NPID_MASK) >>
			    CMIS_NP_NPID_SHIFT;
		bool in_use = acs_byte & CMIS_NP_INUSE_MASK;
		bool hp_src_rx = page_16h[CMIS_NP_HPSOURCE_RX] & (1 << i);
		bool np_src_tx = page_16h[CMIS_NP_NPSOURCE_TX] & (1 << i);
		bool state_changed = false;

		if (page_17h)
			state_changed = page_17h[CMIS_NP_STATE_CHANGED_FLAG] &
					(1 << i);

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u", global_lane);
			print_string(PRINT_JSON, "np_state", "%s",
				     cmis_np_state_str(np_state));
			print_uint(PRINT_JSON, "np_id", "%u", npid);
			print_bool(PRINT_JSON, "np_in_use", NULL, in_use);
			print_string(PRINT_JSON, "config_status", "%s",
				     cmis_np_config_status_str(cfg_status));
			print_bool(PRINT_JSON, "hp_source_rx_replace",
				   NULL, hp_src_rx);
			print_bool(PRINT_JSON, "np_source_tx_replace",
				   NULL, np_src_tx);
			print_bool(PRINT_JSON, "state_changed",
				   NULL, state_changed);
			close_json_object();
		} else {
			printf("\t  %-6d %-15s %-5d %-7s %-10s %-6s %-6s %s\n",
			       global_lane,
			       cmis_np_state_str(np_state),
			       npid,
			       in_use ? "Yes" : "No",
			       cmis_np_config_status_str(cfg_status),
			       hp_src_rx ? "Repl" : "NP",
			       np_src_tx ? "Repl" : "HP",
			       state_changed ? "Yes" : "No");
		}
	}

	if (is_json_context())
		close_json_array(NULL);
}

static void
cmis_show_np_controls(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_16h = map->upper_memory[bank][0x16];
	int lanes = cmis_lanes_per_bank(map, bank);
	int i;

	if (!page_16h)
		return;

	if (is_json_context()) {
		char key[48];

		snprintf(key, sizeof(key), "netpath_control_bank_%d", bank);
		open_json_array(key, NULL);
	} else {
		printf("\n");
		if (bank == 0)
			printf("\t%-41s :\n",
			       "Staged NP Config (Page 16h SCS0)");
		printf("\t  %-6s %-8s %-5s %-7s %-10s\n",
		       "Lane", "NPDeinit", "NPID", "InUse", "InitPend");
		printf("\t  %-6s %-8s %-5s %-7s %-10s\n",
		       "----", "--------", "-----", "-------", "----------");
	}

	for (i = 0; i < lanes; i++) {
		int global_lane = bank * CMIS_CHANNELS_PER_BANK + i + 1;
		bool np_deinit = page_16h[CMIS_NP_DEINIT_OFFSET] & (1 << i);
		__u8 scs_byte = page_16h[CMIS_NP_SCS0_CONFIG_LANE(i)];
		__u8 npid = (scs_byte & CMIS_NP_NPID_MASK) >>
			    CMIS_NP_NPID_SHIFT;
		bool in_use = scs_byte & CMIS_NP_INUSE_MASK;
		bool init_pending = page_16h[CMIS_NP_INIT_PENDING] &
				    (1 << i);

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u", global_lane);
			print_bool(PRINT_JSON, "np_deinit",
				   NULL, np_deinit);
			print_uint(PRINT_JSON, "np_id", "%u", npid);
			print_bool(PRINT_JSON, "np_in_use", NULL, in_use);
			print_bool(PRINT_JSON, "np_init_pending",
				   NULL, init_pending);
			close_json_object();
		} else {
			printf("\t  %-6d %-8s %-5d %-7s %s\n",
			       global_lane,
			       np_deinit ? "Yes" : "No",
			       npid,
			       in_use ? "Yes" : "No",
			       init_pending ? "Yes" : "No");
		}
	}

	if (is_json_context())
		close_json_array(NULL);
}

static void
cmis_show_np_capabilities(const struct cmis_memory_map *map, int bank)
{
	const __u8 *page_16h = map->upper_memory[bank][0x16];
	__u8 misc, dur_init_deinit, dur_txon_txoff;

	if (!page_16h || bank > 0)
		return;

	misc = page_16h[CMIS_NP_MISC_OPTIONS];
	dur_init_deinit = page_16h[CMIS_NP_MAX_DURATION_INIT];
	dur_txon_txoff = page_16h[CMIS_NP_MAX_DURATION_TXON];

	if (is_json_context()) {
		open_json_object("netpath_capabilities");
		print_bool(PRINT_JSON, "replace_hp_signal_tx_supported",
			   NULL,
			   !!(misc & CMIS_NP_REPLACE_HP_TX_SUPPORTED));
		print_bool(PRINT_JSON, "replace_hp_signal_rx_supported",
			   NULL,
			   !!(misc & CMIS_NP_REPLACE_HP_RX_SUPPORTED));
		print_string(PRINT_JSON, "max_np_init", "%s",
			     cmis_duration_str(dur_init_deinit & 0x0F));
		print_string(PRINT_JSON, "max_np_deinit", "%s",
			     cmis_duration_str(dur_init_deinit >> 4));
		print_string(PRINT_JSON, "max_np_tx_turn_on", "%s",
			     cmis_duration_str(dur_txon_txoff & 0x0F));
		print_string(PRINT_JSON, "max_np_tx_turn_off", "%s",
			     cmis_duration_str(dur_txon_txoff >> 4));
		close_json_object();
	} else {
		printf("\n\t%-41s :\n",
		       "Network Path Capabilities");
		printf("\t  %-41s : %s\n",
		       "Replace HP Signal Tx",
		       (misc & CMIS_NP_REPLACE_HP_TX_SUPPORTED) ?
		       "Supported" : "Not supported");
		printf("\t  %-41s : %s\n",
		       "Replace HP Signal Rx",
		       (misc & CMIS_NP_REPLACE_HP_RX_SUPPORTED) ?
		       "Supported" : "Not supported");
		printf("\t  %-41s : %s\n",
		       "Max NPInit duration",
		       cmis_duration_str(dur_init_deinit & 0x0F));
		printf("\t  %-41s : %s\n",
		       "Max NPDeinit duration",
		       cmis_duration_str(dur_init_deinit >> 4));
		printf("\t  %-41s : %s\n",
		       "Max NPTxTurnOn duration",
		       cmis_duration_str(dur_txon_txoff & 0x0F));
		printf("\t  %-41s : %s\n",
		       "Max NPTxTurnOff duration",
		       cmis_duration_str(dur_txon_txoff >> 4));
	}
}

/*----------------------------------------------------------------------
 * NP Write Operations
 *----------------------------------------------------------------------*/

static int
np_write_byte(struct cmd_context *ctx, int bank, __u8 offset, __u8 val)
{
	struct module_eeprom request;

	cmis_request_init(&request, bank, 0x16, offset);
	request.length = 1;
	request.data = &val;
	return set_eeprom_page(ctx, &request);
}

static int
np_rmw_byte(struct cmd_context *ctx, int bank, __u8 offset,
	    __u8 lane_mask, bool set, const char *name)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x16, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = set ? (old | lane_mask) : (old & ~lane_mask);

	cmis_request_init(&request, bank, 0x16, offset);
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

/* Read NP state for a lane (0-7). Returns 4-bit state, or -1 on error. */
static int
np_read_state(struct cmd_context *ctx, int bank, int lane)
{
	struct module_eeprom request;
	int ret;
	__u8 byte_val;

	cmis_request_init(&request, bank, 0x16,
			  CMIS_NP_STATE_OFFSET + lane / 2);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	byte_val = request.data[0];
	if (lane & 1)
		return (byte_val >> 4) & 0x0F;
	return byte_val & 0x0F;
}

int cmis_np_deinit_lane(struct cmd_context *ctx, int bank, int lane)
{
	int ret, elapsed, state;
	__u8 buf;

	/* Set NPDeinit bit for this lane */
	ret = np_rmw_byte(ctx, bank, CMIS_NP_DEINIT_OFFSET,
			  (1 << lane), true, "NPDeinitLane");
	if (ret < 0)
		return ret;

	/* Apply SCS0 for this lane */
	buf = (1 << lane);
	ret = np_write_byte(ctx, bank, CMIS_NP_APPLY_SCS0, buf);
	if (ret < 0)
		return ret;

	/* Poll until NPDeactivated (timeout 5s) */
	for (elapsed = 0; elapsed < 5000; elapsed += 100) {
		usleep(100000);
		state = np_read_state(ctx, bank, lane);
		if (state == CMIS_NP_STATE_DEACTIVATED)
			return 0;
	}

	fprintf(stderr, "Timeout waiting for NPDeactivated on bank %d "
		"lane %d (state=0x%x %s)\n", bank, lane, state,
		cmis_np_state_str(state));
	return -1;
}

int cmis_np_init_lane(struct cmd_context *ctx, int bank, int lane)
{
	int ret, elapsed, state;
	__u8 buf;

	/* Clear NPDeinit bit for this lane */
	ret = np_rmw_byte(ctx, bank, CMIS_NP_DEINIT_OFFSET,
			  (1 << lane), false, "NPDeinitLane");
	if (ret < 0)
		return ret;

	/* Apply SCS0 for this lane */
	buf = (1 << lane);
	ret = np_write_byte(ctx, bank, CMIS_NP_APPLY_SCS0, buf);
	if (ret < 0)
		return ret;

	/* Poll until NPActivated or NPTxTurnOn (timeout 5s) */
	for (elapsed = 0; elapsed < 5000; elapsed += 100) {
		usleep(100000);
		state = np_read_state(ctx, bank, lane);
		if (state == CMIS_NP_STATE_ACTIVATED ||
		    state == CMIS_NP_STATE_TX_TURN_ON)
			return 0;
	}

	fprintf(stderr, "Timeout waiting for NPActivated on bank %d "
		"lane %d (state=0x%x %s)\n", bank, lane, state,
		cmis_np_state_str(state));
	return -1;
}

int cmis_np_configure_lane(struct cmd_context *ctx, int bank, int lane,
			   const struct cmis_np_config *cfg)
{
	struct module_eeprom request;
	__u8 buf;
	int ret, state;

	if (lane < 0 || lane >= 8)
		return -1;

	/* Check current NP state */
	state = np_read_state(ctx, bank, lane);
	if (state < 0)
		return -1;

	if (!is_json_context())
		printf("  NP lane %d: current state = 0x%x (%s)\n",
		       lane, state, cmis_np_state_str(state));

	/* Deinit if not already NPDeactivated */
	if (state != CMIS_NP_STATE_DEACTIVATED) {
		ret = cmis_np_deinit_lane(ctx, bank, lane);
		if (ret < 0)
			return ret;
		if (!is_json_context())
			printf("  NP lane %d: NPDeactivated\n", lane);
	}

	/* Write SCS0 NPConfigLane: NPID<<4 | InUse */
	buf = ((cfg->npid & 0x0F) << CMIS_NP_NPID_SHIFT);
	if (cfg->in_use)
		buf |= CMIS_NP_INUSE_MASK;

	cmis_request_init(&request, bank, 0x16, CMIS_NP_SCS0_CONFIG_LANE(lane));
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (!is_json_context())
		printf("  NP lane %d: config written (NPID=%d InUse=%d)\n",
		       lane, cfg->npid, cfg->in_use);

	/* Re-activate */
	ret = cmis_np_init_lane(ctx, bank, lane);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "NPConfig");
		print_uint(PRINT_JSON, "lane", "%u", lane);
		print_uint(PRINT_JSON, "np_id", "%u", cfg->npid);
		print_bool(PRINT_JSON, "np_in_use", NULL, cfg->in_use);
		close_json_object();
	} else {
		printf("  NP lane %d: activated\n", lane);
	}
	return 0;
}

int cmis_np_set_hp_source_rx(struct cmd_context *ctx, int bank,
			     __u8 lane_mask, bool replace)
{
	return np_rmw_byte(ctx, bank, CMIS_NP_HPSOURCE_RX,
			   lane_mask, replace, "HPSourceRx");
}

int cmis_np_set_np_source_tx(struct cmd_context *ctx, int bank,
			     __u8 lane_mask, bool replace)
{
	return np_rmw_byte(ctx, bank, CMIS_NP_NPSOURCE_TX,
			   lane_mask, replace, "NPSourceTx");
}

int cmis_np_set_state_mask(struct cmd_context *ctx, int bank,
			   __u8 lane_mask, bool mask)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x17, CMIS_NP_STATE_CHANGED_MASK);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = mask ? (old | lane_mask) : (old & ~lane_mask);

	cmis_request_init(&request, bank, 0x17, CMIS_NP_STATE_CHANGED_MASK);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s",
			     "NPStateChangedMask");
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  NPStateChangedMask: 0x%02x -> 0x%02x\n", old, buf);
	}
	return 0;
}

void cmis_show_netpath(const struct cmis_memory_map *map)
{
	int num_banks, bank;
	__u8 banks_bits;

	if (!map->page_01h)
		return;

	/* Network Path is advertised via 01h:142 bit 7 */
	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_NETWORK_PATH))
		return;

	if (!map->upper_memory[0][0x16])
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
		open_json_object("netpath_status");
	} else {
		printf("\n\t%-41s :\n",
		       "Network Path Control/Status (Pages 16h/17h)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		cmis_show_np_lanes(map, bank);
		cmis_show_np_controls(map, bank);
	}

	cmis_show_np_capabilities(map, 0);

	if (is_json_context())
		close_json_object();
}
