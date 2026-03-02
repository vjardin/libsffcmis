/*
 * cmis-tunable.h: CMIS Tunable Laser definitions
 *
 * Page 04h (Tunable Laser Capabilities) and Page 12h (Control/Status)
 * per OIF-CMIS-05.3, sections 7.5 and 8.4.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_TUNABLE_H__
#define CMIS_TUNABLE_H__

#include "cmis-internal.h"

/* Tunable Laser Capabilities Advertisement (Page 01h, byte 155) */
#define CMIS_TUNABLE_ADVER_OFFSET		0x9B
#define CMIS_TUNABLE_ADVER_MASK			0x40

/*-----------------------------------------------------------------------
 * Page 04h: Tunable Laser Capabilities (read-only, not banked)
 * Optional page advertised by 01h:155 bit 6.
 */

/* Grid Supported bitmask (bytes 128-129) */
#define CMIS_TUNABLE_GRID_SUPPORT_MSB		0x80
#define CMIS_TUNABLE_GRID_SUPPORT_LSB		0x81

/* Grid support bit definitions - MSB byte (128) */
#define CMIS_GRID_75GHZ_SUPPORTED		(1 << 7)
#define CMIS_GRID_150GHZ_SUPPORTED		(1 << 6)
#define CMIS_GRID_33GHZ_SUPPORTED		(1 << 5)
#define CMIS_GRID_100GHZ_SUPPORTED		(1 << 4)
#define CMIS_GRID_50GHZ_SUPPORTED		(1 << 3)
#define CMIS_GRID_25GHZ_SUPPORTED		(1 << 2)
#define CMIS_GRID_12P5GHZ_SUPPORTED		(1 << 1)
#define CMIS_GRID_6P25GHZ_SUPPORTED		(1 << 0)

/* Grid support bit definitions - LSB byte (129) */
#define CMIS_GRID_3P125GHZ_SUPPORTED		(1 << 7)
#define CMIS_FINE_TUNING_SUPPORTED		(1 << 0)

/* Grid channel ranges (bytes 130-165): S16 low/high per grid */
#define CMIS_GRID_75GHZ_LOW_CH			0x82
#define CMIS_GRID_75GHZ_HIGH_CH			0x84
#define CMIS_GRID_150GHZ_LOW_CH			0x86
#define CMIS_GRID_150GHZ_HIGH_CH		0x88
#define CMIS_GRID_33GHZ_LOW_CH			0x8A
#define CMIS_GRID_33GHZ_HIGH_CH			0x8C
#define CMIS_GRID_100GHZ_LOW_CH			0x8E
#define CMIS_GRID_100GHZ_HIGH_CH		0x90
#define CMIS_GRID_50GHZ_LOW_CH			0x92
#define CMIS_GRID_50GHZ_HIGH_CH			0x94
#define CMIS_GRID_25GHZ_LOW_CH			0x96
#define CMIS_GRID_25GHZ_HIGH_CH			0x98
#define CMIS_GRID_12P5GHZ_LOW_CH		0x9A
#define CMIS_GRID_12P5GHZ_HIGH_CH		0x9C
#define CMIS_GRID_6P25GHZ_LOW_CH		0x9E
#define CMIS_GRID_6P25GHZ_HIGH_CH		0xA0
#define CMIS_GRID_3P125GHZ_LOW_CH		0xA2
#define CMIS_GRID_3P125GHZ_HIGH_CH		0xA4

/* Fine tuning parameters (bytes 190-195) */
#define CMIS_FINE_TUNING_RESOLUTION		0xBE
#define CMIS_FINE_TUNING_LOW_OFFSET		0xC0
#define CMIS_FINE_TUNING_HIGH_OFFSET		0xC2

/* Programmable output power (bytes 196-201) */
#define CMIS_PROG_OUT_PWR_SUPPORT		0xC4
#define CMIS_PROG_OUT_PWR_SUPPORT_MASK		0x80
#define CMIS_PROG_OUT_PWR_MIN			0xC6
#define CMIS_PROG_OUT_PWR_MAX			0xC8

/*-----------------------------------------------------------------------
 * Page 12h: Tunable Laser Control/Status (banked, 8 lanes per bank)
 */

/* Control registers (RW) - per-lane, n=0..7 */
#define CMIS_TUNABLE_GRID_SPACING_TX(n)		(0x80 + (n))
#define CMIS_TUNABLE_GRID_SPACING_MASK		0xF0
#define CMIS_TUNABLE_GRID_SPACING_SHIFT		4
#define CMIS_TUNABLE_FINE_TUNING_EN_MASK	0x01

#define CMIS_TUNABLE_CHANNEL_NUM_TX(n)		(0x88 + (n) * 2)
#define CMIS_TUNABLE_FINE_OFFSET_TX(n)		(0x98 + (n) * 2)
#define CMIS_TUNABLE_TARGET_PWR_TX(n)		(0xC8 + (n) * 2)

/* Status registers (RO) - per-lane */
#define CMIS_TUNABLE_CURR_FREQ_TX(n)		(0xA8 + (n) * 4)

#define CMIS_TUNABLE_STATUS_TX(n)		(0xDE + (n))
#define CMIS_TUNABLE_STATUS_IN_PROGRESS		0x02
#define CMIS_TUNABLE_STATUS_UNLOCKED		0x01

/* Flags (per-lane, latched RO/COR) */
#define CMIS_TUNABLE_FLAG_SUMMARY		0xE6
#define CMIS_TUNABLE_FLAGS_TX(n)		(0xE7 + (n))
#define CMIS_TUNABLE_FLAG_COMPLETE		0x01
#define CMIS_TUNABLE_FLAG_INVALID_CHAN		0x04
#define CMIS_TUNABLE_FLAG_NOT_ACCEPTED		0x08

/* Mask registers */
#define CMIS_TUNABLE_MASK_TX(n)			(0xEF + (n))

/* Grid spacing encoding values (bits 7-4 of GridSpacingTx) */
#define CMIS_GRID_ENC_3P125GHZ			0x0
#define CMIS_GRID_ENC_6P25GHZ			0x1
#define CMIS_GRID_ENC_12P5GHZ			0x2
#define CMIS_GRID_ENC_25GHZ			0x3
#define CMIS_GRID_ENC_50GHZ			0x4
#define CMIS_GRID_ENC_100GHZ			0x5
#define CMIS_GRID_ENC_33GHZ			0x6
#define CMIS_GRID_ENC_75GHZ			0x7
#define CMIS_GRID_ENC_150GHZ			0x8

/* Configuration structure for tunable laser control */
struct cmis_tunable_config {
	__u8 grid_spacing;	/* Grid encoding value (CMIS_GRID_ENC_*) */
	__s16 channel_number;	/* Signed channel offset on selected grid */
	__u8 fine_tuning_en;	/* Enable fine-tuning (0 or 1) */
	__s16 fine_offset;	/* Fine-tuning offset in 0.001 GHz units */
	__s16 target_power;	/* Target output power in 0.01 dBm units */
};

/* Display tunable laser capabilities and status */
void cmis_show_tunable_laser(const struct cmis_memory_map *map);

/* Write tunable laser configuration for a single lane.
 * Handles DP state transitions and polls for tuning completion.
 * Returns 0 on success, negative on error.
 */
int cmis_tunable_set_lane(struct cmd_context *ctx, int bank, int lane,
			  const struct cmis_tunable_config *cfg);

/* Write tunable laser configuration for all 8 lanes in a bank */
int cmis_tunable_set_bank(struct cmd_context *ctx, int bank,
			  const struct cmis_tunable_config *cfg);

/* Poll for tuning completion on a single lane.
 * Returns 0 when tuning completes and wavelength locks,
 * -1 on timeout or error.
 */
int cmis_tunable_poll_complete(struct cmd_context *ctx, int bank, int lane,
			       int timeout_ms);

/* Set/clear a per-lane tuning flag mask (Page 12h:0xEF+lane).
 * mask_bits: bits to set or clear (bit 3=not accepted, 2=invalid channel,
 *            0=complete). enable: true=mask (suppress), false=unmask.
 */
int cmis_tunable_set_mask(struct cmd_context *ctx, int bank, int lane,
			  __u8 mask_bits, bool enable);

#endif /* CMIS_TUNABLE_H__ */
