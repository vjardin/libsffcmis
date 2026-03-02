/*
 * cmis-vdm.h: VDM (Versatile Diagnostics Monitoring) Pages 20h-2Fh
 *
 * Real-time coherent link parameters (CD, OSNR, eSNR, CFO, etc.)
 * per OIF-CMIS-05.3 Section 8.22 and OIF-C-CMIS-01.3 Table 8.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_VDM_H__
#define CMIS_VDM_H__

#include "cmis-internal.h"

/* Maximum lanes supported (4 banks x 8 lanes) */
#define CMIS_VDM_MAX_LANES	32

/* PAM4 eye diagram data extracted from VDM instances */
struct vdm_pam4_lane {
	int    lane;		/* Lane/DP number (from descriptor bits 0-3) */
	bool   valid;		/* true if all 4 levels were found */
	double level[4];	/* PAM4 Level 0-3 in mV */
	double snr_db;		/* Best SNR in dB (min > I > Q) */
	bool   has_snr;
	double ber;		/* Pre-FEC BER */
	bool   has_ber;
	double mpi_db;		/* PAM4 MPI in dB */
	bool   has_mpi;
};

/**
 * Extract PAM4 level/SNR/BER data from VDM instances.
 * Returns the number of valid lanes (all 4 levels found), or 0 if none.
 * Valid lanes are compacted to the front of the array.
 */
int cmis_vdm_get_pam4_data(const struct cmis_memory_map *map,
			    struct vdm_pam4_lane *lanes, int max_lanes);

/* Coherent link quality data extracted from VDM instances (C-CMIS Type IDs) */
struct vdm_coherent_lane {
	int    lane;		/* Lane/DP number (from descriptor bits 0-3) */
	bool   valid;		/* true if EVM or eSNR found */
	double evm_pct;		/* EVM in % */
	bool   has_evm;
	double esnr_db;		/* Effective SNR in dB */
	bool   has_esnr;
	double osnr_db;		/* OSNR in dB */
	bool   has_osnr;
	double snr_margin_db;	/* SNR margin in dB */
	bool   has_snr_margin;
	double q_factor_db;	/* Q-factor in dB */
	bool   has_q_factor;
};

/**
 * Extract coherent EVM/eSNR/OSNR data from VDM instances.
 * Returns the number of valid lanes (EVM or eSNR found), or 0 if none.
 * Valid lanes are compacted to the front of the array.
 */
int cmis_vdm_get_coherent_data(const struct cmis_memory_map *map,
			       struct vdm_coherent_lane *lanes,
			       int max_lanes);

/* Display VDM real-time monitors (Pages 20h-2Fh) */
void cmis_show_vdm(const struct cmis_memory_map *map);

/* Freeze/unfreeze VDM sample values for consistent reads.
 * Returns 0 on success, -ETIMEDOUT if the module doesn't respond in time.
 */
int cmis_vdm_freeze(struct cmd_context *ctx);
int cmis_vdm_unfreeze(struct cmd_context *ctx);

/* Set VDM power saving mode (true = monitoring OFF to save power). */
int cmis_vdm_set_power_saving(struct cmd_context *ctx, bool save);

/* Set a VDM alarm/warning mask nibble for a given VDM instance.
 * instance: 0-255 (across all 4 groups).
 * mask_nibble: 4-bit mask (bit 3=hi alarm, 2=lo alarm, 1=hi warn, 0=lo warn).
 *              1 = masked (suppressed), 0 = unmasked.
 */
int cmis_vdm_set_mask(struct cmd_context *ctx, int instance,
		       __u8 mask_nibble);

#endif /* CMIS_VDM_H__ */
