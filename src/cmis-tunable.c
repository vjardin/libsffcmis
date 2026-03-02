/**
 * cmis-tunable.c: CMIS Tunable Laser support
 *
 * Implements display of tunable laser capabilities (Page 04h)
 * and per-lane control/status (Page 12h) per OIF-CMIS-05.3.
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
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-tunable.h"
#include "cmis-datapath.h"

/* Page convenience macros for struct cmis_memory_map — local to CMIS files */
#define page_00h upper_memory[0x0][0x0]
#define page_01h upper_memory[0x0][0x1]
#define page_02h upper_memory[0x0][0x2]
#define page_04h upper_memory[0x0][0x4]

struct grid_info {
	const char *name;
	__u8 msb_mask;		/* Bit in MSB byte (128), 0 if in LSB */
	__u8 lsb_mask;		/* Bit in LSB byte (129), 0 if in MSB */
	__u8 low_ch_offset;
	__u8 high_ch_offset;
	__u32 spacing_mhz;	/* Grid spacing in MHz */
	__u8 encoding;		/* CMIS_GRID_ENC_* value */
};

static const struct grid_info grids[] = {
	{ "75 GHz",    CMIS_GRID_75GHZ_SUPPORTED,    0,
	  CMIS_GRID_75GHZ_LOW_CH,    CMIS_GRID_75GHZ_HIGH_CH,
	  75000,  CMIS_GRID_ENC_75GHZ },
	{ "150 GHz",   CMIS_GRID_150GHZ_SUPPORTED,   0,
	  CMIS_GRID_150GHZ_LOW_CH,   CMIS_GRID_150GHZ_HIGH_CH,
	  150000, CMIS_GRID_ENC_150GHZ },
	{ "33 GHz",    CMIS_GRID_33GHZ_SUPPORTED,    0,
	  CMIS_GRID_33GHZ_LOW_CH,    CMIS_GRID_33GHZ_HIGH_CH,
	  33000,  CMIS_GRID_ENC_33GHZ },
	{ "100 GHz",   CMIS_GRID_100GHZ_SUPPORTED,   0,
	  CMIS_GRID_100GHZ_LOW_CH,   CMIS_GRID_100GHZ_HIGH_CH,
	  100000, CMIS_GRID_ENC_100GHZ },
	{ "50 GHz",    CMIS_GRID_50GHZ_SUPPORTED,    0,
	  CMIS_GRID_50GHZ_LOW_CH,    CMIS_GRID_50GHZ_HIGH_CH,
	  50000,  CMIS_GRID_ENC_50GHZ },
	{ "25 GHz",    CMIS_GRID_25GHZ_SUPPORTED,    0,
	  CMIS_GRID_25GHZ_LOW_CH,    CMIS_GRID_25GHZ_HIGH_CH,
	  25000,  CMIS_GRID_ENC_25GHZ },
	{ "12.5 GHz",  CMIS_GRID_12P5GHZ_SUPPORTED,  0,
	  CMIS_GRID_12P5GHZ_LOW_CH,  CMIS_GRID_12P5GHZ_HIGH_CH,
	  12500,  CMIS_GRID_ENC_12P5GHZ },
	{ "6.25 GHz",  CMIS_GRID_6P25GHZ_SUPPORTED,  0,
	  CMIS_GRID_6P25GHZ_LOW_CH,  CMIS_GRID_6P25GHZ_HIGH_CH,
	  6250,   CMIS_GRID_ENC_6P25GHZ },
	{ "3.125 GHz", 0, CMIS_GRID_3P125GHZ_SUPPORTED,
	  CMIS_GRID_3P125GHZ_LOW_CH, CMIS_GRID_3P125GHZ_HIGH_CH,
	  3125,   CMIS_GRID_ENC_3P125GHZ },
};

#define NUM_GRIDS ARRAY_SIZE(grids)

static const char *grid_encoding_to_string(__u8 enc)
{
	switch (enc) {
	case CMIS_GRID_ENC_3P125GHZ:	return "3.125 GHz";
	case CMIS_GRID_ENC_6P25GHZ:	return "6.25 GHz";
	case CMIS_GRID_ENC_12P5GHZ:	return "12.5 GHz";
	case CMIS_GRID_ENC_25GHZ:	return "25 GHz";
	case CMIS_GRID_ENC_50GHZ:	return "50 GHz";
	case CMIS_GRID_ENC_100GHZ:	return "100 GHz";
	case CMIS_GRID_ENC_33GHZ:	return "33 GHz";
	case CMIS_GRID_ENC_75GHZ:	return "75 GHz";
	case CMIS_GRID_ENC_150GHZ:	return "150 GHz";
	default:			return "Unknown";
	}
}

/* Convert frequency in THz to wavelength in nm: λ = c / f */
static double freq_thz_to_nm(double freq_thz)
{
	if (freq_thz <= 0.0)
		return 0.0;
	return 299792.458 / freq_thz;
}

/*
 * Convert a frequency offset in GHz to a wavelength offset in nm
 * at a given center wavelength.  |Δλ| = λ² x |Δf| / c
 * where c = 299792458 when λ is in nm and Δf is in GHz.
 */
static double ghz_offset_to_nm(double ghz, double center_nm)
{
	return center_nm * center_nm * ghz / 299792458.0;
}

/* Print tunable laser capabilities. Relevant documents:
 * [1] OIF-CMIS-05.3, pag. 175, section 8.4.11, Table 8-77 (Page 04h)
 */
static void cmis_show_tunable_caps(const struct cmis_memory_map *map)
{
	const __u8 *page_04 = map->page_04h;
	__u8 grid_msb, grid_lsb;
	__u16 u16val;
	__s16 s16val;
	bool fine_tuning, prog_pwr;
	unsigned int i;
	double nom_wl_nm;

	if (!page_04)
		return;

	/* Nominal wavelength from Page 01h for GHz-to-nm conversions */
	nom_wl_nm = OFFSET_TO_U16_PTR(map->page_01h,
				       CMIS_NOM_WAVELENGTH_MSB) * 0.05;
	if (nom_wl_nm <= 0.0)
		nom_wl_nm = 1550.0;	/* sensible fallback */

	grid_msb = page_04[CMIS_TUNABLE_GRID_SUPPORT_MSB];
	grid_lsb = page_04[CMIS_TUNABLE_GRID_SUPPORT_LSB];

	if (is_json_context())
		open_json_object("tunable_laser_capabilities");
	else
		printf("\t%-41s :\n", "Tunable laser capabilities");

	/* Show supported grids and their channel ranges */
	for (i = 0; i < NUM_GRIDS; i++) {
		bool supported;
		__s16 low_ch, high_ch;
		double low_freq_thz, high_freq_thz;
		double low_wl_nm, high_wl_nm;

		if (grids[i].msb_mask)
			supported = grid_msb & grids[i].msb_mask;
		else
			supported = grid_lsb & grids[i].lsb_mask;

		if (!supported)
			continue;

		low_ch = (__s16)OFFSET_TO_U16_PTR(page_04,
						  grids[i].low_ch_offset);
		high_ch = (__s16)OFFSET_TO_U16_PTR(page_04,
						   grids[i].high_ch_offset);

		if (low_ch == 0 && high_ch == 0)
			continue;

		/* freq = 193.1 THz + n * grid_spacing_mhz / 1e6 THz */
		low_freq_thz = 193.1 +
			(double)low_ch * grids[i].spacing_mhz / 1000000.0;
		high_freq_thz = 193.1 +
			(double)high_ch * grids[i].spacing_mhz / 1000000.0;

		/* Lower freq → longer wavelength, higher freq → shorter */
		low_wl_nm = freq_thz_to_nm(high_freq_thz);
		high_wl_nm = freq_thz_to_nm(low_freq_thz);

		if (is_json_context()) {
			char json_fn[64];
			char *p;

			snprintf(json_fn, sizeof(json_fn), "grid_%s",
				 grids[i].name);
			/* Sanitize for JSON key: replace ' ' and '.' */
			for (p = json_fn; *p; p++) {
				if (*p == ' ' || *p == '.')
					*p = '_';
			}
			open_json_object(json_fn);
			print_int(PRINT_JSON, "low_channel", "%d", low_ch);
			print_int(PRINT_JSON, "high_channel", "%d", high_ch);
			print_float(PRINT_JSON, "low_wavelength_nm", "%.2f",
				    low_wl_nm);
			print_float(PRINT_JSON, "high_wavelength_nm", "%.2f",
				    high_wl_nm);
			close_json_object();
		} else {
			printf("\t  %-39s : ch %d..%d (%.2f..%.2f nm)\n",
			       grids[i].name, low_ch, high_ch,
			       low_wl_nm, high_wl_nm);
		}
	}

	/* Fine tuning support */
	fine_tuning = grid_lsb & CMIS_FINE_TUNING_SUPPORTED;
	if (is_json_context())
		print_bool(PRINT_JSON, "fine_tuning_supported", NULL,
			   fine_tuning);
	else
		printf("\t%-41s : %s\n", "Fine tuning supported",
		       YESNO(fine_tuning));

	if (fine_tuning) {
		double res_ghz, lo_ghz, hi_ghz;

		u16val = OFFSET_TO_U16_PTR(page_04,
					   CMIS_FINE_TUNING_RESOLUTION);
		res_ghz = (double)u16val * 0.001;
		if (is_json_context())
			print_float(PRINT_JSON,
				    "fine_tuning_resolution_nm", "%.4f",
				    ghz_offset_to_nm(res_ghz, nom_wl_nm));
		else
			printf("\t  %-39s : %.4f nm\n",
			       "Fine tuning resolution",
			       ghz_offset_to_nm(res_ghz, nom_wl_nm));

		s16val = (__s16)OFFSET_TO_U16_PTR(page_04,
						  CMIS_FINE_TUNING_LOW_OFFSET);
		lo_ghz = (double)s16val * 0.001;
		if (is_json_context())
			print_float(PRINT_JSON,
				    "fine_tuning_low_offset_nm", "%.3f",
				    -ghz_offset_to_nm(-lo_ghz, nom_wl_nm));
		else
			printf("\t  %-39s : %.3f nm\n",
			       "Fine tuning low offset",
			       -ghz_offset_to_nm(-lo_ghz, nom_wl_nm));

		s16val = (__s16)OFFSET_TO_U16_PTR(page_04,
						  CMIS_FINE_TUNING_HIGH_OFFSET);
		hi_ghz = (double)s16val * 0.001;
		if (is_json_context())
			print_float(PRINT_JSON,
				    "fine_tuning_high_offset_nm", "%.3f",
				    ghz_offset_to_nm(hi_ghz, nom_wl_nm));
		else
			printf("\t  %-39s : %.3f nm\n",
			       "Fine tuning high offset",
			       ghz_offset_to_nm(hi_ghz, nom_wl_nm));
	}

	/* Programmable output power */
	prog_pwr = page_04[CMIS_PROG_OUT_PWR_SUPPORT] &
		   CMIS_PROG_OUT_PWR_SUPPORT_MASK;
	if (is_json_context())
		print_bool(PRINT_JSON, "programmable_output_power", NULL,
			   prog_pwr);
	else
		printf("\t%-41s : %s\n", "Programmable output power",
		       YESNO(prog_pwr));

	if (prog_pwr) {
		s16val = (__s16)OFFSET_TO_U16_PTR(page_04,
						  CMIS_PROG_OUT_PWR_MIN);
		if (is_json_context())
			print_float(PRINT_JSON, "output_power_min_dbm", "%.2f",
				    (double)s16val * 0.01);
		else
			printf("\t  %-39s : %.2f dBm\n",
			       "Output power min", (double)s16val * 0.01);

		s16val = (__s16)OFFSET_TO_U16_PTR(page_04,
						  CMIS_PROG_OUT_PWR_MAX);
		if (is_json_context())
			print_float(PRINT_JSON, "output_power_max_dbm", "%.2f",
				    (double)s16val * 0.01);
		else
			printf("\t  %-39s : %.2f dBm\n",
			       "Output power max", (double)s16val * 0.01);
	}

	if (is_json_context())
		close_json_object();
}

/* Print per-lane tuning status for one bank. Relevant documents:
 * [1] OIF-CMIS-05.3, pag. 183, section 8.4.14, Table 8-83 (Page 12h)
 */
static void cmis_show_tunable_status_bank(const struct cmis_memory_map *map,
					  int bank)
{
	const __u8 *page_12 = map->upper_memory[bank][0x12];
	int i;

	if (!page_12)
		return;

	for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
		int chan = bank * CMIS_CHANNELS_PER_BANK + i;
		__u8 grid_byte, grid_enc, status, flags;
		bool fine_en, tuning_in_progress, wavelength_unlocked;
		__s16 channel, fine_offset, target_pwr;
		__u32 curr_freq;

		grid_byte = page_12[CMIS_TUNABLE_GRID_SPACING_TX(i)];
		grid_enc = (grid_byte & CMIS_TUNABLE_GRID_SPACING_MASK) >>
			   CMIS_TUNABLE_GRID_SPACING_SHIFT;
		fine_en = grid_byte & CMIS_TUNABLE_FINE_TUNING_EN_MASK;

		channel = (__s16)OFFSET_TO_U16_PTR(page_12,
			  CMIS_TUNABLE_CHANNEL_NUM_TX(i));
		fine_offset = (__s16)OFFSET_TO_U16_PTR(page_12,
			      CMIS_TUNABLE_FINE_OFFSET_TX(i));

		/* U32 big-endian: current laser frequency in 0.001 GHz */
		curr_freq =
			((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i)] << 24) |
			((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 1] << 16) |
			((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 2] << 8) |
			page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 3];

		if (grid_enc == 0 && channel == 0 && curr_freq == 0)
			continue;

		target_pwr = (__s16)OFFSET_TO_U16_PTR(page_12,
			     CMIS_TUNABLE_TARGET_PWR_TX(i));

		status = page_12[CMIS_TUNABLE_STATUS_TX(i)];
		flags = page_12[CMIS_TUNABLE_FLAGS_TX(i)];
		tuning_in_progress = status & CMIS_TUNABLE_STATUS_IN_PROGRESS;
		wavelength_unlocked = status & CMIS_TUNABLE_STATUS_UNLOCKED;

		{
			double freq_thz = (double)curr_freq / 1000000.0;
			double wl_nm = freq_thz_to_nm(freq_thz);
			double nom_wl = freq_thz_to_nm(193.1);
			double fo_nm = ghz_offset_to_nm(
					(double)fine_offset * 0.001,
					nom_wl > 0.0 ? nom_wl : 1550.0);

			if (is_json_context()) {
				char json_fn[32];

				snprintf(json_fn, sizeof(json_fn),
					 "lane_%d", chan + 1);
				open_json_object(json_fn);
				print_string(PRINT_JSON, "grid_spacing",
					     "%s",
					     grid_encoding_to_string(
						     grid_enc));
				print_int(PRINT_JSON, "channel_number",
					  "%d", channel);
				print_bool(PRINT_JSON,
					   "fine_tuning_enabled", NULL,
					   fine_en);
				print_float(PRINT_JSON,
					    "fine_tuning_offset_nm",
					    "%.4f", fo_nm);
				print_float(PRINT_JSON,
					    "current_wavelength_nm",
					    "%.2f", wl_nm);
				print_float(PRINT_JSON,
					    "target_power_dbm", "%.2f",
					    (double)target_pwr * 0.01);
				print_bool(PRINT_JSON,
					   "tuning_in_progress", NULL,
					   tuning_in_progress);
				print_bool(PRINT_JSON,
					   "wavelength_unlocked", NULL,
					   wavelength_unlocked);
				print_bool(PRINT_JSON,
					   "tuning_complete", NULL,
					   !!(flags &
					      CMIS_TUNABLE_FLAG_COMPLETE));
				print_bool(PRINT_JSON,
					   "tuning_not_accepted", NULL,
					   !!(flags &
					      CMIS_TUNABLE_FLAG_NOT_ACCEPTED));
				print_bool(PRINT_JSON,
					   "invalid_channel", NULL,
					   !!(flags &
					      CMIS_TUNABLE_FLAG_INVALID_CHAN));
				close_json_object();
			} else {
				printf("\t  Lane %d:\n", chan + 1);
				printf("\t    %-37s : %s\n",
				       "Grid spacing",
				       grid_encoding_to_string(grid_enc));
				printf("\t    %-37s : %d\n",
				       "Channel number", channel);
				printf("\t    %-37s : %s\n",
				       "Fine tuning",
				       fine_en ? "Enabled" : "Disabled");
				printf("\t    %-37s : %.4f nm\n",
				       "Fine tuning offset", fo_nm);
				printf("\t    %-37s : %.2f nm\n",
				       "Current wavelength", wl_nm);
				printf("\t    %-37s : %.2f dBm\n",
				       "Target output power",
				       (double)target_pwr * 0.01);
				printf("\t    %-37s : %s\n",
				       "Tuning in progress",
				       YESNO(tuning_in_progress));
				printf("\t    %-37s : %s\n",
				       "Wavelength unlocked",
				       YESNO(wavelength_unlocked));
				if (flags & CMIS_TUNABLE_FLAG_COMPLETE)
					printf("\t    %-37s : Yes\n",
					       "Tuning complete");
				if (flags &
				    CMIS_TUNABLE_FLAG_NOT_ACCEPTED)
					printf("\t    %-37s : Yes\n",
					       "Tuning not accepted");
				if (flags &
				    CMIS_TUNABLE_FLAG_INVALID_CHAN)
					printf("\t    %-37s : Yes\n",
					       "Invalid channel number");
			}
		}
	}
}

/* Print tunable laser info: check advertisement bit, then show Page 04h
 * capabilities and Page 12h per-bank status.
 */
void cmis_show_tunable_laser(const struct cmis_memory_map *map)
{
	int i;

	if (!map->page_01h)
		return;

	/* Check if tunable laser is advertised (01h:155 bit 6) */
	if (!(map->page_01h[CMIS_TUNABLE_ADVER_OFFSET] &
	      CMIS_TUNABLE_ADVER_MASK))
		return;

	cmis_show_tunable_caps(map);

	/* Show per-bank tuning status */
	if (is_json_context())
		open_json_object("tunable_laser_status");
	else
		printf("\t%-41s :\n", "Tunable laser status");

	for (i = 0; i < CMIS_MAX_BANKS; i++)
		cmis_show_tunable_status_bank(map, i);

	if (is_json_context())
		close_json_object();
}

/*----------------------------------------------------------------------
 * Data Path state management for tunable laser configuration.
 * OIF-CMIS-05.3, Section 7.5: Module must be in DPDeactivated state
 * before changing grid/channel settings.
 *
 * DP state read/write functions are in cmis-datapath.c (shared).
 *----------------------------------------------------------------------*/

/*
 * Poll for tuning completion on a single lane.
 * Reads Page 12h TuningStatus and flags every 100ms.
 * Returns 0 when tuning completes and wavelength locks, -1 on timeout/error.
 */
int cmis_tunable_poll_complete(struct cmd_context *ctx, int bank, int lane,
			       int timeout_ms)
{
	struct module_eeprom request;
	int elapsed, ret;
	__u8 status, flags;

	for (elapsed = 0; elapsed < timeout_ms; elapsed += 100) {
		usleep(100000);

		/* Read tuning status byte */
		cmis_request_init(&request, bank, 0x12,
				  CMIS_TUNABLE_STATUS_TX(lane));
		request.length = 1;
		ret = get_eeprom_page(ctx, &request);
		if (ret < 0)
			continue;

		status = request.data[0];

		/* Check if tuning is no longer in progress and locked */
		if (!(status & CMIS_TUNABLE_STATUS_IN_PROGRESS) &&
		    !(status & CMIS_TUNABLE_STATUS_UNLOCKED)) {
			/* Read flags for completion confirmation */
			cmis_request_init(&request, bank, 0x12,
					  CMIS_TUNABLE_FLAGS_TX(lane));
			request.length = 1;
			ret = get_eeprom_page(ctx, &request);
			if (ret < 0)
				return 0;  /* status says locked, good enough */

			flags = request.data[0];
			if (flags & CMIS_TUNABLE_FLAG_NOT_ACCEPTED) {
				fprintf(stderr,
					"Tuning not accepted on lane %d\n",
					lane);
				return -1;
			}
			if (flags & CMIS_TUNABLE_FLAG_INVALID_CHAN) {
				fprintf(stderr,
					"Invalid channel number on lane %d\n",
					lane);
				return -1;
			}
			return 0;
		}
	}

	fprintf(stderr, "Timeout polling tuning completion on bank %d lane %d "
		"(status=0x%02x)\n", bank, lane, status);
	return -1;
}

/*
 * Write tunable laser configuration for one lane on Page 12h.
 * Writes the 4 control registers (grid/channel/fine/power).
 * Returns 0 on success, negative on error.
 */
static int cmis_tunable_write_lane(struct cmd_context *ctx, int bank, int lane,
				   const struct cmis_tunable_config *cfg)
{
	struct module_eeprom request;
	__u8 buf[2];
	int ret;

	/* Write GridSpacingTx + FineTuningEnableTx */
	cmis_request_init(&request, bank, 0x12,
			  CMIS_TUNABLE_GRID_SPACING_TX(lane));
	request.length = 1;
	buf[0] = ((cfg->grid_spacing << CMIS_TUNABLE_GRID_SPACING_SHIFT) &
		  CMIS_TUNABLE_GRID_SPACING_MASK) |
		 (cfg->fine_tuning_en & CMIS_TUNABLE_FINE_TUNING_EN_MASK);
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Write ChannelNumberTx (S16 big-endian) */
	cmis_request_init(&request, bank, 0x12,
			  CMIS_TUNABLE_CHANNEL_NUM_TX(lane));
	request.length = 2;
	buf[0] = (cfg->channel_number >> 8) & 0xFF;
	buf[1] = cfg->channel_number & 0xFF;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Write FineTuningOffsetTx (S16 big-endian) */
	cmis_request_init(&request, bank, 0x12,
			  CMIS_TUNABLE_FINE_OFFSET_TX(lane));
	request.length = 2;
	buf[0] = (cfg->fine_offset >> 8) & 0xFF;
	buf[1] = cfg->fine_offset & 0xFF;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Write TargetOutputPowerTx (S16 big-endian) */
	cmis_request_init(&request, bank, 0x12,
			  CMIS_TUNABLE_TARGET_PWR_TX(lane));
	request.length = 2;
	buf[0] = (cfg->target_power >> 8) & 0xFF;
	buf[1] = cfg->target_power & 0xFF;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	return 0;
}

/* Write tunable laser configuration for a single lane on Page 12h.
 * Handles DP state transitions and tuning completion polling.
 * [1] OIF-CMIS-05.3, pag. 105, section 7.5
 */
int cmis_tunable_set_lane(struct cmd_context *ctx, int bank, int lane,
			  const struct cmis_tunable_config *cfg)
{
	int state, ret;

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

	/* Step 2: If not DPDeactivated, deinit first */
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

	/* Step 3: Write tuning configuration registers */
	ret = cmis_tunable_write_lane(ctx, bank, lane, cfg);
	if (ret < 0)
		return ret;

	if (!is_json_context())
		printf("  Lane %d: tuning registers written\n", lane);

	/* Step 4: Re-activate the data path */
	if (!is_json_context())
		printf("  Lane %d: re-activating data path...\n", lane);
	ret = cmis_dp_init_lane(ctx, bank, lane);
	if (ret < 0)
		return ret;

	/* Step 5: Poll for tuning completion (10s timeout for tunable) */
	if (!is_json_context())
		printf("  Lane %d: polling for tuning completion...\n", lane);
	ret = cmis_tunable_poll_complete(ctx, bank, lane, 10000);
	if (ret < 0)
		return ret;

	if (!is_json_context())
		printf("  Lane %d: tuning complete, wavelength locked\n",
		       lane);
	return 0;
}

/* Write tunable laser configuration for all 8 lanes in a bank. */
int cmis_tunable_set_bank(struct cmd_context *ctx, int bank,
			  const struct cmis_tunable_config *cfg)
{
	int i, ret;

	for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
		ret = cmis_tunable_set_lane(ctx, bank, i, cfg);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int cmis_tunable_set_mask(struct cmd_context *ctx, int bank, int lane,
			  __u8 mask_bits, bool enable)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	if (lane < 0 || lane >= CMIS_CHANNELS_PER_BANK)
		return -1;

	cmis_request_init(&request, bank, 0x12, CMIS_TUNABLE_MASK_TX(lane));
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = enable ? (old | mask_bits) : (old & ~mask_bits);

	cmis_request_init(&request, bank, 0x12, CMIS_TUNABLE_MASK_TX(lane));
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s",
			     "TuningFlagMask");
		print_uint(PRINT_JSON, "lane", "%u", lane);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  TuningFlagMask lane %d: 0x%02x -> 0x%02x\n",
		       lane, old, buf);
	}
	return 0;
}
