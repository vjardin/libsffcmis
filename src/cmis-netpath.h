/**
 * cmis-netpath.h: CMIS Network Path Control and Status (Pages 16h-17h)
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_NETPATH_H__
#define CMIS_NETPATH_H__

#include "cmis-internal.h"

void cmis_show_netpath(const struct cmis_memory_map *map);

/*-----------------------------------------------------------------------
 * NP Write Operations (Page 16h)
 */

/* Deactivate a network path lane (set NPDeinit, apply SCS0, poll). */
int cmis_np_deinit_lane(struct cmd_context *ctx, int bank, int lane);

/* Re-activate a network path lane (clear NPDeinit, apply SCS0, poll). */
int cmis_np_init_lane(struct cmd_context *ctx, int bank, int lane);

/* NP lane configuration. */
struct cmis_np_config {
	__u8 npid;      /* 0-15 */
	bool in_use;    /* NP in-use bit */
};

/* Configure a lane's NPID/InUse in SCS0, then apply. */
int cmis_np_configure_lane(struct cmd_context *ctx, int bank, int lane,
			   const struct cmis_np_config *cfg);

/* Set signal source selection for Rx HP (Page 16h:0xA2).
 * If replace=true, internal replacement signal is used instead of NP signal.
 */
int cmis_np_set_hp_source_rx(struct cmd_context *ctx, int bank,
			     __u8 lane_mask, bool replace);

/* Set signal source selection for Tx NP (Page 16h:0xA3).
 * If replace=true, internal replacement signal is used instead of HP signal.
 */
int cmis_np_set_np_source_tx(struct cmd_context *ctx, int bank,
			     __u8 lane_mask, bool replace);

/* Set NP state-changed mask (Page 17h:0xC0). */
int cmis_np_set_state_mask(struct cmd_context *ctx, int bank,
			   __u8 lane_mask, bool mask);

#endif /* CMIS_NETPATH_H__ */
