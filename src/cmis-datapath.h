/**
 * cmis-datapath.h: CMIS Data Path Control and Status (Pages 10h/11h)
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_DATAPATH_H__
#define CMIS_DATAPATH_H__

#include "cmis-internal.h"

void cmis_show_datapath(const struct cmis_memory_map *map);

/*-----------------------------------------------------------------------
 * DP State Management — shared by tunable, datapath, and test code
 */

/* Read DP state for a lane. Returns 4-bit state value, or -1 on error. */
int cmis_dp_read_state(struct cmd_context *ctx, int bank, int lane);

/* Return human-readable name for a DP state value. */
const char *cmis_dp_state_name(int state);

/* Deactivate a lane (DPDeinit + ApplyDPInit, poll for DPDeactivated). */
int cmis_dp_deinit_lane(struct cmd_context *ctx, int bank, int lane);

/* Re-activate a lane (clear DPDeinit + ApplyDPInit, poll for DPActivated). */
int cmis_dp_init_lane(struct cmd_context *ctx, int bank, int lane);

/*-----------------------------------------------------------------------
 * DP Write Operations — Tx disable, CDR, AppSel configuration
 */

/* Disable Tx output for lanes specified in lane_mask. */
int cmis_dp_tx_disable(struct cmd_context *ctx, int bank, __u8 lane_mask);

/* Enable Tx output for lanes specified in lane_mask. */
int cmis_dp_tx_enable(struct cmd_context *ctx, int bank, __u8 lane_mask);

/* DP lane configuration for AppSel/DPID programming. */
struct cmis_dp_config {
	__u8 appsel;       /* 1-15, or 0 = don't change */
	__u8 dpid;         /* 0-7 */
	bool explicit_ctl; /* explicit control mode */
};

/* Configure a lane's AppSel/DPID (deinit, write, reinit). */
int cmis_dp_configure_lane(struct cmd_context *ctx, int bank, int lane,
			   const struct cmis_dp_config *cfg);

/* Set CDR enable for lanes specified in lane_mask. */
int cmis_dp_set_cdr(struct cmd_context *ctx, int bank, __u8 lane_mask,
		    bool tx_on, bool rx_on);

/*-----------------------------------------------------------------------
 * Page 10h Direct-Effect Lane Controls
 */

/* Set/clear Tx input polarity flip for lanes in lane_mask. */
int cmis_dp_set_polarity_tx(struct cmd_context *ctx, int bank,
			    __u8 lane_mask, bool flip);

/* Set/clear Rx output polarity flip for lanes in lane_mask. */
int cmis_dp_set_polarity_rx(struct cmd_context *ctx, int bank,
			    __u8 lane_mask, bool flip);

/* Disable/enable Rx output for lanes in lane_mask. */
int cmis_dp_rx_disable(struct cmd_context *ctx, int bank, __u8 lane_mask);
int cmis_dp_rx_enable(struct cmd_context *ctx, int bank, __u8 lane_mask);

/* Set/clear Tx auto-squelch disable for lanes in lane_mask. */
int cmis_dp_set_auto_squelch_tx(struct cmd_context *ctx, int bank,
				__u8 lane_mask, bool disable);

/* Set/clear Tx output squelch force for lanes in lane_mask. */
int cmis_dp_set_squelch_force_tx(struct cmd_context *ctx, int bank,
				 __u8 lane_mask, bool force);

/* Set/clear Rx auto-squelch disable for lanes in lane_mask. */
int cmis_dp_set_auto_squelch_rx(struct cmd_context *ctx, int bank,
				__u8 lane_mask, bool disable);

/* Set/clear adaptive Tx input EQ freeze for lanes in lane_mask. */
int cmis_dp_set_adapt_eq_freeze_tx(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool freeze);

/*-----------------------------------------------------------------------
 * Page 10h SCS0 Staged Controls (applied via ApplyImmediate)
 */

/* Set adaptive Tx input EQ enable for lanes in lane_mask. */
int cmis_dp_set_adapt_eq_enable_tx(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool enable);

/* Set host-controlled fixed Tx input EQ target for a lane (0-15). */
int cmis_dp_set_fixed_eq_tx(struct cmd_context *ctx, int bank,
			    int lane, __u8 target);

/* Set Rx output EQ pre-cursor target for a lane (0-15). */
int cmis_dp_set_rx_eq_pre(struct cmd_context *ctx, int bank,
			  int lane, __u8 target);

/* Set Rx output EQ post-cursor target for a lane (0-15). */
int cmis_dp_set_rx_eq_post(struct cmd_context *ctx, int bank,
			   int lane, __u8 target);

/* Set Rx output amplitude target for a lane (0-15). */
int cmis_dp_set_rx_amplitude(struct cmd_context *ctx, int bank,
			     int lane, __u8 target);

/*-----------------------------------------------------------------------
 * Page 10h Lane-Specific Masks
 */

/* Generic read-modify-write on a Page 10h lane mask byte.
 * offset: one of CMIS_MASK_* constants.
 * lane_mask: bitmask of affected lanes.
 * enable: true = set bits (mask the interrupt), false = clear bits (unmask).
 */
int cmis_dp_set_lane_mask(struct cmd_context *ctx, int bank,
			  __u8 offset, __u8 lane_mask, bool enable);

/*-----------------------------------------------------------------------
 * Module Global Controls (Lower Memory, Page 00h)
 */

/* Software reset (WO/SC, self-clearing). */
int cmis_module_sw_reset(struct cmd_context *ctx);

/* Set bank broadcast enable. */
int cmis_module_set_bank_broadcast(struct cmd_context *ctx, bool enable);

/* Set squelch method (false = reduce OMA, true = reduce Pav). */
int cmis_module_set_squelch_method(struct cmd_context *ctx, bool pav);

/* Set a module-level mask byte (00h:31-36).
 * offset: CMIS_MODULE_MASK_OFFSET, _VCC_TEMP, _AUX12, or _AUX3_CUSTOM.
 * mask/value: bits to set or clear.
 */
int cmis_module_set_mask(struct cmd_context *ctx, __u8 offset,
			 __u8 mask, bool enable);

/* Password entry and change. */
int cmis_module_password_entry(struct cmd_context *ctx, __u32 password);
int cmis_module_password_change(struct cmd_context *ctx,
				__u32 current_pw, __u32 new_pw);

/*-----------------------------------------------------------------------
 * User EEPROM (Page 03h, optional)
 */

/* Read user EEPROM. offset: 0-127, len: 1-128. buf must be >= len bytes. */
int cmis_user_eeprom_read(struct cmd_context *ctx, int offset,
			  __u8 *buf, int len);

/* Write user EEPROM. offset: 0-127, len: 1-8 per write. */
int cmis_user_eeprom_write(struct cmd_context *ctx, int offset,
			   const __u8 *data, int len);

/*-----------------------------------------------------------------------
 * Host Lane Switching (Page 1Dh, banked)
 */

/* Set redirection target for a lane. redir_lane: target lane number (0-7). */
int cmis_lane_switch_set_redir(struct cmd_context *ctx, int bank,
			       int lane, __u8 redir_lane);

/* Enable or disable lane switching. */
int cmis_lane_switch_enable(struct cmd_context *ctx, int bank, bool enable);

/* Commit lane switching configuration (WO/SC trigger). */
int cmis_lane_switch_commit(struct cmd_context *ctx, int bank);

/* Read lane switching result. Returns result byte, or -1 on error. */
int cmis_lane_switch_result(struct cmd_context *ctx, int bank);

#endif /* CMIS_DATAPATH_H__ */
