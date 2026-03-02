/**
 * cmis-diag.h: CMIS Diagnostics (Pages 13h-14h)
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_DIAG_H__
#define CMIS_DIAG_H__

#include "cmis-internal.h"

void cmis_show_diag(const struct cmis_memory_map *map);

/* PRBS/BER test configuration */
struct cmis_bert_config {
	__u8  pattern;		/* PRBS pattern index (0-15) */
	__u8  lanes;		/* Bitmask of lanes to enable */
	bool  host_gen;		/* Enable host-side generator */
	bool  media_chk;	/* Enable media-side checker */
	bool  host_chk;		/* Enable host-side checker */
	bool  media_gen;	/* Enable media-side generator */
};

/* PRBS pattern name table (indexed 0-15, Table 8-105) */
const char *cmis_diag_pattern_name(int index);

/* Lookup pattern index by name (case-insensitive). Returns -1 if not found. */
int cmis_diag_pattern_lookup(const char *name);

/*
 * Start a PRBS/BER test: configure generators/checkers and start measurement.
 * Validates pattern against module capabilities before writing.
 */
int cmis_diag_bert_start(struct cmd_context *ctx, int bank,
			 const struct cmis_bert_config *cfg);

/* Stop an active PRBS/BER test: disable generators/checkers and stop. */
int cmis_diag_bert_stop(struct cmd_context *ctx, int bank);

/*
 * Read PRBS/BER test results: BER, error counts, and bit counts.
 * Prints results to stdout (plain text or JSON).
 */
int cmis_diag_bert_read(struct cmd_context *ctx, int bank);

/*
 * Start a loopback: validates capability, writes the control register.
 * mode: "host-input", "host-output", "media-input", or "media-output".
 * lanes: bitmask of lanes (0xFF = all).
 */
int cmis_diag_loopback_start(struct cmd_context *ctx, int bank,
			     const char *mode, __u8 lanes);

/* Stop all loopback modes: clear all four control registers. */
int cmis_diag_loopback_stop(struct cmd_context *ctx, int bank);

/*-----------------------------------------------------------------------
 * Diagnostics Masks, Scratchpad, and User Pattern (Page 13h)
 */

/* Set a diagnostics mask byte (Page 13h).
 * offset: one of CMIS_DIAG_MASK_* constants.
 * lane_mask: bitmask of lanes to mask/unmask.
 * enable: true = set bits (mask/suppress), false = clear bits (unmask).
 */
int cmis_diag_set_mask(struct cmd_context *ctx, int bank,
		       __u8 offset, __u8 lane_mask, bool enable);

/* Read the host scratchpad (Page 13h, 8 bytes).
 * buf must be at least CMIS_DIAG_SCRATCHPAD_SIZE bytes.
 */
int cmis_diag_scratchpad_read(struct cmd_context *ctx, int bank, __u8 *buf);

/* Write the host scratchpad (Page 13h, 8 bytes). */
int cmis_diag_scratchpad_write(struct cmd_context *ctx, int bank,
			       const __u8 *data, int len);

/* Write a PRBS user-defined pattern (Page 13h, up to 32 bytes). */
int cmis_diag_user_pattern_write(struct cmd_context *ctx, int bank,
				 const __u8 *data, int len);

#endif /* CMIS_DIAG_H__ */
