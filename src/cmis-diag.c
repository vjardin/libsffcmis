/**
 * cmis-diag.c: CMIS Diagnostics Capabilities and Results (Pages 13h-14h)
 *
 * Display of diagnostic capabilities (loopback modes, PRBS patterns,
 * measurement/reporting) and current BER/SNR values, plus PRBS/BER
 * test control (start/stop/read).
 * OIF-CMIS-05.3, Section 8.10, Tables 8-97 through 8-115.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <math.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-diag.h"

/* Page convenience macros */
#define page_01h upper_memory[0x0][0x1]

/* PRBS pattern names (Table 8-105), indexed 0-15 */
static const char *prbs_pattern_names[] = {
	"PRBS-31Q",	/* 0 */
	"PRBS-31",	/* 1 */
	"PRBS-23Q",	/* 2 */
	"PRBS-23",	/* 3 */
	"PRBS-15Q",	/* 4 */
	"PRBS-15",	/* 5 */
	"PRBS-13Q",	/* 6 */
	"PRBS-13",	/* 7 */
	"PRBS-9Q",	/* 8 */
	"PRBS-9",	/* 9 */
	"PRBS-7Q",	/* 10 */
	"PRBS-7",	/* 11 */
	"SSPRQ",	/* 12 */
	"Custom",	/* 13 */
	"User",		/* 14 */
	"Reserved",	/* 15 */
};

/* F16 (IEEE 754 half-precision) to double — same as cmis-vdm.c */
static double f16_to_double(__u16 raw)
{
	int sign = (raw >> 15) & 1;
	int exp = (raw >> 10) & 0x1F;
	int mant = raw & 0x3FF;
	double val;

	if (exp == 0) {
		val = ldexp((double)mant, -24);
	} else if (exp == 0x1F) {
		val = (mant == 0) ? INFINITY : NAN;
	} else {
		val = ldexp((double)(mant + 1024), exp - 25);
	}

	return sign ? -val : val;
}

/*
 * Print a PRBS pattern support bitmap as a comma-separated list.
 */
static void
print_pattern_list(const __u8 *page_13h, __u8 offset, const char *label,
		   const char *json_key)
{
	__u16 bitmap = OFFSET_TO_U16_PTR(page_13h, offset);
	char buf[256];
	int pos = 0;
	int i;

	for (i = 0; i < CMIS_DIAG_NUM_PRBS_PATTERNS && pos < (int)sizeof(buf) - 1; i++) {
		if (bitmap & (1 << (15 - i))) {
			if (pos > 0)
				pos += snprintf(buf + pos, sizeof(buf) - pos,
						", ");
			pos += snprintf(buf + pos, sizeof(buf) - pos,
					"%s", prbs_pattern_names[i]);
		}
	}
	if (pos == 0)
		snprintf(buf, sizeof(buf), "(none)");

	if (is_json_context())
		print_string(PRINT_JSON, json_key, "%s", buf);
	else
		printf("\t  %-39s : %s\n", label, buf);
}

static void
cmis_show_diag_caps(const struct cmis_memory_map *map)
{
	const __u8 *page_13h = map->upper_memory[0][0x13];
	__u8 lb_caps, meas_caps, report_caps, pattern_loc;

	if (!page_13h)
		return;

	lb_caps = page_13h[CMIS_DIAG_LOOPBACK_CAPS];
	meas_caps = page_13h[CMIS_DIAG_MEAS_CAPS];
	report_caps = page_13h[CMIS_DIAG_REPORT_CAPS];
	pattern_loc = page_13h[CMIS_DIAG_PATTERN_LOC];

	/* Loopback capabilities */
	if (is_json_context()) {
		open_json_object("loopback_caps");
		print_bool(PRINT_JSON, "media_side_output", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_MEDIA_OUT));
		print_bool(PRINT_JSON, "media_side_input", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_MEDIA_IN));
		print_bool(PRINT_JSON, "host_side_output", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_HOST_OUT));
		print_bool(PRINT_JSON, "host_side_input", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_HOST_IN));
		print_bool(PRINT_JSON, "per_lane_media", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_PL_MEDIA));
		print_bool(PRINT_JSON, "per_lane_host", NULL,
			   !!(lb_caps & CMIS_DIAG_LOOPBACK_PL_HOST));
		close_json_object();
	} else {
		printf("\t  %-39s :\n", "Loopback capabilities");
		printf("\t    %-37s : %s\n", "Media side output",
		       (lb_caps & CMIS_DIAG_LOOPBACK_MEDIA_OUT) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Media side input",
		       (lb_caps & CMIS_DIAG_LOOPBACK_MEDIA_IN) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Host side output",
		       (lb_caps & CMIS_DIAG_LOOPBACK_HOST_OUT) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Host side input",
		       (lb_caps & CMIS_DIAG_LOOPBACK_HOST_IN) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Per-lane control (media)",
		       (lb_caps & CMIS_DIAG_LOOPBACK_PL_MEDIA) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Per-lane control (host)",
		       (lb_caps & CMIS_DIAG_LOOPBACK_PL_HOST) ? "Yes" : "No");
	}

	/* Measurement capabilities */
	if (is_json_context()) {
		open_json_object("measurement_caps");
		print_bool(PRINT_JSON, "gating", NULL,
			   !!(meas_caps & CMIS_DIAG_MEAS_GATING));
		print_bool(PRINT_JSON, "gating_accuracy_2ms", NULL,
			   !!(meas_caps & CMIS_DIAG_MEAS_GATING_ACC_2MS));
		print_bool(PRINT_JSON, "auto_restart", NULL,
			   !!(meas_caps & CMIS_DIAG_MEAS_AUTO_RESTART));
		print_bool(PRINT_JSON, "periodic_update", NULL,
			   !!(meas_caps & CMIS_DIAG_MEAS_PERIODIC_UPD));
		close_json_object();
	} else {
		printf("\t  %-39s :\n", "Measurement capabilities");
		printf("\t    %-37s : %s\n", "Gating",
		       (meas_caps & CMIS_DIAG_MEAS_GATING) ? "Yes" : "No");
		if (meas_caps & CMIS_DIAG_MEAS_GATING)
			printf("\t    %-37s : %s\n", "Gating accuracy",
			       (meas_caps & CMIS_DIAG_MEAS_GATING_ACC_2MS) ?
			       "< 2 ms" : ">= 2 ms");
		printf("\t    %-37s : %s\n", "Auto-restart gating",
		       (meas_caps & CMIS_DIAG_MEAS_AUTO_RESTART) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Periodic updates",
		       (meas_caps & CMIS_DIAG_MEAS_PERIODIC_UPD) ? "Yes" : "No");
	}

	/* Reporting capabilities */
	if (is_json_context()) {
		open_json_object("reporting_caps");
		print_bool(PRINT_JSON, "ber_results", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_BER));
		print_bool(PRINT_JSON, "error_counting", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_ERR_CNT));
		print_bool(PRINT_JSON, "snr_host", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_SNR_HOST));
		print_bool(PRINT_JSON, "snr_media", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_SNR_MEDIA));
		print_bool(PRINT_JSON, "fec_host", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_FEC_HOST));
		print_bool(PRINT_JSON, "fec_media", NULL,
			   !!(report_caps & CMIS_DIAG_REPORT_FEC_MEDIA));
		close_json_object();
	} else {
		printf("\t  %-39s :\n", "Reporting capabilities");
		printf("\t    %-37s : %s\n", "BER results",
		       (report_caps & CMIS_DIAG_REPORT_BER) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Error counting",
		       (report_caps & CMIS_DIAG_REPORT_ERR_CNT) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "SNR host",
		       (report_caps & CMIS_DIAG_REPORT_SNR_HOST) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "SNR media",
		       (report_caps & CMIS_DIAG_REPORT_SNR_MEDIA) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "FEC host",
		       (report_caps & CMIS_DIAG_REPORT_FEC_HOST) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "FEC media",
		       (report_caps & CMIS_DIAG_REPORT_FEC_MEDIA) ? "Yes" : "No");
	}

	/* Pattern generator/checker locations */
	if (is_json_context()) {
		open_json_object("pattern_locations");
		print_bool(PRINT_JSON, "gen_host", NULL,
			   !!(pattern_loc & CMIS_DIAG_PATTERN_GEN_HOST));
		print_bool(PRINT_JSON, "gen_media", NULL,
			   !!(pattern_loc & CMIS_DIAG_PATTERN_GEN_MEDIA));
		print_bool(PRINT_JSON, "chk_host", NULL,
			   !!(pattern_loc & CMIS_DIAG_PATTERN_CHK_HOST));
		print_bool(PRINT_JSON, "chk_media", NULL,
			   !!(pattern_loc & CMIS_DIAG_PATTERN_CHK_MEDIA));
		close_json_object();
	} else {
		printf("\t  %-39s :\n", "Pattern gen/check locations");
		printf("\t    %-37s : %s\n", "Host generator",
		       (pattern_loc & CMIS_DIAG_PATTERN_GEN_HOST) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Media generator",
		       (pattern_loc & CMIS_DIAG_PATTERN_GEN_MEDIA) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Host checker",
		       (pattern_loc & CMIS_DIAG_PATTERN_CHK_HOST) ? "Yes" : "No");
		printf("\t    %-37s : %s\n", "Media checker",
		       (pattern_loc & CMIS_DIAG_PATTERN_CHK_MEDIA) ? "Yes" : "No");
	}

	/* PRBS pattern support bitmaps */
	if (pattern_loc & CMIS_DIAG_PATTERN_GEN_HOST)
		print_pattern_list(page_13h, CMIS_DIAG_PATTERN_HOST_GEN,
				   "Host generator patterns",
				   "host_gen_patterns");
	if (pattern_loc & CMIS_DIAG_PATTERN_CHK_HOST)
		print_pattern_list(page_13h, CMIS_DIAG_PATTERN_HOST_CHK,
				   "Host checker patterns",
				   "host_chk_patterns");
	if (pattern_loc & CMIS_DIAG_PATTERN_GEN_MEDIA)
		print_pattern_list(page_13h, CMIS_DIAG_PATTERN_MEDIA_GEN,
				   "Media generator patterns",
				   "media_gen_patterns");
	if (pattern_loc & CMIS_DIAG_PATTERN_CHK_MEDIA)
		print_pattern_list(page_13h, CMIS_DIAG_PATTERN_MEDIA_CHK,
				   "Media checker patterns",
				   "media_chk_patterns");

	/* Active loopback state */
	if (!is_json_context()) {
		__u8 lb;

		printf("\t  %-39s :\n", "Active loopback");
		lb = page_13h[CMIS_DIAG_LOOPBACK_MEDIA_OUT_CTL];
		printf("\t    %-37s : 0x%02x%s\n", "Media output",
		       lb, lb ? " (active)" : "");
		lb = page_13h[CMIS_DIAG_LOOPBACK_MEDIA_IN_CTL];
		printf("\t    %-37s : 0x%02x%s\n", "Media input",
		       lb, lb ? " (active)" : "");
		lb = page_13h[CMIS_DIAG_LOOPBACK_HOST_OUT_CTL];
		printf("\t    %-37s : 0x%02x%s\n", "Host output",
		       lb, lb ? " (active)" : "");
		lb = page_13h[CMIS_DIAG_LOOPBACK_HOST_IN_CTL];
		printf("\t    %-37s : 0x%02x%s\n", "Host input",
		       lb, lb ? " (active)" : "");
	}
}

static void
cmis_show_diag_data(const struct cmis_memory_map *map)
{
	const __u8 *page_14h = map->upper_memory[0][0x14];
	__u8 selector;
	int i;

	if (!page_14h)
		return;

	selector = page_14h[CMIS_DIAG_SELECTOR];

	if (is_json_context()) {
		print_uint(PRINT_JSON, "diagnostics_selector", "%u", selector);
	} else {
		printf("\t  %-39s : 0x%02x", "DiagnosticsSelector", selector);
	}

	/* Decode and display based on current selector value */
	switch (selector) {
	case CMIS_DIAG_SEL_BER_RT:
	case CMIS_DIAG_SEL_BER_GATED: {
		/* F16 BER per lane, 8 host + 8 media = 32 bytes */
		if (!is_json_context()) {
			printf(" (%s BER)\n",
			       selector == CMIS_DIAG_SEL_BER_RT ?
			       "real-time" : "gated");
			printf("\t  %-39s :\n", "BER per lane");
			printf("\t    %-6s %-14s %-14s\n",
			       "Lane", "Host BER", "Media BER");
			printf("\t    %-6s %-14s %-14s\n",
			       "----", "--------------", "--------------");
		} else {
			open_json_array("ber_per_lane", NULL);
		}
		for (i = 0; i < 8; i++) {
			int h_off = CMIS_DIAG_DATA_START + i * 2;
			int m_off = CMIS_DIAG_DATA_START + 16 + i * 2;
			double h_ber = f16_to_double(
				OFFSET_TO_U16_PTR(page_14h, h_off));
			double m_ber = f16_to_double(
				OFFSET_TO_U16_PTR(page_14h, m_off));

			if (is_json_context()) {
				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u", i + 1);
				print_float(PRINT_JSON, "host_ber",
					    "%.3e", h_ber);
				print_float(PRINT_JSON, "media_ber",
					    "%.3e", m_ber);
				close_json_object();
			} else {
				printf("\t    %-6d %-14.3e %-14.3e\n",
				       i + 1, h_ber, m_ber);
			}
		}
		if (is_json_context())
			close_json_array(NULL);
		break;
	}
	case CMIS_DIAG_SEL_SNR: {
		/* U16 LE SNR, unit 1/256 dB, 8 host + 8 media */
		if (!is_json_context()) {
			printf(" (SNR)\n");
			printf("\t  %-39s :\n", "SNR per lane");
			printf("\t    %-6s %-14s %-14s\n",
			       "Lane", "Host SNR (dB)", "Media SNR (dB)");
			printf("\t    %-6s %-14s %-14s\n",
			       "----", "--------------", "--------------");
		} else {
			open_json_array("snr_per_lane", NULL);
		}
		for (i = 0; i < 8; i++) {
			int h_off = CMIS_DIAG_DATA_START + i * 2;
			int m_off = CMIS_DIAG_DATA_START + 16 + i * 2;
			/* U16 little-endian */
			__u16 h_raw = page_14h[h_off] |
				      (page_14h[h_off + 1] << 8);
			__u16 m_raw = page_14h[m_off] |
				      (page_14h[m_off + 1] << 8);
			double h_snr = (double)h_raw / 256.0;
			double m_snr = (double)m_raw / 256.0;

			if (is_json_context()) {
				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u", i + 1);
				print_float(PRINT_JSON, "host_snr_db",
					    "%.1f", h_snr);
				print_float(PRINT_JSON, "media_snr_db",
					    "%.1f", m_snr);
				close_json_object();
			} else {
				printf("\t    %-6d %-14.1f %-14.1f\n",
				       i + 1, h_snr, m_snr);
			}
		}
		if (is_json_context())
			close_json_array(NULL);
		break;
	}
	default:
		if (!is_json_context())
			printf("\n");
		break;
	}
}

void cmis_show_diag(const struct cmis_memory_map *map)
{
	if (!map->page_01h)
		return;

	/* Check diagnostics advertisement (01h:142 bit 5) */
	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_DIAG))
		return;

	if (!map->upper_memory[0][0x13])
		return;

	if (is_json_context()) {
		open_json_object("diagnostics");
	} else {
		printf("\n\t%-41s :\n",
		       "Diagnostics (Pages 13h-14h)");
	}

	cmis_show_diag_caps(map);
	cmis_show_diag_data(map);

	if (is_json_context())
		close_json_object();
}

/*----------------------------------------------------------------------
 * PRBS/BER test control functions
 *----------------------------------------------------------------------*/

const char *cmis_diag_pattern_name(int index)
{
	if (index < 0 || index >= CMIS_DIAG_NUM_PRBS_PATTERNS)
		return NULL;
	return prbs_pattern_names[index];
}

int cmis_diag_pattern_lookup(const char *name)
{
	int i;

	for (i = 0; i < CMIS_DIAG_NUM_PRBS_PATTERNS; i++) {
		if (strcasecmp(name, prbs_pattern_names[i]) == 0)
			return i;
	}
	return -1;
}

/*
 * Write a single byte to a CMIS page via I2C.
 */
static int
diag_write_byte(struct cmd_context *ctx, int bank, __u8 page, __u8 offset,
		__u8 val)
{
	struct module_eeprom request;

	cmis_request_init(&request, bank, page, offset);
	request.length = 1;
	request.data = &val;
	return set_eeprom_page(ctx, &request);
}

/*
 * Write multiple bytes to a CMIS page via I2C.
 */
static int
diag_write_bytes(struct cmd_context *ctx, int bank, __u8 page, __u8 offset,
		 __u8 *data, int len)
{
	struct module_eeprom request;

	cmis_request_init(&request, bank, page, offset);
	request.length = len;
	request.data = data;
	return set_eeprom_page(ctx, &request);
}

/*
 * Validate that a pattern index is supported by the module.
 * Reads the 2-byte capability bitmap at caps_offset on Page 13h.
 * Bit (15 - pattern) must be set.
 */
static int
diag_validate_pattern(struct cmd_context *ctx, int bank,
		      __u8 pattern, __u8 caps_offset, const char *direction)
{
	struct module_eeprom request;
	__u16 bitmap;
	int ret;

	cmis_request_init(&request, bank, 0x13, caps_offset);
	request.length = 2;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read %s pattern caps\n",
			direction);
		return -1;
	}
	bitmap = (request.data[0] << 8) | request.data[1];

	if (!(bitmap & (1 << (15 - pattern)))) {
		fprintf(stderr, "Error: pattern %s not supported for %s "
			"(caps bitmap: 0x%04x)\n",
			prbs_pattern_names[pattern], direction, bitmap);
		return -1;
	}
	return 0;
}

/*
 * Write pattern select registers for the given lanes.
 * Pattern select packs 2 lanes per byte in 4-bit nibbles:
 * bits 7-4 = even lane (2,4,6,8), bits 3-0 = odd lane (1,3,5,7).
 * 4 bytes at pat_offset cover 8 lanes.
 */
static int
diag_write_pattern_select(struct cmd_context *ctx, int bank,
			  __u8 pat_offset, __u8 pattern, __u8 lane_mask)
{
	struct module_eeprom request;
	__u8 buf[4];
	int ret, i;

	/* Read current pattern select bytes */
	cmis_request_init(&request, bank, 0x13, pat_offset);
	request.length = 4;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	memcpy(buf, request.data, 4);

	/* Set pattern nibbles for enabled lanes */
	for (i = 0; i < 8; i++) {
		if (!(lane_mask & (1 << i)))
			continue;
		if (i & 1) {
			/* Even lane index (2,4,6,8 → array 1,3,5,7) in high nibble */
			buf[i / 2] = (buf[i / 2] & 0x0F) |
				     (pattern << 4);
		} else {
			/* Odd lane index (1,3,5,7 → array 0,2,4,6) in low nibble */
			buf[i / 2] = (buf[i / 2] & 0xF0) | (pattern & 0x0F);
		}
	}

	return diag_write_bytes(ctx, bank, 0x13, pat_offset, buf, 4);
}

int cmis_diag_bert_start(struct cmd_context *ctx, int bank,
			 const struct cmis_bert_config *cfg)
{
	int ret;

	/* Validate requested patterns against module capabilities */
	if (cfg->host_gen) {
		ret = diag_validate_pattern(ctx, bank, cfg->pattern,
					    CMIS_DIAG_PATTERN_HOST_GEN,
					    "host generator");
		if (ret < 0)
			return ret;
	}
	if (cfg->media_chk) {
		ret = diag_validate_pattern(ctx, bank, cfg->pattern,
					    CMIS_DIAG_PATTERN_MEDIA_CHK,
					    "media checker");
		if (ret < 0)
			return ret;
	}
	if (cfg->host_chk) {
		ret = diag_validate_pattern(ctx, bank, cfg->pattern,
					    CMIS_DIAG_PATTERN_HOST_CHK,
					    "host checker");
		if (ret < 0)
			return ret;
	}
	if (cfg->media_gen) {
		ret = diag_validate_pattern(ctx, bank, cfg->pattern,
					    CMIS_DIAG_PATTERN_MEDIA_GEN,
					    "media generator");
		if (ret < 0)
			return ret;
	}

	/* Configure host generator */
	if (cfg->host_gen) {
		ret = diag_write_pattern_select(ctx, bank,
						CMIS_DIAG_HOST_GEN_PAT_SEL,
						cfg->pattern, cfg->lanes);
		if (ret < 0)
			return ret;
		ret = diag_write_byte(ctx, bank, 0x13,
				      CMIS_DIAG_HOST_GEN_ENABLE, cfg->lanes);
		if (ret < 0)
			return ret;
	}

	/* Configure media checker */
	if (cfg->media_chk) {
		ret = diag_write_pattern_select(ctx, bank,
						CMIS_DIAG_MEDIA_CHK_PAT_SEL,
						cfg->pattern, cfg->lanes);
		if (ret < 0)
			return ret;
		ret = diag_write_byte(ctx, bank, 0x13,
				      CMIS_DIAG_MEDIA_CHK_ENABLE, cfg->lanes);
		if (ret < 0)
			return ret;
	}

	/* Configure host checker */
	if (cfg->host_chk) {
		ret = diag_write_pattern_select(ctx, bank,
						CMIS_DIAG_HOST_CHK_PAT_SEL,
						cfg->pattern, cfg->lanes);
		if (ret < 0)
			return ret;
		ret = diag_write_byte(ctx, bank, 0x13,
				      CMIS_DIAG_HOST_CHK_ENABLE, cfg->lanes);
		if (ret < 0)
			return ret;
	}

	/* Configure media generator */
	if (cfg->media_gen) {
		ret = diag_write_pattern_select(ctx, bank,
						CMIS_DIAG_MEDIA_GEN_PAT_SEL,
						cfg->pattern, cfg->lanes);
		if (ret < 0)
			return ret;
		ret = diag_write_byte(ctx, bank, 0x13,
				      CMIS_DIAG_MEDIA_GEN_ENABLE, cfg->lanes);
		if (ret < 0)
			return ret;
	}

	/* Start measurement: set StartStop (bit 7) + ResetErrorInfo (bit 5) */
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_MEAS_CONTROL,
			      CMIS_DIAG_MEAS_START_STOP |
			      CMIS_DIAG_MEAS_RESET_ERR);
	if (ret < 0)
		return ret;

	printf("BERT started: pattern=%s lanes=0x%02x bank=%d\n",
	       prbs_pattern_names[cfg->pattern], cfg->lanes, bank);
	printf("  Host gen=%s  Media chk=%s  Host chk=%s  Media gen=%s\n",
	       cfg->host_gen ? "on" : "off",
	       cfg->media_chk ? "on" : "off",
	       cfg->host_chk ? "on" : "off",
	       cfg->media_gen ? "on" : "off");
	return 0;
}

int cmis_diag_bert_stop(struct cmd_context *ctx, int bank)
{
	int ret;

	/* Stop measurement */
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_MEAS_CONTROL, 0x00);
	if (ret < 0)
		return ret;

	/* Disable all generators and checkers */
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_HOST_GEN_ENABLE, 0);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_MEDIA_GEN_ENABLE, 0);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_HOST_CHK_ENABLE, 0);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13, CMIS_DIAG_MEDIA_CHK_ENABLE, 0);
	if (ret < 0)
		return ret;

	printf("BERT stopped (bank %d)\n", bank);
	return 0;
}

/*
 * Helper: set the DiagnosticsSelector on Page 14h and re-read the data area.
 * Returns pointer to a 128-byte page buffer (caller indexes at 0xC0+).
 */
static const __u8 *
diag_select_and_read(struct cmd_context *ctx, int bank, __u8 selector)
{
	struct module_eeprom request;
	int ret;

	/* Write selector */
	ret = diag_write_byte(ctx, bank, 0x14, CMIS_DIAG_SELECTOR, selector);
	if (ret < 0)
		return NULL;

	/* Short delay for module to update data area */
	usleep(10000);

	/* Read the full page to get data at 0xC0-0xFF */
	cmis_request_init(&request, bank, 0x14, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return NULL;

	return request.data - CMIS_PAGE_SIZE;
}

int cmis_diag_bert_read(struct cmd_context *ctx, int bank)
{
	const __u8 *page;
	int i;

	if (is_json_context())
		open_json_object("bert_results");

	/* 1. Real-time BER (selector 0x01) */
	page = diag_select_and_read(ctx, bank, CMIS_DIAG_SEL_BER_RT);
	if (!page) {
		fprintf(stderr, "Error: cannot read BER data\n");
		if (is_json_context())
			close_json_object();
		return -1;
	}

	if (is_json_context()) {
		open_json_array("ber_per_lane", NULL);
	} else {
		printf("BERT Results (bank %d):\n", bank);
		printf("  Real-time BER per lane:\n");
		printf("    %-6s %-14s %-14s\n",
		       "Lane", "Host BER", "Media BER");
		printf("    %-6s %-14s %-14s\n",
		       "----", "--------------", "--------------");
	}

	for (i = 0; i < 8; i++) {
		int h_off = CMIS_DIAG_DATA_START + i * 2;
		int m_off = CMIS_DIAG_DATA_START + 16 + i * 2;
		double h_ber = f16_to_double(
			OFFSET_TO_U16_PTR(page, h_off));
		double m_ber = f16_to_double(
			OFFSET_TO_U16_PTR(page, m_off));

		if (is_json_context()) {
			open_json_object(NULL);
			print_uint(PRINT_JSON, "lane", "%u", i + 1);
			print_float(PRINT_JSON, "host_ber", "%.3e", h_ber);
			print_float(PRINT_JSON, "media_ber", "%.3e", m_ber);
			close_json_object();
		} else {
			printf("    %-6d %-14.3e %-14.3e\n",
			       i + 1, h_ber, m_ber);
		}
	}

	if (is_json_context())
		close_json_array(NULL);

	/* 2. Host error count (selector 0x02) */
	page = diag_select_and_read(ctx, bank, CMIS_DIAG_SEL_ERR_CNT_H);
	if (!page)
		goto out;

	if (is_json_context()) {
		open_json_array("host_error_count", NULL);
	} else {
		printf("\n  Host error/bit counts:\n");
		printf("    %-6s %-20s", "Lane", "Errors");
	}

	/* 3. Host bit count (selector 0x03) — read after displaying errors */
	{
		__u64 host_err[8];

		for (i = 0; i < 8; i++) {
			int off = CMIS_DIAG_DATA_START + i * 8;

			host_err[i] = OFFSET_TO_U64_PTR(page, off);
		}

		page = diag_select_and_read(ctx, bank,
					    CMIS_DIAG_SEL_BITS_CNT_H);

		if (!is_json_context())
			printf("%-20s\n    %-6s %-20s %-20s\n",
			       "Bits",
			       "----", "--------------------",
			       "--------------------");

		for (i = 0; i < 8; i++) {
			__u64 bits = 0;

			if (page) {
				int off = CMIS_DIAG_DATA_START + i * 8;

				bits = OFFSET_TO_U64_PTR(page, off);
			}

			if (is_json_context()) {
				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u", i + 1);
				print_lluint(PRINT_JSON, "errors",
					     "%llu",
					     (unsigned long long)host_err[i]);
				print_lluint(PRINT_JSON, "bits",
					     "%llu",
					     (unsigned long long)bits);
				close_json_object();
			} else {
				printf("    %-6d %-20llu %-20llu\n",
				       i + 1,
				       (unsigned long long)host_err[i],
				       (unsigned long long)bits);
			}
		}

		if (is_json_context())
			close_json_array(NULL);
	}

	/* 4. Media error count (selector 0x04) */
	page = diag_select_and_read(ctx, bank, CMIS_DIAG_SEL_ERR_CNT_M);
	if (!page)
		goto out;

	if (is_json_context()) {
		open_json_array("media_error_count", NULL);
	} else {
		printf("\n  Media error/bit counts:\n");
		printf("    %-6s %-20s", "Lane", "Errors");
	}

	/* 5. Media bit count (selector 0x05) */
	{
		__u64 media_err[8];

		for (i = 0; i < 8; i++) {
			int off = CMIS_DIAG_DATA_START + i * 8;

			media_err[i] = OFFSET_TO_U64_PTR(page, off);
		}

		page = diag_select_and_read(ctx, bank,
					    CMIS_DIAG_SEL_BITS_CNT_M);

		if (!is_json_context())
			printf("%-20s\n    %-6s %-20s %-20s\n",
			       "Bits",
			       "----", "--------------------",
			       "--------------------");

		for (i = 0; i < 8; i++) {
			__u64 bits = 0;

			if (page) {
				int off = CMIS_DIAG_DATA_START + i * 8;

				bits = OFFSET_TO_U64_PTR(page, off);
			}

			if (is_json_context()) {
				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u", i + 1);
				print_lluint(PRINT_JSON, "errors",
					     "%llu",
					     (unsigned long long)media_err[i]);
				print_lluint(PRINT_JSON, "bits",
					     "%llu",
					     (unsigned long long)bits);
				close_json_object();
			} else {
				printf("    %-6d %-20llu %-20llu\n",
				       i + 1,
				       (unsigned long long)media_err[i],
				       (unsigned long long)bits);
			}
		}

		if (is_json_context())
			close_json_array(NULL);
	}

out:
	if (is_json_context())
		close_json_object();
	return 0;
}

/*----------------------------------------------------------------------
 * Loopback control functions
 *----------------------------------------------------------------------*/

static const struct {
	const char *name;
	__u8 cap_bit;
	__u8 ctl_offset;
} loopback_modes[] = {
	{ "host-input",    CMIS_DIAG_LOOPBACK_HOST_IN,   CMIS_DIAG_LOOPBACK_HOST_IN_CTL },
	{ "host-output",   CMIS_DIAG_LOOPBACK_HOST_OUT,  CMIS_DIAG_LOOPBACK_HOST_OUT_CTL },
	{ "media-input",   CMIS_DIAG_LOOPBACK_MEDIA_IN,  CMIS_DIAG_LOOPBACK_MEDIA_IN_CTL },
	{ "media-output",  CMIS_DIAG_LOOPBACK_MEDIA_OUT, CMIS_DIAG_LOOPBACK_MEDIA_OUT_CTL },
};

int cmis_diag_loopback_start(struct cmd_context *ctx, int bank,
			     const char *mode, __u8 lanes)
{
	struct module_eeprom request;
	__u8 caps;
	int ret, i, idx = -1;

	for (i = 0; i < (int)(sizeof(loopback_modes) / sizeof(loopback_modes[0])); i++) {
		if (strcasecmp(mode, loopback_modes[i].name) == 0) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		fprintf(stderr, "Error: unknown loopback mode '%s'\n", mode);
		fprintf(stderr, "Valid modes: host-input, host-output, "
			"media-input, media-output\n");
		return -1;
	}

	/* Read loopback capability byte (Page 13h byte 128) */
	cmis_request_init(&request, bank, 0x13, CMIS_DIAG_LOOPBACK_CAPS);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read loopback capabilities\n");
		return -1;
	}
	caps = request.data[0];

	if (!(caps & loopback_modes[idx].cap_bit))
		fprintf(stderr, "Warning: module does not advertise %s "
			"loopback (caps=0x%02x), writing anyway\n",
			mode, caps);

	/* Write lane bitmask to the control register */
	ret = diag_write_byte(ctx, bank, 0x13,
			      loopback_modes[idx].ctl_offset, lanes);
	if (ret < 0) {
		fprintf(stderr, "Error: failed to write loopback control\n");
		return -1;
	}

	printf("Loopback %s started: lanes=0x%02x bank=%d\n",
	       mode, lanes, bank);
	return 0;
}

int cmis_diag_loopback_stop(struct cmd_context *ctx, int bank)
{
	int ret;

	ret = diag_write_byte(ctx, bank, 0x13,
			      CMIS_DIAG_LOOPBACK_MEDIA_OUT_CTL, 0x00);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13,
			      CMIS_DIAG_LOOPBACK_MEDIA_IN_CTL, 0x00);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13,
			      CMIS_DIAG_LOOPBACK_HOST_OUT_CTL, 0x00);
	if (ret < 0)
		return ret;
	ret = diag_write_byte(ctx, bank, 0x13,
			      CMIS_DIAG_LOOPBACK_HOST_IN_CTL, 0x00);
	if (ret < 0)
		return ret;

	printf("Loopback stopped (bank %d)\n", bank);
	return 0;
}

/*----------------------------------------------------------------------
 * Diagnostics Masks, Scratchpad, and User Pattern
 *----------------------------------------------------------------------*/

int cmis_diag_set_mask(struct cmd_context *ctx, int bank,
		       __u8 offset, __u8 lane_mask, bool enable)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, 0x13, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	old = request.data[0];
	buf = enable ? (old | lane_mask) : (old & ~lane_mask);

	cmis_request_init(&request, bank, 0x13, offset);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	if (is_json_context()) {
		char name[48];

		snprintf(name, sizeof(name), "DiagMask[0x%02x]", offset);
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", name);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  DiagMask[0x%02x]: 0x%02x -> 0x%02x\n",
		       offset, old, buf);
	}
	return 0;
}

int cmis_diag_scratchpad_read(struct cmd_context *ctx, int bank, __u8 *buf)
{
	struct module_eeprom request;
	int ret;

	cmis_request_init(&request, bank, 0x13, CMIS_DIAG_SCRATCHPAD_OFFSET);
	request.length = CMIS_DIAG_SCRATCHPAD_SIZE;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;

	memcpy(buf, request.data, CMIS_DIAG_SCRATCHPAD_SIZE);
	return 0;
}

int cmis_diag_scratchpad_write(struct cmd_context *ctx, int bank,
			       const __u8 *data, int len)
{
	struct module_eeprom request;
	__u8 buf[CMIS_DIAG_SCRATCHPAD_SIZE];

	if (len < 1 || len > CMIS_DIAG_SCRATCHPAD_SIZE)
		return -1;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, data, len);

	cmis_request_init(&request, bank, 0x13, CMIS_DIAG_SCRATCHPAD_OFFSET);
	request.length = CMIS_DIAG_SCRATCHPAD_SIZE;
	request.data = buf;
	return set_eeprom_page(ctx, &request);
}

int cmis_diag_user_pattern_write(struct cmd_context *ctx, int bank,
				 const __u8 *data, int len)
{
	struct module_eeprom request;
	__u8 buf[CMIS_DIAG_USER_PATTERN_SIZE];

	if (len < 1 || len > CMIS_DIAG_USER_PATTERN_SIZE)
		return -1;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, data, len);

	cmis_request_init(&request, bank, 0x13,
			  CMIS_DIAG_USER_PATTERN_OFFSET);
	request.length = CMIS_DIAG_USER_PATTERN_SIZE;
	request.data = buf;
	return set_eeprom_page(ctx, &request);
}
