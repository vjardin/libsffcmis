/*
 * cmis-coherent.h: C-CMIS Coherent Link Performance Monitoring
 *
 * Page 35h (PM metrics), Page 34h (FEC counters), Page 33h/3Bh (flags),
 * Page 40h (revision), Page 42h (PM adver), Page 44h (alarm adver)
 * per OIF-C-CMIS-01.3.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_COHERENT_H__
#define CMIS_COHERENT_H__

#include "cmis-internal.h"

/* Display C-CMIS coherent PM (Page 35h metrics + Page 34h FEC counters) */
void cmis_show_coherent_pm(const struct cmis_memory_map *map);

/* Display C-CMIS coherent flags (Page 33h media + Page 3Bh host) */
void cmis_show_coherent_flags(const struct cmis_memory_map *map);

/*-----------------------------------------------------------------------
 * Coherent Threshold Write Operations (Page 30h)
 */

/* Set a coherent threshold value on Page 30h.
 * type: "total-pwr-hi-alarm", "total-pwr-lo-alarm", etc., or
 *       "fdd-raise", "fdd-clear", "fed-raise", "fed-clear".
 * value: dBm for power thresholds, BER for FDD/FED.
 */
int cmis_coherent_set_threshold(struct cmd_context *ctx, int bank,
				const char *type, double value);

/* Enable or disable use of configured thresholds.
 * type: "total-pwr", "sig-pwr", "fdd", or "fed".
 */
int cmis_coherent_threshold_enable(struct cmd_context *ctx, int bank,
				   const char *type, bool enable);

/*-----------------------------------------------------------------------
 * Media Lane Provisioning (Page 31h)
 */

/* Set Tx filter enable for lanes in lane_mask. */
int cmis_coherent_set_tx_filter_enable(struct cmd_context *ctx, int bank,
				       __u8 lane_mask, bool enable);

/* Set Tx filter type for a lane (0=None, 1=RRC, 2=RC, 3=Gaussian). */
int cmis_coherent_set_tx_filter_type(struct cmd_context *ctx, int bank,
				     int lane, __u8 type);

/* Set LF insertion on LD enable for lanes in lane_mask. */
int cmis_coherent_set_lf_insertion(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool enable);

/*-----------------------------------------------------------------------
 * Media Lane Flag Masks (Page 32h)
 */

/* Set a media flag mask byte (Page 32h).
 * offset: CCMIS_MEDIA_MASK_TX_OFFSET, _RX, _RX_FEC, or _DEGRADE.
 * mask/lane_mask: bits to set or clear in the register.
 * enable: true = set (mask/suppress), false = clear (unmask).
 */
int cmis_coherent_set_media_mask(struct cmd_context *ctx, int bank,
				 __u8 offset, __u8 mask, bool enable);

/*-----------------------------------------------------------------------
 * Host Interface Thresholds (Page 38h)
 */

/* Set a host BER threshold on Page 38h.
 * type: "host-fdd-raise", "host-fdd-clear", "host-fed-raise", "host-fed-clear".
 * value: BER (F16).
 */
int cmis_coherent_set_host_threshold(struct cmd_context *ctx, int bank,
				     const char *type, double value);

/* Enable/disable host BER threshold monitoring.
 * type: "host-fdd" or "host-fed".
 */
int cmis_coherent_host_threshold_enable(struct cmd_context *ctx, int bank,
					const char *type, bool enable);

/*-----------------------------------------------------------------------
 * Host Data Path Flag Masks (Page 3Bh)
 */

/* Set a host flag mask byte (Page 3Bh, bytes 128-132).
 * offset: CCMIS_HOST_MASKS_FEC_OFFSET, _DEGRADE, _FLEXE, _TX, or _RX.
 * mask/enable: bits to set or clear.
 */
int cmis_coherent_set_host_mask(struct cmd_context *ctx, int bank,
				__u8 offset, __u8 mask, bool enable);

#endif /* CMIS_COHERENT_H__ */
