/**
 * cmis-eye.c: Synthetic PAM4 eye diagram / IQ constellation from VDM data
 *
 * PAM4 eye: Uses amplitude levels (VDM Type IDs 16-19, in mV) and SNR
 * measurements (Type IDs 29-34, in dB) to render a synthetic ASCII eye
 * diagram.  Vertical structure is faithful to VDM data; horizontal
 * transitions are modeled with a raised cosine (no jitter data available).
 *
 * IQ constellation: Uses coherent EVM/eSNR (C-CMIS Type IDs 140-142) to
 * render a synthetic constellation diagram.  Modulation format is auto-
 * detected from the active application descriptor (QPSK/8QAM/16QAM/64QAM).
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/types.h>

#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-vdm.h"
#include "cmis-eye.h"

/* --- PAM4 Eye Diagram --- */

#define EYE_ROWS	40
#define EYE_COLS	70
#define EYE_MARGIN	0.15	/* Extra Y margin above L3 / below L0 */
#define SNR_DEFAULT_DB	20.0	/* Assumed SNR when VDM doesn't report it */
#define SIGMA_MIN_FRAC	0.005	/* Minimum sigma as fraction of span */
#define NUM_LEVELS	4
#define NUM_EYES	3	/* Three PAM4 eyes */

/* Density character palette (6 levels, UTF-8) */
static const char *density_chars[] = {
	" ", ".", "\xe2\x96\x91", "\xe2\x96\x92",
	"\xe2\x96\x93", "\xe2\x96\x88"
};
#define DENSITY_LEVELS	((int)(sizeof(density_chars) / sizeof(density_chars[0])))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double gaussian(double x, double mu, double sigma)
{
	double d = (x - mu) / sigma;
	return exp(-0.5 * d * d);
}

static double lerp(double a, double b, double t)
{
	return a + t * (b - a);
}

/*
 * Render eye diagram for one lane into grid[EYE_ROWS][EYE_COLS].
 * Returns eye heights in eye_heights[3] (mV).
 */
static void render_eye(const struct vdm_pam4_lane *lane,
		       double grid[EYE_ROWS][EYE_COLS],
		       double *eye_heights)
{
	double span = lane->level[3] - lane->level[0];
	double y_min = lane->level[0] - EYE_MARGIN * span;
	double y_max = lane->level[3] + EYE_MARGIN * span;
	double snr_db = lane->has_snr ? lane->snr_db : SNR_DEFAULT_DB;
	double sigma, sigma_min;
	int r, c, i, j;
	double log_min, log_max;
	bool first;

	/* Sigma from SNR: span / (6 * 10^(snr/20)) */
	sigma = span / (6.0 * pow(10.0, snr_db / 20.0));
	sigma_min = span * SIGMA_MIN_FRAC;
	if (sigma < sigma_min)
		sigma = sigma_min;

	/* Compute density for each cell */
	for (r = 0; r < EYE_ROWS; r++) {
		double y = y_max - r * (y_max - y_min) / (EYE_ROWS - 1);

		for (c = 0; c < EYE_COLS; c++) {
			double x = (double)c / (EYE_COLS - 1);
			/* Raised cosine: 0 at edges, 1 at center */
			double w = 0.5 * (1.0 - cos(2.0 * M_PI * x));
			double density = 0.0;

			for (i = 0; i < NUM_LEVELS; i++) {
				/* Settled contribution */
				density += w * gaussian(y, lane->level[i],
							sigma);

				/* Transition contributions from all other levels */
				for (j = 0; j < NUM_LEVELS; j++) {
					if (j == i)
						continue;
					double mid = lerp(lane->level[j],
							  lane->level[i], w);
					density += (1.0 - w) *
						   gaussian(y, mid,
							    1.5 * sigma) /
						   (double)(NUM_LEVELS *
							    (NUM_LEVELS - 1));
				}
			}

			grid[r][c] = density / NUM_LEVELS;
		}
	}

	/* Log-scale normalization */
	log_min = 1e30;
	log_max = -1e30;
	first = true;
	for (r = 0; r < EYE_ROWS; r++) {
		for (c = 0; c < EYE_COLS; c++) {
			if (grid[r][c] > 1e-12) {
				double lv = log10(grid[r][c]);
				if (first || lv < log_min)
					log_min = lv;
				if (first || lv > log_max)
					log_max = lv;
				first = false;
				grid[r][c] = lv;
			} else {
				grid[r][c] = -999.0;
			}
		}
	}

	/* Normalize to 0..1 */
	if (log_max > log_min) {
		double range = log_max - log_min;
		for (r = 0; r < EYE_ROWS; r++)
			for (c = 0; c < EYE_COLS; c++) {
				if (grid[r][c] > -900.0)
					grid[r][c] = (grid[r][c] - log_min) /
						     range;
				else
					grid[r][c] = 0.0;
			}
	}

	/* Eye heights: gap between adjacent levels minus 6*sigma (3-sigma per side) */
	for (i = 0; i < NUM_EYES; i++) {
		double gap = lane->level[i + 1] - lane->level[i];
		eye_heights[i] = gap - 6.0 * sigma;
		if (eye_heights[i] < 0.0)
			eye_heights[i] = 0.0;
	}
}

/* Find the closest grid row to a given Y value */
static int y_to_row(double y_val, double y_min, double y_max)
{
	int row;

	if (y_max <= y_min)
		return 0;
	row = (int)((y_max - y_val) / (y_max - y_min) * (EYE_ROWS - 1) + 0.5);
	if (row < 0)
		row = 0;
	if (row >= EYE_ROWS)
		row = EYE_ROWS - 1;
	return row;
}

static void print_eye_text(const struct vdm_pam4_lane *lane,
			   double grid[EYE_ROWS][EYE_COLS],
			   const double *eye_heights)
{
	double span = lane->level[3] - lane->level[0];
	double y_min = lane->level[0] - EYE_MARGIN * span;
	double y_max = lane->level[3] + EYE_MARGIN * span;
	int level_rows[NUM_LEVELS];
	int r, c, i;

	/* Header */
	printf("\tLane %d", lane->lane);
	if (lane->has_snr)
		printf("  SNR: %.1f dB", lane->snr_db);
	if (lane->has_ber)
		printf("  BER: %.2e", lane->ber);
	if (lane->has_mpi)
		printf("  MPI: %.1f dB", lane->mpi_db);
	printf("  Levels: %.0f/%.0f/%.0f/%.0f mV\n",
	       lane->level[0], lane->level[1],
	       lane->level[2], lane->level[3]);

	/* Compute level marker rows */
	for (i = 0; i < NUM_LEVELS; i++)
		level_rows[i] = y_to_row(lane->level[i], y_min, y_max);

	/* Render grid */
	for (r = 0; r < EYE_ROWS; r++) {
		double y_val = y_max - r * (y_max - y_min) / (EYE_ROWS - 1);
		char label[8] = "      ";

		/* Y-axis label: level markers or voltage */
		for (i = NUM_LEVELS - 1; i >= 0; i--) {
			if (r == level_rows[i]) {
				snprintf(label, sizeof(label), "  L%d  ", i);
				break;
			}
		}
		if (label[0] == ' ' && label[1] == ' ' && label[2] != 'L')
			snprintf(label, sizeof(label), "%+5.0f ", y_val);

		printf("\t%s|", label);

		for (c = 0; c < EYE_COLS; c++) {
			int idx = (int)(grid[r][c] * (DENSITY_LEVELS - 1) +
					0.5);
			if (idx < 0)
				idx = 0;
			if (idx >= DENSITY_LEVELS)
				idx = DENSITY_LEVELS - 1;
			printf("%s", density_chars[idx]);
		}

		printf("|\n");
	}

	/* X-axis */
	printf("\t      +");
	for (c = 0; c < EYE_COLS; c++)
		printf("-");
	printf("+\n");
	printf("\t       0");
	for (c = 0; c < EYE_COLS - 16; c++)
		printf(" ");
	printf("0.5 UI");
	for (c = 0; c < 2; c++)
		printf(" ");
	printf("1\n");

	/* Eye heights */
	printf("\t  Eye heights:");
	for (i = 0; i < NUM_EYES; i++)
		printf("  Eye%d=%.1f", i, eye_heights[i]);
	printf(" mV\n");

	printf("\t  NOTE: Vertical from VDM amplitude/SNR."
	       " Horizontal modeled (no jitter data).\n\n");
}

static void print_eye_json(const struct vdm_pam4_lane *lane,
			   double grid[EYE_ROWS][EYE_COLS],
			   const double *eye_heights)
{
	double span = lane->level[3] - lane->level[0];
	double y_min = lane->level[0] - EYE_MARGIN * span;
	double y_max = lane->level[3] + EYE_MARGIN * span;
	int level_rows[NUM_LEVELS];
	int r, c, i;
	char line[EYE_COLS * 4 + 1]; /* UTF-8 chars up to 3 bytes each + NUL */

	open_json_object(NULL);
	print_uint(PRINT_JSON, "lane", "%u", lane->lane);

	open_json_array("levels_mv", NULL);
	for (i = 0; i < NUM_LEVELS; i++)
		print_float(PRINT_JSON, NULL, "%.1f", lane->level[i]);
	close_json_array(NULL);

	if (lane->has_snr)
		print_float(PRINT_JSON, "snr_db", "%.1f", lane->snr_db);
	if (lane->has_ber)
		print_float(PRINT_JSON, "ber", "%.2e", lane->ber);
	if (lane->has_mpi)
		print_float(PRINT_JSON, "mpi_db", "%.1f", lane->mpi_db);

	open_json_array("eye_heights_mv", NULL);
	for (i = 0; i < NUM_EYES; i++)
		print_float(PRINT_JSON, NULL, "%.1f", eye_heights[i]);
	close_json_array(NULL);

	/* Level marker rows */
	for (i = 0; i < NUM_LEVELS; i++)
		level_rows[i] = y_to_row(lane->level[i], y_min, y_max);

	/* ASCII art lines */
	open_json_array("ascii", NULL);
	for (r = 0; r < EYE_ROWS; r++) {
		double y_val = y_max - r * (y_max - y_min) / (EYE_ROWS - 1);
		char label[8] = "      ";
		int pos;

		for (i = NUM_LEVELS - 1; i >= 0; i--) {
			if (r == level_rows[i]) {
				snprintf(label, sizeof(label), "  L%d  ", i);
				break;
			}
		}
		if (label[0] == ' ' && label[1] == ' ' && label[2] != 'L')
			snprintf(label, sizeof(label), "%+5.0f ", y_val);

		pos = snprintf(line, sizeof(line), "%s|", label);
		for (c = 0; c < EYE_COLS; c++) {
			int idx = (int)(grid[r][c] * (DENSITY_LEVELS - 1) +
					0.5);
			if (idx < 0)
				idx = 0;
			if (idx >= DENSITY_LEVELS)
				idx = DENSITY_LEVELS - 1;
			pos += snprintf(line + pos, sizeof(line) - pos,
					"%s", density_chars[idx]);
		}
		snprintf(line + pos, sizeof(line) - pos, "|");
		print_string(PRINT_JSON, NULL, "%s", line);
	}
	close_json_array(NULL);

	close_json_object();
}

/* --- IQ Constellation Diagram --- */

#define CONST_SIZE	41	/* 41x41 grid */
#define CONST_RANGE	1.3	/* IQ axis range [-1.3, +1.3] */
#define CONST_SIGMA_DEFAULT	0.10	/* Default sigma when no EVM/eSNR */
#define CONST_MAX_POINTS	64	/* Max constellation points (64-QAM) */

struct iq_point {
	double i;
	double q;
};

enum cmis_modulation {
	CMIS_MOD_UNKNOWN = 0,
	CMIS_MOD_QPSK,		/* 4 points */
	CMIS_MOD_8QAM,		/* 8 points */
	CMIS_MOD_16QAM,		/* 16 points */
	CMIS_MOD_64QAM,		/* 64 points */
};

static const char *modulation_name(enum cmis_modulation mod)
{
	switch (mod) {
	case CMIS_MOD_QPSK:	return "DP-QPSK";
	case CMIS_MOD_8QAM:	return "DP-8QAM";
	case CMIS_MOD_16QAM:	return "DP-16QAM";
	case CMIS_MOD_64QAM:	return "DP-64QAM";
	default:		return "Unknown";
	}
}

/*
 * Detect modulation from active application descriptor.
 * Reads AppSel from Page 11h, then indexes into the application descriptor
 * table in lower memory and looks up the media interface ID name.
 */
static enum cmis_modulation detect_modulation(const struct cmis_memory_map *map)
{
	const __u8 *page_11;
	__u8 appsel, module_type, media_id;
	int app_idx;
	const char *name;

	/* Page 11h: Active Control Set */
	page_11 = map->upper_memory[0][0x11];
	if (!page_11)
		return CMIS_MOD_UNKNOWN;

	/* Active AppSel for lane 0: upper nibble of byte 0xCE */
	appsel = (page_11[CMIS_ACS_DPCONFIG_LANE(0)] >> 4) & 0x0F;
	if (appsel == 0)
		return CMIS_MOD_UNKNOWN;

	/* Module type from lower memory */
	if (!map->lower_memory)
		return CMIS_MOD_UNKNOWN;
	module_type = map->lower_memory[CMIS_MODULE_TYPE_OFFSET];

	/* Application descriptor: 4 bytes per app, starting at 0x56 */
	app_idx = appsel - 1;
	if (app_idx >= CMIS_MAX_APP_DESCS)
		return CMIS_MOD_UNKNOWN;

	media_id = map->lower_memory[CMIS_APP_DESC_START_OFFSET +
				     app_idx * CMIS_APP_DESC_SIZE + 1];

	/* Well-known coherent media IDs (hardcoded fallbacks) */
	switch (media_id) {
	case 0x3E: /* 400ZR DWDM amplified */
	case 0x3F: /* 400ZR Unamplified */
	case 0x4D: /* 400GBASE-ZR */
		return CMIS_MOD_16QAM;
	case 0x44: /* 100GBASE-ZR */
		return CMIS_MOD_QPSK;
	case 0x6C: /* 800ZR-A DWDM */
	case 0x6D: /* 800ZR-B DWDM */
		return CMIS_MOD_64QAM;
	case 0x6E: /* 800ZR-C DWDM */
		return CMIS_MOD_16QAM;
	}

	/* Try name-based detection from SFF-8024 table */
	name = sff8024_media_id_name(module_type, media_id);
	if (name) {
		if (strstr(name, "64QAM"))
			return CMIS_MOD_64QAM;
		if (strstr(name, "16QAM"))
			return CMIS_MOD_16QAM;
		if (strstr(name, "8QAM"))
			return CMIS_MOD_8QAM;
		if (strstr(name, "QPSK"))
			return CMIS_MOD_QPSK;
	}

	return CMIS_MOD_UNKNOWN;
}

/*
 * Generate ideal constellation points for a given modulation.
 * Points are normalized to unit average energy.
 * Returns number of points.
 */
static int init_constellation(enum cmis_modulation mod,
			      struct iq_point *points)
{
	int n = 0;
	int i, q;
	double norm;

	switch (mod) {
	case CMIS_MOD_QPSK:
		/* 4 points at (+-1, +-1)/sqrt(2) */
		norm = 1.0 / sqrt(2.0);
		for (i = -1; i <= 1; i += 2)
			for (q = -1; q <= 1; q += 2)
				points[n++] = (struct iq_point){
					i * norm, q * norm};
		break;

	case CMIS_MOD_8QAM:
		/* Star-8QAM: inner ring (4 diagonal) + outer ring (4 axis) */
		norm = 1.0 / sqrt(1.0 + 3.0); /* normalize avg energy */
		/* Inner ring: 4 points at 45/135/225/315 deg, radius 1 */
		for (i = -1; i <= 1; i += 2)
			for (q = -1; q <= 1; q += 2)
				points[n++] = (struct iq_point){
					i * norm, q * norm};
		/* Outer ring: 4 axis points, radius sqrt(3) */
		{
			double r_out = sqrt(3.0) * norm;
			points[n++] = (struct iq_point){ r_out, 0.0};
			points[n++] = (struct iq_point){-r_out, 0.0};
			points[n++] = (struct iq_point){ 0.0,  r_out};
			points[n++] = (struct iq_point){ 0.0, -r_out};
		}
		break;

	case CMIS_MOD_16QAM:
		/* 4x4 grid at {-3,-1,+1,+3}/sqrt(10) */
		norm = 1.0 / sqrt(10.0);
		for (i = -3; i <= 3; i += 2)
			for (q = -3; q <= 3; q += 2)
				points[n++] = (struct iq_point){
					i * norm, q * norm};
		break;

	case CMIS_MOD_64QAM:
		/* 8x8 grid at {-7,-5,-3,-1,+1,+3,+5,+7}/sqrt(42) */
		norm = 1.0 / sqrt(42.0);
		for (i = -7; i <= 7; i += 2)
			for (q = -7; q <= 7; q += 2)
				points[n++] = (struct iq_point){
					i * norm, q * norm};
		break;

	default:
		break;
	}

	return n;
}

/*
 * Compute noise sigma from EVM/eSNR.
 * EVM (%) = 100 * sigma (for unit-energy constellation).
 * eSNR (dB) -> sigma = 1/10^(eSNR/20).
 */
static double coherent_sigma(const struct vdm_coherent_lane *lane)
{
	if (lane->has_evm && lane->evm_pct > 0.0)
		return lane->evm_pct / 100.0;
	if (lane->has_esnr && lane->esnr_db > 0.0)
		return 1.0 / pow(10.0, lane->esnr_db / 20.0);
	return CONST_SIGMA_DEFAULT;
}

/*
 * Render constellation for one lane into grid[CONST_SIZE][CONST_SIZE].
 * Grid values are normalized to 0..1.
 */
static void render_constellation(const struct vdm_coherent_lane *lane,
				 const struct iq_point *points, int npts,
				 double grid[CONST_SIZE][CONST_SIZE])
{
	double sigma = coherent_sigma(lane);
	int r, c, p;
	double log_min, log_max;
	bool first;

	for (r = 0; r < CONST_SIZE; r++) {
		double q_val = CONST_RANGE - r * 2.0 * CONST_RANGE /
			       (CONST_SIZE - 1);

		for (c = 0; c < CONST_SIZE; c++) {
			double i_val = -CONST_RANGE + c * 2.0 * CONST_RANGE /
				       (CONST_SIZE - 1);
			double density = 0.0;

			for (p = 0; p < npts; p++)
				density += gaussian(i_val, points[p].i,
						    sigma) *
					   gaussian(q_val, points[p].q,
						    sigma);

			grid[r][c] = density / npts;
		}
	}

	/* Log-scale normalization */
	log_min = 1e30;
	log_max = -1e30;
	first = true;
	for (r = 0; r < CONST_SIZE; r++) {
		for (c = 0; c < CONST_SIZE; c++) {
			if (grid[r][c] > 1e-12) {
				double lv = log10(grid[r][c]);
				if (first || lv < log_min)
					log_min = lv;
				if (first || lv > log_max)
					log_max = lv;
				first = false;
				grid[r][c] = lv;
			} else {
				grid[r][c] = -999.0;
			}
		}
	}

	if (log_max > log_min) {
		double range = log_max - log_min;
		for (r = 0; r < CONST_SIZE; r++)
			for (c = 0; c < CONST_SIZE; c++) {
				if (grid[r][c] > -900.0)
					grid[r][c] = (grid[r][c] - log_min) /
						     range;
				else
					grid[r][c] = 0.0;
			}
	}
}

static void print_const_text(const struct vdm_coherent_lane *lane,
			     const char *mod_name,
			     double grid[CONST_SIZE][CONST_SIZE])
{
	int r, c;
	int mid = CONST_SIZE / 2;

	/* Header */
	printf("\tLane %d  %s", lane->lane, mod_name);
	if (lane->has_evm)
		printf("  EVM: %.1f%%", lane->evm_pct);
	if (lane->has_esnr)
		printf("  eSNR: %.1f dB", lane->esnr_db);
	if (lane->has_osnr)
		printf("  OSNR: %.1f dB", lane->osnr_db);
	if (lane->has_q_factor)
		printf("  Q: %.1f dB", lane->q_factor_db);
	printf("\n");

	/* Render grid */
	for (r = 0; r < CONST_SIZE; r++) {
		char label[8] = "      ";

		/* Y-axis labels: +1, 0, -1 */
		if (r == 0)
			snprintf(label, sizeof(label), "  +1  ");
		else if (r == mid)
			snprintf(label, sizeof(label), "   Q  ");
		else if (r == CONST_SIZE - 1)
			snprintf(label, sizeof(label), "  -1  ");

		printf("\t%s|", label);

		for (c = 0; c < CONST_SIZE; c++) {
			int idx = (int)(grid[r][c] * (DENSITY_LEVELS - 1) +
					0.5);
			if (idx < 0)
				idx = 0;
			if (idx >= DENSITY_LEVELS)
				idx = DENSITY_LEVELS - 1;
			printf("%s", density_chars[idx]);
		}

		printf("|\n");
	}

	/* X-axis */
	printf("\t      +");
	for (c = 0; c < CONST_SIZE; c++)
		printf("-");
	printf("+\n");
	printf("\t      -1");
	for (c = 0; c < CONST_SIZE / 2 - 2; c++)
		printf(" ");
	printf("I");
	for (c = 0; c < CONST_SIZE / 2 - 1; c++)
		printf(" ");
	printf("+1\n");

	printf("\t  NOTE: Synthetic from EVM/eSNR."
	       " No raw IQ samples available.\n\n");
}

static void print_const_json(const struct vdm_coherent_lane *lane,
			     const char *mod_name,
			     double grid[CONST_SIZE][CONST_SIZE])
{
	int r, c;
	int mid = CONST_SIZE / 2;
	char line[CONST_SIZE * 4 + 16];

	open_json_object(NULL);
	print_uint(PRINT_JSON, "lane", "%u", lane->lane);
	print_string(PRINT_JSON, "modulation", "%s", mod_name);

	if (lane->has_evm)
		print_float(PRINT_JSON, "evm_pct", "%.1f", lane->evm_pct);
	if (lane->has_esnr)
		print_float(PRINT_JSON, "esnr_db", "%.1f", lane->esnr_db);
	if (lane->has_osnr)
		print_float(PRINT_JSON, "osnr_db", "%.1f", lane->osnr_db);
	if (lane->has_snr_margin)
		print_float(PRINT_JSON, "snr_margin_db", "%.1f",
			    lane->snr_margin_db);
	if (lane->has_q_factor)
		print_float(PRINT_JSON, "q_factor_db", "%.1f",
			    lane->q_factor_db);

	open_json_array("ascii", NULL);
	for (r = 0; r < CONST_SIZE; r++) {
		char label[8] = "      ";
		int pos;

		if (r == 0)
			snprintf(label, sizeof(label), "  +1  ");
		else if (r == mid)
			snprintf(label, sizeof(label), "   Q  ");
		else if (r == CONST_SIZE - 1)
			snprintf(label, sizeof(label), "  -1  ");

		pos = snprintf(line, sizeof(line), "%s|", label);
		for (c = 0; c < CONST_SIZE; c++) {
			int idx = (int)(grid[r][c] * (DENSITY_LEVELS - 1) +
					0.5);
			if (idx < 0)
				idx = 0;
			if (idx >= DENSITY_LEVELS)
				idx = DENSITY_LEVELS - 1;
			pos += snprintf(line + pos, sizeof(line) - pos,
					"%s", density_chars[idx]);
		}
		snprintf(line + pos, sizeof(line) - pos, "|");
		print_string(PRINT_JSON, NULL, "%s", line);
	}
	close_json_array(NULL);

	close_json_object();
}

/* --- Main entry point --- */

void cmis_show_eye(const struct cmis_memory_map *map)
{
	struct vdm_pam4_lane pam4_lanes[CMIS_VDM_MAX_LANES];
	struct vdm_coherent_lane coh_lanes[CMIS_VDM_MAX_LANES];
	int pam4_count, coh_count, i;

	pam4_count = cmis_vdm_get_pam4_data(map, pam4_lanes,
					     CMIS_VDM_MAX_LANES);
	coh_count = cmis_vdm_get_coherent_data(map, coh_lanes,
					       CMIS_VDM_MAX_LANES);

	if (pam4_count == 0 && coh_count == 0) {
		if (is_json_context()) {
			open_json_object("eye_diagram");
			print_string(PRINT_JSON, "error", "%s",
				     "No PAM4 level or coherent EVM/eSNR"
				     " data in VDM instances");
			close_json_object();
		} else {
			printf("\tNo PAM4 level or coherent EVM/eSNR"
			       " data in VDM instances.\n");
		}
		return;
	}

	/* Constellation diagram (coherent) */
	if (coh_count > 0) {
		enum cmis_modulation mod;
		const char *mod_name;
		struct iq_point points[CONST_MAX_POINTS];
		double cgrid[CONST_SIZE][CONST_SIZE];
		int npts;

		mod = detect_modulation(map);
		if (mod == CMIS_MOD_UNKNOWN)
			mod = CMIS_MOD_16QAM; /* safe default for coherent */
		mod_name = modulation_name(mod);
		npts = init_constellation(mod, points);

		if (is_json_context()) {
			open_json_object("constellation");
			print_string(PRINT_JSON, "modulation", "%s", mod_name);
			open_json_array("lanes", NULL);
		}

		for (i = 0; i < coh_count; i++) {
			render_constellation(&coh_lanes[i], points, npts,
					     cgrid);

			if (is_json_context())
				print_const_json(&coh_lanes[i], mod_name,
						 cgrid);
			else
				print_const_text(&coh_lanes[i], mod_name,
						 cgrid);
		}

		if (is_json_context()) {
			close_json_array(NULL);
			close_json_object();
		}
	}

	/* PAM4 eye diagram */
	if (pam4_count > 0) {
		double egrid[EYE_ROWS][EYE_COLS];
		double eye_heights[NUM_EYES];

		if (is_json_context()) {
			open_json_object("eye_diagram");
			open_json_array("lanes", NULL);
		}

		for (i = 0; i < pam4_count; i++) {
			render_eye(&pam4_lanes[i], egrid, eye_heights);

			if (is_json_context())
				print_eye_json(&pam4_lanes[i], egrid,
					       eye_heights);
			else
				print_eye_text(&pam4_lanes[i], egrid,
					       eye_heights);
		}

		if (is_json_context()) {
			close_json_array(NULL);
			close_json_object();
		}
	}
}
