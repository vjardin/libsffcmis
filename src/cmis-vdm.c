/**
 * cmis-vdm.c: VDM (Versatile Diagnostics Monitoring) Pages 20h-2Fh
 *
 * Real-time instantaneous readings of coherent link parameters
 * (CD, OSNR, eSNR, CFO, etc.) updated every ~1 second.
 *
 * Per OIF-CMIS-05.3 Section 8.22 and OIF-C-CMIS-01.3 Table 8.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-vdm.h"

/* Page convenience macros for struct cmis_memory_map */
#define page_01h upper_memory[0x0][0x1]

/* VDM observable data format */
enum vdm_format {
	VDM_FMT_U16,
	VDM_FMT_S16,
	VDM_FMT_F16,
};

/* Descriptor for one VDM observable type */
struct vdm_obs_type {
	const char *name;
	const char *json_name;
	const char *unit;
	enum vdm_format fmt;
	double scale;
};

/*
 * IEEE 754 half-precision (binary16) to double.
 * 1 sign + 5 exponent + 10 mantissa.
 */
static double f16_to_double(__u16 raw)
{
	int sign = (raw >> 15) & 1;
	int exp = (raw >> 10) & 0x1F;
	int mant = raw & 0x3FF;
	double val;

	if (exp == 0) {
		/* Subnormal or zero */
		val = ldexp((double)mant, -24);
	} else if (exp == 31) {
		/* Inf or NaN */
		if (mant == 0)
			return sign ? -INFINITY : INFINITY;
		return NAN;
	} else {
		/* Normal */
		val = ldexp((double)(mant + 1024), exp - 25);
	}

	return sign ? -val : val;
}

/*
 * Observable type table.
 *
 * Standard CMIS IDs 1-34 (Table 8-170, OIF-CMIS-05.3)
 * Coherent C-CMIS IDs 128-152 (Table 8, OIF-C-CMIS-01.3)
 *
 * Indexed by observable type ID.  ID 0 = unused/end marker.
 * IDs not in the table are displayed as "Unknown (ID N)".
 */
static const struct vdm_obs_type vdm_std_types[] = {
	[1]  = { "Laser Age",              "laser_age",
		 "%",    VDM_FMT_U16, 1.0 },
	[2]  = { "TEC Current",            "tec_current",
		 "mA",   VDM_FMT_S16, 0.1 },
	[3]  = { "Laser Freq Error",       "laser_freq_error",
		 "MHz",  VDM_FMT_S16, 0.1 },
	[4]  = { "Tx Power (high)",        "tx_power_high",
		 "uW",   VDM_FMT_U16, 0.1 },
	[5]  = { "Tx Power (low)",         "tx_power_low",
		 "W",    VDM_FMT_F16, 1.0 },
	[6]  = { "Bias Current Tx (high)", "bias_current_tx_high",
		 "uA",   VDM_FMT_U16, 1.0 },
	[7]  = { "Bias Current Tx (low)",  "bias_current_tx_low",
		 "A",    VDM_FMT_F16, 1.0 },
	[8]  = { "Bias Current Rx (high)", "bias_current_rx_high",
		 "uA",   VDM_FMT_U16, 1.0 },
	[9]  = { "Bias Current Rx (low)",  "bias_current_rx_low",
		 "A",    VDM_FMT_F16, 1.0 },
	[10] = { "Tx Power (OMA high)",    "tx_power_oma_high",
		 "uW",   VDM_FMT_U16, 0.1 },
	[11] = { "Tx Power (OMA low)",     "tx_power_oma_low",
		 "W",    VDM_FMT_F16, 1.0 },
	[12] = { "Rx Power (high)",        "rx_power_high",
		 "uW",   VDM_FMT_U16, 0.1 },
	[13] = { "Rx Power (low)",         "rx_power_low",
		 "W",    VDM_FMT_F16, 1.0 },
	[14] = { "Media BER (pre-FEC)",    "media_ber_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[15] = { "Media BER (errored)",    "media_ber_errored",
		 "",     VDM_FMT_F16, 1.0 },
	[16] = { "PAM4 Level 0 mUA",      "pam4_level0_mua",
		 "mV",   VDM_FMT_U16, 0.1 },
	[17] = { "PAM4 Level 1 mUA",      "pam4_level1_mua",
		 "mV",   VDM_FMT_U16, 0.1 },
	[18] = { "PAM4 Level 2 mUA",      "pam4_level2_mua",
		 "mV",   VDM_FMT_U16, 0.1 },
	[19] = { "PAM4 Level 3 mUA",      "pam4_level3_mua",
		 "mV",   VDM_FMT_U16, 0.1 },
	[20] = { "PAM4 MPI",              "pam4_mpi",
		 "dB",   VDM_FMT_S16, 0.1 },
	[21] = { "PAM4 Level Transition", "pam4_level_transition",
		 "",     VDM_FMT_U16, 1.0 },
	[22] = { "Host BER (pre-FEC)",     "host_ber_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[23] = { "Host BER (errored)",     "host_ber_errored",
		 "",     VDM_FMT_F16, 1.0 },
	[24] = { "Media BER (min pre-FEC)","media_ber_min_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[25] = { "Media BER (max pre-FEC)","media_ber_max_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[26] = { "Host BER (min pre-FEC)", "host_ber_min_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[27] = { "Host BER (max pre-FEC)", "host_ber_max_pre_fec",
		 "",     VDM_FMT_F16, 1.0 },
	[28] = { "Media BER (avg)",        "media_ber_avg",
		 "",     VDM_FMT_F16, 1.0 },
	[29] = { "SNR Media (I)",          "snr_media_i",
		 "dB",   VDM_FMT_U16, 0.1 },
	[30] = { "SNR Media (Q)",          "snr_media_q",
		 "dB",   VDM_FMT_U16, 0.1 },
	[31] = { "SNR Media (min)",        "snr_media_min",
		 "dB",   VDM_FMT_U16, 0.1 },
	[32] = { "SNR Media (max)",        "snr_media_max",
		 "dB",   VDM_FMT_U16, 0.1 },
	[33] = { "SNR Host (I)",           "snr_host_i",
		 "dB",   VDM_FMT_U16, 0.1 },
	[34] = { "SNR Host (Q)",           "snr_host_q",
		 "dB",   VDM_FMT_U16, 0.1 },
};

#define VDM_STD_MAX ARRAY_SIZE(vdm_std_types)

/* Coherent C-CMIS types (IDs 128-152) */
static const struct vdm_obs_type vdm_coherent_types[] = {
	[0]  = { "Modulator Bias X/I",     "mod_bias_xi",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[1]  = { "Modulator Bias X/Q",     "mod_bias_xq",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[2]  = { "Modulator Bias Y/I",     "mod_bias_yi",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[3]  = { "Modulator Bias Y/Q",     "mod_bias_yq",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[4]  = { "Modulator Bias X Phase", "mod_bias_x_phase",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[5]  = { "Modulator Bias Y Phase", "mod_bias_y_phase",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[6]  = { "CD (high gran)",         "cd_high_gran",
		 "ps/nm", VDM_FMT_S16, 1.0 },
	[7]  = { "CD (low gran)",          "cd_low_gran",
		 "ps/nm", VDM_FMT_S16, 20.0 },
	[8]  = { "DGD",                    "dgd",
		 "ps",   VDM_FMT_U16, 0.01 },
	[9]  = { "SOPMD (high gran)",      "sopmd_high_gran",
		 "ps^2", VDM_FMT_U16, 0.01 },
	[10] = { "PDL",                    "pdl",
		 "dB",   VDM_FMT_U16, 0.1 },
	[11] = { "OSNR",                   "osnr",
		 "dB",   VDM_FMT_U16, 0.1 },
	[12] = { "eSNR",                   "esnr",
		 "dB",   VDM_FMT_U16, 0.1 },
	[13] = { "CFO",                    "cfo",
		 "MHz",  VDM_FMT_S16, 1.0 },
	[14] = { "EVM",                    "evm",
		 "%",    VDM_FMT_U16, 100.0 / 65535.0 },
	[15] = { "Tx Power",              "tx_power",
		 "dBm",  VDM_FMT_S16, 0.01 },
	[16] = { "Rx Total Power",        "rx_total_power",
		 "dBm",  VDM_FMT_S16, 0.01 },
	[17] = { "Rx Signal Power",       "rx_signal_power",
		 "dBm",  VDM_FMT_S16, 0.01 },
	[18] = { "SOP ROC",               "sop_roc",
		 "krad/s", VDM_FMT_U16, 1.0 },
	[19] = { "MER",                    "mer",
		 "dB",   VDM_FMT_U16, 0.1 },
	[20] = { "Clock Recovery BW",     "clock_recovery_bw",
		 "%",    VDM_FMT_S16, 100.0 / 32767.0 },
	[21] = { "SOPMD (low gran)",       "sopmd_low_gran",
		 "ps^2", VDM_FMT_U16, 1.0 },
	[22] = { "SNR Margin",            "snr_margin",
		 "dB",   VDM_FMT_S16, 0.1 },
	[23] = { "Q-factor",              "q_factor",
		 "dB",   VDM_FMT_U16, 0.1 },
	[24] = { "Q-margin",              "q_margin",
		 "dB",   VDM_FMT_S16, 0.1 },
};

#define VDM_COHERENT_BASE	128
#define VDM_COHERENT_MAX	ARRAY_SIZE(vdm_coherent_types)

/* Look up an observable type by ID */
static const struct vdm_obs_type *vdm_type_lookup(__u8 type_id)
{
	if (type_id == 0)
		return NULL;

	if (type_id < VDM_STD_MAX && vdm_std_types[type_id].name)
		return &vdm_std_types[type_id];

	if (type_id >= VDM_COHERENT_BASE &&
	    (unsigned int)(type_id - VDM_COHERENT_BASE) < VDM_COHERENT_MAX &&
	    vdm_coherent_types[type_id - VDM_COHERENT_BASE].name)
		return &vdm_coherent_types[type_id - VDM_COHERENT_BASE];

	return NULL;
}

/* Decode a 2-byte VDM sample to double using the given format and scale */
static double vdm_decode_sample(__u16 raw, enum vdm_format fmt, double scale)
{
	switch (fmt) {
	case VDM_FMT_U16:
		return (double)raw * scale;
	case VDM_FMT_S16:
		return (double)(__s16)raw * scale;
	case VDM_FMT_F16:
		return f16_to_double(raw) * scale;
	}
	return 0.0;
}

/* Build 4-character flag string: H=high alarm, h=high warn, L=low alarm, l=low warn */
static void vdm_flag_str(__u8 nibble, char *out)
{
	out[0] = (nibble & 0x08) ? 'H' : '-';
	out[1] = (nibble & 0x02) ? 'h' : '-';
	out[2] = (nibble & 0x04) ? 'L' : '-';
	out[3] = (nibble & 0x01) ? 'l' : '-';
	out[4] = '\0';
}

/* Get the 4-bit flag nibble for a given VDM instance from Page 2Ch */
static __u8 vdm_get_flags(const __u8 *flags_page, int instance)
{
	__u8 byte;

	if (!flags_page)
		return 0;

	byte = flags_page[0x80 + instance / 2];
	if (instance % 2 == 0)
		return (byte >> 4) & 0x0F;
	return byte & 0x0F;
}

/* Decode one threshold set (8 bytes: HiAlm, LoAlm, HiWrn, LoWrn) */
static void vdm_get_thresholds(const __u8 *thresh_page, int set_id,
			       enum vdm_format fmt, double scale,
			       double *hi_alarm, double *lo_alarm,
			       double *hi_warn, double *lo_warn)
{
	int off;
	__u16 raw;

	if (!thresh_page || set_id < 0 || set_id > 15) {
		*hi_alarm = *lo_alarm = *hi_warn = *lo_warn = 0;
		return;
	}

	off = 0x80 + set_id * 8;

	raw = OFFSET_TO_U16_PTR(thresh_page, off);
	*hi_alarm = vdm_decode_sample(raw, fmt, scale);

	raw = OFFSET_TO_U16_PTR(thresh_page, off + 2);
	*lo_alarm = vdm_decode_sample(raw, fmt, scale);

	raw = OFFSET_TO_U16_PTR(thresh_page, off + 4);
	*hi_warn = vdm_decode_sample(raw, fmt, scale);

	raw = OFFSET_TO_U16_PTR(thresh_page, off + 6);
	*lo_warn = vdm_decode_sample(raw, fmt, scale);
}

/* Select printf format based on scale */
static const char *vdm_fmt(double scale)
{
	if (scale >= 1.0)
		return "%.0f";
	if (scale >= 0.01)
		return "%.2f";
	return "%.4f";
}

/* VDM freeze/unfreeze poll parameters */
#define VDM_FREEZE_POLL_RETRIES		10
#define VDM_FREEZE_POLL_INTERVAL_US	10000	/* 10 ms */

int cmis_vdm_freeze(struct cmd_context *ctx)
{
	struct module_eeprom request;
	__u8 buf[1];
	int i, ret;

	/* Read current value of Page 2Fh:0x90 */
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Set FreezeRequest (bit 7) */
	buf[0] = request.data[0] | CMIS_VDM_FREEZE_REQUEST;
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Poll FreezeDone (bit 6) */
	for (i = 0; i < VDM_FREEZE_POLL_RETRIES; i++) {
		cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
				  CMIS_VDM_FREEZE_OFFSET);
		request.length = 1;
		ret = get_eeprom_page(ctx, &request);
		if (ret < 0)
			return ret;
		if (request.data[0] & CMIS_VDM_FREEZE_DONE)
			return 0;
		usleep(VDM_FREEZE_POLL_INTERVAL_US);
	}

	return -ETIMEDOUT;
}

int cmis_vdm_unfreeze(struct cmd_context *ctx)
{
	struct module_eeprom request;
	__u8 buf[1];
	int i, ret;

	/* Read current value of Page 2Fh:0x90 */
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Clear FreezeRequest (bit 7) */
	buf[0] = request.data[0] & ~CMIS_VDM_FREEZE_REQUEST;
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	/* Poll UnfreezeDone (Page 2Fh:0x91 bit 7) */
	for (i = 0; i < VDM_FREEZE_POLL_RETRIES; i++) {
		cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
				  CMIS_VDM_UNFREEZE_OFFSET);
		request.length = 1;
		ret = get_eeprom_page(ctx, &request);
		if (ret < 0)
			return ret;
		if (request.data[0] & CMIS_VDM_UNFREEZE_DONE)
			return 0;
		usleep(VDM_FREEZE_POLL_INTERVAL_US);
	}

	return -ETIMEDOUT;
}

int cmis_vdm_set_power_saving(struct cmd_context *ctx, bool save)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	old = request.data[0];
	if (save)
		buf = old | CMIS_VDM_POWER_SAVING;
	else
		buf = old & ~CMIS_VDM_POWER_SAVING;

	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_VDM_FREEZE_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s",
			     "VDM_PowerSavingMode");
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		print_bool(PRINT_JSON, "power_saving", NULL, save);
		close_json_object();
	} else {
		printf("  VDM PowerSavingMode: 0x%02x -> 0x%02x (%s)\n",
		       old, buf,
		       save ? "on (monitoring off)" :
			      "off (monitoring on)");
	}
	return 0;
}

int cmis_vdm_set_mask(struct cmd_context *ctx, int instance,
		       __u8 mask_nibble)
{
	struct module_eeprom request;
	__u8 byte_off, buf, old, shift;
	int ret;

	if (instance < 0 || instance > 255 || mask_nibble > 0x0F)
		return -EINVAL;

	/* Page 2Dh: 2 instances per byte, 4 bits each.
	 * Even instances in low nibble, odd in high nibble.
	 */
	byte_off = 0x80 + instance / 2;
	shift = (instance & 1) ? 4 : 0;

	cmis_request_init(&request, 0, CMIS_VDM_MASKS_PAGE, byte_off);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	old = request.data[0];
	buf = (old & ~(0x0F << shift)) | ((mask_nibble & 0x0F) << shift);

	cmis_request_init(&request, 0, CMIS_VDM_MASKS_PAGE, byte_off);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "VDM_Mask");
		print_uint(PRINT_JSON, "instance", "%u", instance);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		print_uint(PRINT_JSON, "mask", "0x%x", mask_nibble);
		close_json_object();
	} else {
		printf("  VDM mask instance %d: 0x%02x -> 0x%02x (mask=0x%x)\n",
		       instance, old, buf, mask_nibble);
	}
	return 0;
}

int cmis_vdm_get_pam4_data(const struct cmis_memory_map *map,
			   struct vdm_pam4_lane *lanes, int max_lanes)
{
	const __u8 *page_2f;
	int num_groups, g, inst, n, count;
	bool level_found[CMIS_VDM_MAX_LANES][4];

	memset(lanes, 0, max_lanes * sizeof(*lanes));
	memset(level_found, 0, sizeof(level_found));

	for (n = 0; n < max_lanes && n < CMIS_VDM_MAX_LANES; n++)
		lanes[n].lane = n;

	if (!map->page_01h)
		return 0;

	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_VDM))
		return 0;

	page_2f = map->upper_memory[0][CMIS_VDM_ADVER_PAGE];
	if (!page_2f)
		return 0;

	num_groups = (page_2f[CMIS_VDM_SUPPORT_OFFSET] &
		      CMIS_VDM_SUPPORT_MASK) + 1;
	if (num_groups > CMIS_VDM_MAX_GROUPS)
		num_groups = CMIS_VDM_MAX_GROUPS;

	for (g = 0; g < num_groups; g++) {
		const __u8 *desc_page;
		const __u8 *sample_page;

		desc_page = map->upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g];
		sample_page = map->upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g];
		if (!desc_page || !sample_page)
			continue;

		for (inst = 0; inst < CMIS_VDM_INSTANCES_PER_GROUP; inst++) {
			int desc_off = 0x80 + inst * 2;
			int sample_off = 0x80 + inst * 2;
			__u8 type_id, lane_dp;
			__u16 raw_sample;
			const struct vdm_obs_type *obs;
			double value;
			int li;

			type_id = desc_page[desc_off + 1];
			if (type_id == 0)
				continue;

			lane_dp = desc_page[desc_off] & 0x0F;
			li = lane_dp;
			if (li >= max_lanes || li >= CMIS_VDM_MAX_LANES)
				continue;

			obs = vdm_type_lookup(type_id);
			if (!obs)
				continue;

			raw_sample = OFFSET_TO_U16_PTR(sample_page, sample_off);
			value = vdm_decode_sample(raw_sample, obs->fmt,
						  obs->scale);

			switch (type_id) {
			case 16: /* PAM4 Level 0 */
				lanes[li].level[0] = value;
				level_found[li][0] = true;
				break;
			case 17: /* PAM4 Level 1 */
				lanes[li].level[1] = value;
				level_found[li][1] = true;
				break;
			case 18: /* PAM4 Level 2 */
				lanes[li].level[2] = value;
				level_found[li][2] = true;
				break;
			case 19: /* PAM4 Level 3 */
				lanes[li].level[3] = value;
				level_found[li][3] = true;
				break;
			case 31: /* SNR Media (min) — best single metric */
				lanes[li].snr_db = value;
				lanes[li].has_snr = true;
				break;
			case 29: /* SNR Media (I) — fallback if no min */
				if (!lanes[li].has_snr) {
					lanes[li].snr_db = value;
					lanes[li].has_snr = true;
				}
				break;
			case 30: /* SNR Media (Q) — fallback if no min/I */
				if (!lanes[li].has_snr) {
					lanes[li].snr_db = value;
					lanes[li].has_snr = true;
				}
				break;
			case 14: /* Media BER (pre-FEC) */
				lanes[li].ber = value;
				lanes[li].has_ber = true;
				break;
			case 20: /* PAM4 MPI */
				lanes[li].mpi_db = value;
				lanes[li].has_mpi = true;
				break;
			}
		}
	}

	/* Mark valid lanes and compact to front.
	 * Require all 4 levels found AND nonzero span (L3 != L0) —
	 * a module with no signal reports all levels as 0 mV.
	 */
	count = 0;
	for (n = 0; n < max_lanes && n < CMIS_VDM_MAX_LANES; n++) {
		if (level_found[n][0] && level_found[n][1] &&
		    level_found[n][2] && level_found[n][3] &&
		    lanes[n].level[3] != lanes[n].level[0]) {
			lanes[n].valid = true;
			if (count != n)
				lanes[count] = lanes[n];
			count++;
		}
	}

	return count;
}

int cmis_vdm_get_coherent_data(const struct cmis_memory_map *map,
			       struct vdm_coherent_lane *lanes,
			       int max_lanes)
{
	const __u8 *page_2f;
	int num_groups, g, inst, n, count;

	memset(lanes, 0, max_lanes * sizeof(*lanes));

	for (n = 0; n < max_lanes && n < CMIS_VDM_MAX_LANES; n++)
		lanes[n].lane = n;

	if (!map->page_01h)
		return 0;

	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_VDM))
		return 0;

	page_2f = map->upper_memory[0][CMIS_VDM_ADVER_PAGE];
	if (!page_2f)
		return 0;

	num_groups = (page_2f[CMIS_VDM_SUPPORT_OFFSET] &
		      CMIS_VDM_SUPPORT_MASK) + 1;
	if (num_groups > CMIS_VDM_MAX_GROUPS)
		num_groups = CMIS_VDM_MAX_GROUPS;

	for (g = 0; g < num_groups; g++) {
		const __u8 *desc_page;
		const __u8 *sample_page;

		desc_page = map->upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g];
		sample_page = map->upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g];
		if (!desc_page || !sample_page)
			continue;

		for (inst = 0; inst < CMIS_VDM_INSTANCES_PER_GROUP; inst++) {
			int desc_off = 0x80 + inst * 2;
			int sample_off = 0x80 + inst * 2;
			__u8 type_id, lane_dp;
			__u16 raw_sample;
			const struct vdm_obs_type *obs;
			double value;
			int li;

			type_id = desc_page[desc_off + 1];
			if (type_id == 0)
				continue;

			lane_dp = desc_page[desc_off] & 0x0F;
			li = lane_dp;
			if (li >= max_lanes || li >= CMIS_VDM_MAX_LANES)
				continue;

			obs = vdm_type_lookup(type_id);
			if (!obs)
				continue;

			raw_sample = OFFSET_TO_U16_PTR(sample_page, sample_off);
			value = vdm_decode_sample(raw_sample, obs->fmt,
						  obs->scale);

			/* C-CMIS coherent type IDs (128-based) */
			switch (type_id) {
			case 142: /* EVM (C-CMIS ID 14, base 128) */
				lanes[li].evm_pct = value;
				lanes[li].has_evm = true;
				break;
			case 140: /* eSNR (C-CMIS ID 12) */
				lanes[li].esnr_db = value;
				lanes[li].has_esnr = true;
				break;
			case 139: /* OSNR (C-CMIS ID 11) */
				lanes[li].osnr_db = value;
				lanes[li].has_osnr = true;
				break;
			case 150: /* SNR Margin (C-CMIS ID 22) */
				lanes[li].snr_margin_db = value;
				lanes[li].has_snr_margin = true;
				break;
			case 151: /* Q-factor (C-CMIS ID 23) */
				lanes[li].q_factor_db = value;
				lanes[li].has_q_factor = true;
				break;
			}
		}
	}

	/* Mark valid lanes and compact to front.
	 * Require EVM > 0 or eSNR > 0 — a module with no signal
	 * reports all-zero values even though the descriptors exist.
	 */
	count = 0;
	for (n = 0; n < max_lanes && n < CMIS_VDM_MAX_LANES; n++) {
		bool evm_ok = lanes[n].has_evm && lanes[n].evm_pct > 0.0;
		bool esnr_ok = lanes[n].has_esnr && lanes[n].esnr_db > 0.0;

		if (evm_ok || esnr_ok) {
			lanes[n].valid = true;
			if (count != n)
				lanes[count] = lanes[n];
			count++;
		}
	}

	return count;
}

void cmis_show_vdm(const struct cmis_memory_map *map)
{
	const __u8 *page_2f;
	const __u8 *flags_page;
	const __u8 *masks_page;
	int num_groups, total_instances;
	__u16 fine_interval;
	int g, inst;

	if (!map->page_01h)
		return;

	/* Check VDM advertisement (01h:142 bit 6) */
	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_VDM))
		return;

	/* Page 2Fh: advertisement/control */
	page_2f = map->upper_memory[0][CMIS_VDM_ADVER_PAGE];
	if (!page_2f)
		return;

	num_groups = (page_2f[CMIS_VDM_SUPPORT_OFFSET] &
		      CMIS_VDM_SUPPORT_MASK) + 1;
	if (num_groups > CMIS_VDM_MAX_GROUPS)
		num_groups = CMIS_VDM_MAX_GROUPS;
	total_instances = num_groups * CMIS_VDM_INSTANCES_PER_GROUP;

	fine_interval = OFFSET_TO_U16_PTR(page_2f,
					   CMIS_VDM_FINE_INTERVAL_MSB);

	/* Flag page (Page 2Ch), shared across all groups */
	flags_page = map->upper_memory[0][CMIS_VDM_FLAGS_PAGE];

	/* Mask page (Page 2Dh), same layout as flags */
	masks_page = map->upper_memory[0][CMIS_VDM_MASKS_PAGE];

	if (is_json_context()) {
		open_json_object("vdm_monitors");
		print_bool(PRINT_JSON, "freeze_active", NULL,
			   !!(page_2f[CMIS_VDM_FREEZE_OFFSET] &
			      CMIS_VDM_FREEZE_REQUEST));
		print_bool(PRINT_JSON, "power_saving", NULL,
			   !!(page_2f[CMIS_VDM_FREEZE_OFFSET] &
			      CMIS_VDM_POWER_SAVING));
		print_uint(PRINT_JSON, "groups", "%u", num_groups);
		print_uint(PRINT_JSON, "total_instances", "%u",
			   total_instances);
		if (fine_interval)
			print_float(PRINT_JSON, "fine_interval_ms", "%.1f",
				    (double)fine_interval * 0.1);
		open_json_array("instances", NULL);
	} else {
		printf("\t%-41s :\n",
		       "VDM Real-Time Monitors (Pages 20h-2Fh)");
		printf("\t  %-39s : %d (%d instances)\n",
		       "Groups", num_groups, total_instances);
		if (fine_interval)
			printf("\t  %-39s : %.1f ms\n",
			       "Fine interval",
			       (double)fine_interval * 0.1);
		printf("\t  %4s  %4s  %-26s %11s %-8s %9s %9s %9s %9s %-5s %s\n",
		       "Inst", "Lane", "Observable",
		       "Value", "Unit",
		       "HiAlm", "HiWrn", "LoWrn", "LoAlm",
		       "Flags", "Mask");
		printf("\t  %4s  %4s  %-26s %11s %-8s %9s %9s %9s %9s %-5s %s\n",
		       "----", "----", "--------------------------",
		       "-----------", "--------",
		       "---------", "---------", "---------", "---------",
		       "-----", "----");
	}

	for (g = 0; g < num_groups; g++) {
		const __u8 *desc_page;
		const __u8 *sample_page;
		const __u8 *thresh_page;

		desc_page = map->upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g];
		sample_page = map->upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g];
		thresh_page = map->upper_memory[0][CMIS_VDM_THRESH_PAGE_BASE + g];

		if (!desc_page || !sample_page)
			continue;

		for (inst = 0; inst < CMIS_VDM_INSTANCES_PER_GROUP; inst++) {
			int global_inst = g * CMIS_VDM_INSTANCES_PER_GROUP + inst;
			int desc_off = 0x80 + inst * 2;
			int sample_off = 0x80 + inst * 2;
			__u8 desc_byte0, type_id, lane_dp, thresh_set_id;
			__u16 raw_sample;
			const struct vdm_obs_type *obs;
			double value;
			double hi_alarm, lo_alarm, hi_warn, lo_warn;
			int has_thresh;
			char flags[5], mask[5];
			__u8 flag_nibble, mask_nibble;
			char unknown_name[32];

			desc_byte0 = desc_page[desc_off];
			type_id = desc_page[desc_off + 1];

			/* Type ID 0 = unused instance */
			if (type_id == 0)
				continue;

			lane_dp = desc_byte0 & 0x0F;
			thresh_set_id = (desc_byte0 >> 4) & 0x0F;
			raw_sample = OFFSET_TO_U16_PTR(sample_page, sample_off);

			obs = vdm_type_lookup(type_id);

			flag_nibble = vdm_get_flags(flags_page, global_inst);
			vdm_flag_str(flag_nibble, flags);

			mask_nibble = vdm_get_flags(masks_page, global_inst);
			vdm_flag_str(mask_nibble, mask);

			if (obs) {
				value = vdm_decode_sample(raw_sample,
							  obs->fmt,
							  obs->scale);
			} else {
				/* Unknown type — display raw U16 */
				value = (double)raw_sample;
			}

			if (!obs)
				snprintf(unknown_name, sizeof(unknown_name),
					 "Unknown (ID %u)", type_id);

			has_thresh = thresh_page && obs;
			if (has_thresh)
				vdm_get_thresholds(thresh_page, thresh_set_id,
						   obs->fmt, obs->scale,
						   &hi_alarm, &lo_alarm,
						   &hi_warn, &lo_warn);

			if (is_json_context()) {
				const char *name = obs ? obs->json_name :
						   "unknown";

				open_json_object(NULL);
				print_uint(PRINT_JSON, "instance", "%u",
					   global_inst + 1);
				print_uint(PRINT_JSON, "lane", "%u",
					   lane_dp);
				print_string(PRINT_JSON, "observable",
					     "%s", name);
				print_uint(PRINT_JSON, "type_id", "%u",
					   type_id);
				if (obs) {
					print_float(PRINT_JSON, "value",
						    vdm_fmt(obs->scale),
						    value);
					print_string(PRINT_JSON, "unit",
						     "%s", obs->unit);
				} else {
					print_uint(PRINT_JSON, "raw_value",
						   "%u", raw_sample);
				}
				if (has_thresh) {
					open_json_object("thresholds");
					print_float(PRINT_JSON,
						    "high_alarm",
						    vdm_fmt(obs->scale),
						    hi_alarm);
					print_float(PRINT_JSON,
						    "low_alarm",
						    vdm_fmt(obs->scale),
						    lo_alarm);
					print_float(PRINT_JSON,
						    "high_warning",
						    vdm_fmt(obs->scale),
						    hi_warn);
					print_float(PRINT_JSON,
						    "low_warning",
						    vdm_fmt(obs->scale),
						    lo_warn);
					close_json_object();
				}
				print_string(PRINT_JSON, "flags", "%s",
					     flags);
				print_string(PRINT_JSON, "mask", "%s",
					     mask);
				close_json_object();
			} else {
				const char *name = obs ? obs->name :
						   unknown_name;
				const char *unit = obs ? obs->unit : "";
				const char *f;
				char vbuf[20];

				if (obs) {
					f = vdm_fmt(obs->scale);
					if (obs->fmt == VDM_FMT_F16)
						f = "%.4e";

					snprintf(vbuf, sizeof(vbuf), f, value);
					printf("\t  %4d  %4u  %-26s %11s %-8s",
					       global_inst + 1,
					       lane_dp, name,
					       vbuf, unit);

					if (has_thresh) {
						snprintf(vbuf, sizeof(vbuf), f, hi_alarm);
						printf(" %9s", vbuf);
						snprintf(vbuf, sizeof(vbuf), f, hi_warn);
						printf(" %9s", vbuf);
						snprintf(vbuf, sizeof(vbuf), f, lo_warn);
						printf(" %9s", vbuf);
						snprintf(vbuf, sizeof(vbuf), f, lo_alarm);
						printf(" %9s", vbuf);
					} else {
						printf(" %9s %9s %9s %9s",
						       "-", "-", "-", "-");
					}
					printf(" %-5s %s\n", flags, mask);
				} else {
					printf("\t  %4d  %4u  %-26s %11u %-8s %9s %9s %9s %9s %-5s %s\n",
					       global_inst + 1, lane_dp,
					       name, raw_sample,
					       unit,
					       "-", "-", "-", "-",
					       flags, mask);
				}
			}
		}
	}

	if (is_json_context()) {
		close_json_array(NULL);
		close_json_object();
	}
}
