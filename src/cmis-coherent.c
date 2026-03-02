/**
 * cmis-coherent.c: C-CMIS Coherent Link Performance Monitoring
 *
 * Implements display of coherent PM metrics (Page 35h),
 * FEC performance counters (Page 34h), C-CMIS revision (Page 40h),
 * and PM advertisement (Page 42h) per OIF-C-CMIS-01.3.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "internal.h"
#include "json_print.h"
#include "sff-common.h"
#include "sff-common-ext.h"
#include "module-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-coherent.h"

/* Page convenience macros for struct cmis_memory_map */
#define page_01h upper_memory[0x0][0x1]
#define page_33h upper_memory[0x0][0x33]
#define page_34h upper_memory[0x0][0x34]
#define page_35h upper_memory[0x0][0x35]
#define page_3Bh upper_memory[0x0][0x3B]
#define page_40h upper_memory[0x0][0x40]
#define page_42h upper_memory[0x0][0x42]
#define page_44h upper_memory[0x0][0x44]

/* Descriptor for one Page 35h PM field */
struct ccmis_pm_field {
	const char *name;
	const char *json_name;
	const char *unit;
	__u8 offset;       /* page offset of avg field */
	__u8 adver_byte;   /* byte index (0-12) in Page 42h bitmap */
	__u8 adver_bit;    /* bit position for "avg" within that byte */
	__u8 size;         /* element size: 2=U16/S16, 4=S32 */
	bool is_signed;
	double scale;      /* multiply raw value by this */
};

static const struct ccmis_pm_field ccmis_pm_fields[] = {
	{ "Chromatic Dispersion", "chromatic_dispersion", "ps/nm",
	  CCMIS_PM_CD_OFFSET,       2, 6, 4, true,  1.0 },
	{ "DGD", "dgd", "ps",
	  CCMIS_PM_DGD_OFFSET,      2, 2, 2, false, 0.01 },
	{ "HG-SOPMD", "hg_sopmd", "ps^2",
	  CCMIS_PM_HGSOPMD_OFFSET,  10, 2, 2, false, 0.01 },
	{ "PDL", "pdl", "dB",
	  CCMIS_PM_PDL_OFFSET,      3, 2, 2, false, 0.1 },
	{ "OSNR", "osnr", "dB",
	  CCMIS_PM_OSNR_OFFSET,     4, 6, 2, false, 0.1 },
	{ "eSNR", "esnr", "dB",
	  CCMIS_PM_ESNR_OFFSET,     4, 2, 2, false, 0.1 },
	{ "CFO", "cfo", "MHz",
	  CCMIS_PM_CFO_OFFSET,      5, 6, 2, true,  1.0 },
	{ "EVM", "evm", "%",
	  CCMIS_PM_EVM_OFFSET,      5, 2, 2, false, 100.0/65535.0 },
	{ "Tx Power", "tx_power", "dBm",
	  CCMIS_PM_TXPOWER_OFFSET,  6, 2, 2, true,  0.01 },
	{ "Rx Total Power", "rx_total_power", "dBm",
	  CCMIS_PM_RXPOWER_OFFSET,  7, 6, 2, true,  0.01 },
	{ "Rx Signal Power", "rx_signal_power", "dBm",
	  CCMIS_PM_RXSIGPOWER_OFFSET, 7, 2, 2, true, 0.01 },
	{ "SOP-ROC", "sop_roc", "krad/s",
	  CCMIS_PM_SOPROC_OFFSET,   6, 6, 2, false, 1.0 },
	{ "MER", "mer", "dB",
	  CCMIS_PM_MER_OFFSET,      11, 2, 2, false, 0.1 },
	{ "Clock Recovery", "clock_recovery", "%",
	  CCMIS_PM_CLOCKREC_OFFSET, 10, 6, 2, true,  100.0/32767.0 },
	{ "LG-SOPMD", "lg_sopmd", "ps^2",
	  CCMIS_PM_LGSOPMD_OFFSET,  3, 6, 2, false, 1.0 },
	{ "SNR Margin", "snr_margin", "dB",
	  CCMIS_PM_SNRMARGIN_OFFSET, 11, 6, 2, true, 0.1 },
	{ "Q-Factor", "q_factor", "dB",
	  CCMIS_PM_QFACTOR_OFFSET,  12, 6, 2, false, 0.1 },
	{ "Q-Margin", "q_margin", "dB",
	  CCMIS_PM_QMARGIN_OFFSET,  12, 2, 2, true,  0.1 },
};

#define NUM_CCMIS_PM_FIELDS ARRAY_SIZE(ccmis_pm_fields)

/* Read a single PM raw value from page data at the given offset+size */
static double
ccmis_pm_read_value(const __u8 *page, __u8 offset, __u8 size,
		    bool is_signed, double scale)
{
	if (size == 4) {
		__s32 raw = OFFSET_TO_S32_PTR(page, offset);
		return (double)raw * scale;
	}
	if (is_signed) {
		__s16 raw = OFFSET_TO_S16_PTR(page, offset);
		return (double)raw * scale;
	}
	__u16 raw = OFFSET_TO_U16_PTR(page, offset);
	return (double)raw * scale;
}

/* Select printf format based on scale */
static const char *ccmis_pm_fmt(double scale)
{
	if (scale >= 1.0)
		return "%.0f";
	if (scale >= 0.01)
		return "%.2f";
	return "%.4f";
}

/* Show Page 35h coherent PM metrics */
static void cmis_show_coherent_pm_fields(const struct cmis_memory_map *map)
{
	const __u8 *page_35 = map->page_35h;
	const __u8 *page_42 = map->page_42h;
	__u8 pm_adver[CCMIS_PM_ADVER_LEN];
	unsigned int i;

	if (!page_35)
		return;

	if (page_42)
		memcpy(pm_adver, &page_42[CCMIS_PM_ADVER_OFFSET],
		       CCMIS_PM_ADVER_LEN);
	else
		memset(pm_adver, 0xFF, CCMIS_PM_ADVER_LEN);

	if (is_json_context()) {
		open_json_object("coherent_pm_metrics");
	} else {
		printf("\t%-41s :\n",
		       "Coherent PM (Page 35h)");
		printf("\t  %-26s %12s %12s %12s  %s\n",
		       "", "Avg", "Min", "Max", "Unit");
	}

	for (i = 0; i < NUM_CCMIS_PM_FIELDS; i++) {
		const struct ccmis_pm_field *f = &ccmis_pm_fields[i];
		double avg, min, max;
		__u8 triplet_size;
		bool advertised;

		/* Check PM advertisement bit for avg */
		advertised = (f->adver_byte < CCMIS_PM_ADVER_LEN &&
			      (pm_adver[f->adver_byte] &
			       (1 << f->adver_bit)));

		if (!advertised) {
			if (is_json_context()) {
				open_json_object(f->json_name);
				print_string(PRINT_JSON, "avg", "%s", "N/A");
				print_string(PRINT_JSON, "min", "%s", "N/A");
				print_string(PRINT_JSON, "max", "%s", "N/A");
				print_string(PRINT_JSON, "unit", "%s", f->unit);
				close_json_object();
			} else {
				printf("\t  %-26s %12s %12s %12s  %s\n",
				       f->name, "N/A", "N/A", "N/A", f->unit);
			}
			continue;
		}

		triplet_size = f->size;
		avg = ccmis_pm_read_value(page_35, f->offset,
					  triplet_size, f->is_signed, f->scale);
		min = ccmis_pm_read_value(page_35, f->offset + triplet_size,
					  triplet_size, f->is_signed, f->scale);
		max = ccmis_pm_read_value(page_35, f->offset + 2 * triplet_size,
					  triplet_size, f->is_signed, f->scale);

		if (is_json_context()) {
			const char *fmt = ccmis_pm_fmt(f->scale);

			open_json_object(f->json_name);
			print_float(PRINT_JSON, "avg", fmt, avg);
			print_float(PRINT_JSON, "min", fmt, min);
			print_float(PRINT_JSON, "max", fmt, max);
			print_string(PRINT_JSON, "unit", "%s", f->unit);
			close_json_object();
		} else {
			if (f->scale >= 1.0)
				printf("\t  %-26s %12.0f %12.0f %12.0f  %s\n",
				       f->name, avg, min, max, f->unit);
			else if (f->scale >= 0.01)
				printf("\t  %-26s %12.2f %12.2f %12.2f  %s\n",
				       f->name, avg, min, max, f->unit);
			else
				printf("\t  %-26s %12.4f %12.4f %12.4f  %s\n",
				       f->name, avg, min, max, f->unit);
		}
	}

	if (is_json_context())
		close_json_object();
}

/* Show Page 34h FEC performance counters */
static void cmis_show_coherent_fec(const struct cmis_memory_map *map)
{
	const __u8 *page_34 = map->page_34h;
	__u64 val64;
	__u32 val32;

	if (!page_34)
		return;

	if (is_json_context()) {
		open_json_object("coherent_fec_counters");
	} else {
		printf("\t%-41s :\n",
		       "Media FEC Performance (Page 34h)");
	}

	/* U64 counters */
	val64 = OFFSET_TO_U64_PTR(page_34, CCMIS_FEC_RXBITS_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "rx_bits_pm",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Rx Bits (PM interval)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_34, CCMIS_FEC_RXBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "rx_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Rx Bits (sub-interval)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_34, CCMIS_FEC_RXCORRBITS_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "rx_corrected_bits_pm",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Rx Corrected Bits (PM)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_34, CCMIS_FEC_RXMINCORRBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "rx_min_corrected_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Rx Min Corrected Bits (sub-int)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_34, CCMIS_FEC_RXMAXCORRBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "rx_max_corrected_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Rx Max Corrected Bits (sub-int)",
		       (unsigned long long)val64);

	/* U32 counters */
	val32 = OFFSET_TO_U32_PTR(page_34, CCMIS_FEC_RXFRAMES_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "rx_frames_pm", "%u", val32);
	else
		printf("\t  %-39s : %u\n", "Rx Frames (PM interval)", val32);

	val32 = OFFSET_TO_U32_PTR(page_34, CCMIS_FEC_RXFRAMES_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "rx_frames_subint", "%u", val32);
	else
		printf("\t  %-39s : %u\n", "Rx Frames (sub-interval)", val32);

	val32 = OFFSET_TO_U32_PTR(page_34, CCMIS_FEC_RXUNCORRERR_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "rx_uncorrectable_frames_pm",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Rx Uncorrectable Err Frames (PM)", val32);

	val32 = OFFSET_TO_U32_PTR(page_34, CCMIS_FEC_RXMINUNCORRERR_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "rx_min_uncorrectable_frames_subint",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Rx Min Uncorr Err Frames (sub-int)", val32);

	val32 = OFFSET_TO_U32_PTR(page_34, CCMIS_FEC_RXMAXUNCORRERR_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "rx_max_uncorrectable_frames_subint",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Rx Max Uncorr Err Frames (sub-int)", val32);

	if (is_json_context())
		close_json_object();
}

/* Show Page 3Ah Host FEC performance counters */
static void cmis_show_host_fec(const struct cmis_memory_map *map)
{
	const __u8 *page_3a = map->upper_memory[0][CCMIS_HOST_FEC_PAGE];
	__u64 val64;
	__u32 val32;

	if (!page_3a)
		return;

	if (is_json_context()) {
		open_json_object("host_fec_counters");
	} else {
		printf("\t%-41s :\n",
		       "Host FEC Performance (Page 3Ah)");
	}

	/* U64 counters */
	val64 = OFFSET_TO_U64_PTR(page_3a, CCMIS_HOST_FEC_TXBITS_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "tx_bits_pm",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Tx Bits (PM interval)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_3a, CCMIS_HOST_FEC_TXBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "tx_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Tx Bits (sub-interval)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_3a, CCMIS_HOST_FEC_TXCORRBITS_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "tx_corrected_bits_pm",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Tx Corrected Bits (PM)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_3a, CCMIS_HOST_FEC_TXMINCORRBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "tx_min_corrected_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Tx Min Corrected Bits (sub-int)",
		       (unsigned long long)val64);

	val64 = OFFSET_TO_U64_PTR(page_3a, CCMIS_HOST_FEC_TXMAXCORRBITS_SUBINT_OFFSET);
	if (is_json_context())
		print_lluint(PRINT_JSON, "tx_max_corrected_bits_subint",
			     "%llu", (unsigned long long)val64);
	else
		printf("\t  %-39s : %llu\n", "Tx Max Corrected Bits (sub-int)",
		       (unsigned long long)val64);

	/* U32 counters */
	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXFRAMES_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "tx_frames_pm", "%u", val32);
	else
		printf("\t  %-39s : %u\n", "Tx Frames (PM interval)", val32);

	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXFRAMES_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "tx_frames_subint", "%u", val32);
	else
		printf("\t  %-39s : %u\n", "Tx Frames (sub-interval)", val32);

	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXUNCORRERR_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "tx_uncorrectable_frames_pm",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Tx Uncorrectable Err Frames (PM)", val32);

	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXMINUNCORRERR_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "tx_min_uncorrectable_frames_subint",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Tx Min Uncorr Err Frames (sub-int)", val32);

	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXMAXUNCORRERR_SUBINT_OFFSET);
	if (is_json_context())
		print_uint(PRINT_JSON, "tx_max_uncorrectable_frames_subint",
			   "%u", val32);
	else
		printf("\t  %-39s : %u\n",
		       "Tx Max Uncorr Err Frames (sub-int)", val32);

	/* C-CMIS 1.3+ extra fields */
	val32 = OFFSET_TO_U32_PTR(page_3a, CCMIS_HOST_FEC_TXCORRFRAMES_OFFSET);
	if (val32 != 0 || OFFSET_TO_U32_PTR(page_3a,
	    CCMIS_HOST_FEC_TXCORRFRAMES_SUBINT_OFFSET) != 0) {
		if (is_json_context())
			print_uint(PRINT_JSON, "tx_corrected_frames_pm",
				   "%u", val32);
		else
			printf("\t  %-39s : %u\n",
			       "Tx Corrected Frames (PM)", val32);

		val32 = OFFSET_TO_U32_PTR(page_3a,
			CCMIS_HOST_FEC_TXCORRFRAMES_SUBINT_OFFSET);
		if (is_json_context())
			print_uint(PRINT_JSON,
				   "tx_corrected_frames_subint",
				   "%u", val32);
		else
			printf("\t  %-39s : %u\n",
			       "Tx Corrected Frames (sub-int)", val32);
	}

	if (is_json_context())
		close_json_object();
}

/* IEEE 754 half-precision float to double */
static double f16_to_double(__u16 raw)
{
	int sign = (raw >> 15) & 1;
	int exp = (raw >> 10) & 0x1F;
	int mant = raw & 0x3FF;
	double val;

	if (exp == 0) {
		val = ldexp((double)mant, -24);
	} else if (exp == 31) {
		if (mant == 0)
			return sign ? -INFINITY : INFINITY;
		return NAN;
	} else {
		val = ldexp((double)(mant + 1024), exp - 25);
	}

	return sign ? -val : val;
}

static const char *ccmis_rx_los_type_str(__u8 type)
{
	switch (type) {
	case 0:
		return "Total Power";
	case 1:
		return "Signal Power";
	case 2:
		return "Total + Signal Power";
	default:
		return "Reserved";
	}
}

/* Show Page 41h Rx signal power advertisement */
static void cmis_show_rx_power_adver(const struct cmis_memory_map *map)
{
	const __u8 *page_41 = map->upper_memory[0][CCMIS_RX_PWR_ADVER_PAGE];
	__u8 impl;
	__u8 los_type;

	if (!page_41)
		return;

	impl = page_41[CCMIS_RX_PWR_IMPL_OFFSET];
	los_type = (impl & CCMIS_RX_LOS_TYPE_MASK) >> CCMIS_RX_LOS_TYPE_SHIFT;

	if (is_json_context()) {
		open_json_object("rx_power_advertisement");
		print_bool(PRINT_JSON, "total_power_threshold_cfg",
			   NULL, !!(impl & CCMIS_RX_PWR_IMPL_TOTAL));
		print_bool(PRINT_JSON, "signal_power_threshold_cfg",
			   NULL, !!(impl & CCMIS_RX_PWR_IMPL_SIGNAL));
		print_string(PRINT_JSON, "rx_los_type", "%s",
			     ccmis_rx_los_type_str(los_type));

		if (impl & CCMIS_RX_PWR_IMPL_TOTAL) {
			print_float(PRINT_JSON, "total_pwr_hi_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_TOTAL_HI_ALARM) * 0.01);
			print_float(PRINT_JSON, "total_pwr_lo_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_TOTAL_LO_ALARM) * 0.01);
			print_float(PRINT_JSON, "total_pwr_hi_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_TOTAL_HI_WARN) * 0.01);
			print_float(PRINT_JSON, "total_pwr_lo_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_TOTAL_LO_WARN) * 0.01);
		}
		if (impl & CCMIS_RX_PWR_IMPL_SIGNAL) {
			print_float(PRINT_JSON, "sig_pwr_hi_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_SIG_HI_ALARM) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_lo_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_SIG_LO_ALARM) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_hi_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_SIG_HI_WARN) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_lo_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_41,
				    CCMIS_RX_PWR_SIG_LO_WARN) * 0.01);
		}
		close_json_object();
	} else {
		printf("\t%-41s :\n",
		       "C-CMIS Media Lane Capabilities (Page 41h)");
		printf("\t  %-39s : %s\n",
		       "Rx Total Power threshold config",
		       (impl & CCMIS_RX_PWR_IMPL_TOTAL) ? "Yes" : "No");
		printf("\t  %-39s : %s\n",
		       "Rx Signal Power threshold config",
		       (impl & CCMIS_RX_PWR_IMPL_SIGNAL) ? "Yes" : "No");
		printf("\t  %-39s : %s\n",
		       "Rx LOS type",
		       ccmis_rx_los_type_str(los_type));

		if (impl & CCMIS_RX_PWR_IMPL_TOTAL) {
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Hi Alarm range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_TOTAL_HI_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Lo Alarm range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_TOTAL_LO_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Hi Warn range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_TOTAL_HI_WARN) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Lo Warn range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_TOTAL_LO_WARN) * 0.01);
		}
		if (impl & CCMIS_RX_PWR_IMPL_SIGNAL) {
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Hi Alarm range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_SIG_HI_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Lo Alarm range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_SIG_LO_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Hi Warn range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_SIG_HI_WARN) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Lo Warn range",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_41,
			       CCMIS_RX_PWR_SIG_LO_WARN) * 0.01);
		}
	}
}

/* Show Page 43h provisioning advertisement */
static void cmis_show_prov_adver(const struct cmis_memory_map *map)
{
	const __u8 *page_43 = map->upper_memory[0][CCMIS_PROV_ADVER_PAGE];
	__u8 adver;

	if (!page_43)
		return;

	adver = page_43[CCMIS_PROV_ADVER_OFFSET];

	if (is_json_context()) {
		open_json_object("provisioning_advertisement");
		print_bool(PRINT_JSON, "tx_filter_control",
			   NULL, !!(adver & CCMIS_PROV_ADVER_TX_FILTER));
		print_bool(PRINT_JSON, "lf_insertion_on_ld",
			   NULL, !!(adver & CCMIS_PROV_ADVER_LF_INSERTION));
		close_json_object();
	} else {
		printf("\t%-41s :\n",
		       "C-CMIS Provisioning Adver (Page 43h)");
		printf("\t  %-39s : %s\n",
		       "Tx filter control",
		       (adver & CCMIS_PROV_ADVER_TX_FILTER) ?
		       "Supported" : "Not supported");
		printf("\t  %-39s : %s\n",
		       "LF insertion on LD enable",
		       (adver & CCMIS_PROV_ADVER_LF_INSERTION) ?
		       "Supported" : "Not supported");
	}
}

/* Show Page 30h coherent configurable thresholds */
static void cmis_show_coherent_thresholds(const struct cmis_memory_map *map)
{
	const __u8 *page_30;
	int bank, num_banks;

	num_banks = 0;
	for (bank = 0; bank < CMIS_MAX_BANKS; bank++) {
		if (map->upper_memory[bank][CCMIS_THRESH_PAGE])
			num_banks = bank + 1;
	}
	if (num_banks == 0)
		return;

	if (is_json_context()) {
		open_json_object("coherent_thresholds");
	} else {
		printf("\t%-41s :\n",
		       "Coherent Thresholds (Page 30h)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		page_30 = map->upper_memory[bank][CCMIS_THRESH_PAGE];
		if (!page_30)
			continue;

		if (is_json_context()) {
			char key[32];
			__u8 use_cfg, fec_en;

			snprintf(key, sizeof(key), "bank_%d", bank);
			open_json_object(key);

			print_float(PRINT_JSON, "total_pwr_hi_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_TOTAL_PWR_HI_ALARM) * 0.01);
			print_float(PRINT_JSON, "total_pwr_lo_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_TOTAL_PWR_LO_ALARM) * 0.01);
			print_float(PRINT_JSON, "total_pwr_hi_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_TOTAL_PWR_HI_WARN) * 0.01);
			print_float(PRINT_JSON, "total_pwr_lo_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_TOTAL_PWR_LO_WARN) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_hi_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_SIG_PWR_HI_ALARM) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_lo_alarm_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_SIG_PWR_LO_ALARM) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_hi_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_SIG_PWR_HI_WARN) * 0.01);
			print_float(PRINT_JSON, "sig_pwr_lo_warn_dbm",
				    "%.2f",
				    (double)(__s16)OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_SIG_PWR_LO_WARN) * 0.01);

			use_cfg = page_30[CCMIS_THRESH_USE_CFG_OFFSET];
			print_bool(PRINT_JSON, "use_cfg_total_pwr", NULL,
				   !!(use_cfg & CCMIS_THRESH_USE_CFG_TOTAL_PWR));
			print_bool(PRINT_JSON, "use_cfg_sig_pwr", NULL,
				   !!(use_cfg & CCMIS_THRESH_USE_CFG_SIG_PWR));

			print_float(PRINT_JSON, "fdd_raise_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_FDD_RAISE)));
			print_float(PRINT_JSON, "fdd_clear_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_FDD_CLEAR)));
			print_float(PRINT_JSON, "fed_raise_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_FED_RAISE)));
			print_float(PRINT_JSON, "fed_clear_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_30,
				    CCMIS_THRESH_FED_CLEAR)));

			fec_en = page_30[CCMIS_THRESH_FEC_ENABLE_OFFSET];
			print_bool(PRINT_JSON, "fed_enable", NULL,
				   !!(fec_en & CCMIS_THRESH_FED_ENABLE));
			print_bool(PRINT_JSON, "fdd_enable", NULL,
				   !!(fec_en & CCMIS_THRESH_FDD_ENABLE));

			close_json_object();
		} else {
			__u8 use_cfg, fec_en;

			if (bank > 0)
				printf("\t  (Bank %d)\n", bank);

			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Hi Alarm",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_TOTAL_PWR_HI_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Lo Alarm",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_TOTAL_PWR_LO_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Hi Warn",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_TOTAL_PWR_HI_WARN) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Total Power Lo Warn",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_TOTAL_PWR_LO_WARN) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Hi Alarm",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_SIG_PWR_HI_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Lo Alarm",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_SIG_PWR_LO_ALARM) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Hi Warn",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_SIG_PWR_HI_WARN) * 0.01);
			printf("\t  %-39s : %.2f dBm\n",
			       "Rx Signal Power Lo Warn",
			       (double)(__s16)OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_SIG_PWR_LO_WARN) * 0.01);

			use_cfg = page_30[CCMIS_THRESH_USE_CFG_OFFSET];
			printf("\t  %-39s : %s\n",
			       "Use configured Total Power thresholds",
			       (use_cfg & CCMIS_THRESH_USE_CFG_TOTAL_PWR) ?
			       "Yes" : "No");
			printf("\t  %-39s : %s\n",
			       "Use configured Signal Power thresholds",
			       (use_cfg & CCMIS_THRESH_USE_CFG_SIG_PWR) ?
			       "Yes" : "No");

			printf("\t  %-39s : %e\n",
			       "FDD Raise BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_FDD_RAISE)));
			printf("\t  %-39s : %e\n",
			       "FDD Clear BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_FDD_CLEAR)));
			printf("\t  %-39s : %e\n",
			       "FED Raise BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_FED_RAISE)));
			printf("\t  %-39s : %e\n",
			       "FED Clear BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_30,
			       CCMIS_THRESH_FED_CLEAR)));

			fec_en = page_30[CCMIS_THRESH_FEC_ENABLE_OFFSET];
			printf("\t  %-39s : %s\n",
			       "FED enable",
			       (fec_en & CCMIS_THRESH_FED_ENABLE) ?
			       "Yes" : "No");
			printf("\t  %-39s : %s\n",
			       "FDD enable",
			       (fec_en & CCMIS_THRESH_FDD_ENABLE) ?
			       "Yes" : "No");
		}
	}

	if (is_json_context())
		close_json_object();
}

static const char *ccmis_tx_filter_type_str(__u8 type)
{
	switch (type & 0x03) {
	case 0:
		return "None";
	case 1:
		return "Root Raised Cosine (RRC)";
	case 2:
		return "Raised Cosine (RC)";
	case 3:
		return "Gaussian";
	default:
		return "Unknown";
	}
}

/* Show Page 31h media lane provisioning */
static void cmis_show_media_prov(const struct cmis_memory_map *map)
{
	const __u8 *page_31;
	int bank, num_banks;

	num_banks = 0;
	for (bank = 0; bank < CMIS_MAX_BANKS; bank++) {
		if (map->upper_memory[bank][CCMIS_PROV_PAGE])
			num_banks = bank + 1;
	}
	if (num_banks == 0)
		return;

	if (is_json_context()) {
		open_json_object("media_lane_provisioning");
	} else {
		printf("\t%-41s :\n",
		       "Media Lane Provisioning (Page 31h)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		int lanes = cmis_lanes_per_bank(map, bank);
		int i;

		page_31 = map->upper_memory[bank][CCMIS_PROV_PAGE];
		if (!page_31)
			continue;

		if (is_json_context()) {
			char key[32];

			snprintf(key, sizeof(key), "bank_%d", bank);
			open_json_array(key, NULL);
			for (i = 0; i < lanes; i++) {
				__u8 filter_en = page_31[CCMIS_PROV_TX_FILTER_ENABLE];
				__u8 filter_type_byte;
				__u8 filter_type;
				__u8 lf_ins = page_31[CCMIS_PROV_LF_INSERTION];

				/* 2 bits per lane in filter type bytes */
				filter_type_byte = page_31[CCMIS_PROV_TX_FILTER_TYPE +
							   (i / 4)];
				filter_type = (filter_type_byte >>
					       (6 - 2 * (i % 4))) & 0x03;

				open_json_object(NULL);
				print_uint(PRINT_JSON, "lane", "%u",
					   bank * CMIS_CHANNELS_PER_BANK +
					   i + 1);
				print_bool(PRINT_JSON, "tx_filter_enable",
					   NULL,
					   !!(filter_en & (1 << i)));
				print_string(PRINT_JSON, "tx_filter_type",
					     "%s",
					     ccmis_tx_filter_type_str(filter_type));
				print_bool(PRINT_JSON, "lf_insertion_on_ld",
					   NULL,
					   !!(lf_ins & (1 << i)));
				close_json_object();
			}
			close_json_array(NULL);
		} else {
			if (bank > 0)
				printf("\t  (Bank %d)\n", bank);

			printf("\t  %-6s %-12s %-28s %-12s\n",
			       "Lane", "TxFilter", "FilterType", "LF Insert");
			printf("\t  %-6s %-12s %-28s %-12s\n",
			       "----", "------------", "----------------------------",
			       "------------");

			for (i = 0; i < lanes; i++) {
				int global_lane =
					bank * CMIS_CHANNELS_PER_BANK + i + 1;
				__u8 filter_en = page_31[CCMIS_PROV_TX_FILTER_ENABLE];
				__u8 filter_type_byte;
				__u8 filter_type;
				__u8 lf_ins = page_31[CCMIS_PROV_LF_INSERTION];

				filter_type_byte = page_31[CCMIS_PROV_TX_FILTER_TYPE +
							   (i / 4)];
				filter_type = (filter_type_byte >>
					       (6 - 2 * (i % 4))) & 0x03;

				printf("\t  %-6d %-12s %-28s %s\n",
				       global_lane,
				       (filter_en & (1 << i)) ? "On" : "Off",
				       ccmis_tx_filter_type_str(filter_type),
				       (lf_ins & (1 << i)) ? "On" : "Off");
			}
		}
	}

	if (is_json_context())
		close_json_object();
}

/* Show Page 38h host interface thresholds */
static void cmis_show_host_thresholds(const struct cmis_memory_map *map)
{
	int bank, num_banks;

	num_banks = 0;
	for (bank = 0; bank < CMIS_MAX_BANKS; bank++) {
		if (map->upper_memory[bank][CCMIS_HOST_THRESH_PAGE])
			num_banks = bank + 1;
	}
	if (num_banks == 0)
		return;

	if (is_json_context()) {
		open_json_object("host_thresholds");
	} else {
		printf("\t%-41s :\n",
		       "Host Interface Thresholds (Page 38h)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		const __u8 *page_38 =
			map->upper_memory[bank][CCMIS_HOST_THRESH_PAGE];
		__u8 enable_byte;

		if (!page_38)
			continue;

		enable_byte = page_38[CCMIS_HOST_THRESH_ENABLE_OFFSET];

		if (is_json_context()) {
			char key[32];

			snprintf(key, sizeof(key), "bank_%d", bank);
			open_json_object(key);

			print_float(PRINT_JSON, "host_fdd_raise_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_38,
				    CCMIS_HOST_THRESH_FDD_RAISE)));
			print_float(PRINT_JSON, "host_fdd_clear_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_38,
				    CCMIS_HOST_THRESH_FDD_CLEAR)));
			print_float(PRINT_JSON, "host_fed_raise_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_38,
				    CCMIS_HOST_THRESH_FED_RAISE)));
			print_float(PRINT_JSON, "host_fed_clear_ber", "%e",
				    f16_to_double(OFFSET_TO_U16_PTR(page_38,
				    CCMIS_HOST_THRESH_FED_CLEAR)));
			print_bool(PRINT_JSON, "host_fdd_enable", NULL,
				   !!(enable_byte &
				      CCMIS_HOST_THRESH_FDD_ENABLE));
			print_bool(PRINT_JSON, "host_fed_enable", NULL,
				   !!(enable_byte &
				      CCMIS_HOST_THRESH_FED_ENABLE));

			close_json_object();
		} else {
			if (bank > 0)
				printf("\t  (Bank %d)\n", bank);

			printf("\t  %-39s : %e\n",
			       "Host FDD Raise BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_38,
			       CCMIS_HOST_THRESH_FDD_RAISE)));
			printf("\t  %-39s : %e\n",
			       "Host FDD Clear BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_38,
			       CCMIS_HOST_THRESH_FDD_CLEAR)));
			printf("\t  %-39s : %e\n",
			       "Host FED Raise BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_38,
			       CCMIS_HOST_THRESH_FED_RAISE)));
			printf("\t  %-39s : %e\n",
			       "Host FED Clear BER threshold",
			       f16_to_double(OFFSET_TO_U16_PTR(page_38,
			       CCMIS_HOST_THRESH_FED_CLEAR)));
			printf("\t  %-39s : %s\n",
			       "Host FDD enable",
			       (enable_byte & CCMIS_HOST_THRESH_FDD_ENABLE) ?
			       "Yes" : "No");
			printf("\t  %-39s : %s\n",
			       "Host FED enable",
			       (enable_byte & CCMIS_HOST_THRESH_FED_ENABLE) ?
			       "Yes" : "No");
		}
	}

	if (is_json_context())
		close_json_object();
}

void cmis_show_coherent_pm(const struct cmis_memory_map *map)
{
	const __u8 *page_40;
	__u8 ccmis_rev;

	if (!map->page_01h)
		return;

	/* Check if C-CMIS coherent pages are advertised (01h:142 bit 4) */
	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_COHERENT))
		return;

	/* Show C-CMIS revision from Page 40h */
	page_40 = map->page_40h;
	if (page_40) {
		ccmis_rev = page_40[CCMIS_REV_OFFSET];
		if (is_json_context()) {
			open_json_object("ccmis_revision");
			print_uint(PRINT_JSON, "major", "%u",
				   (ccmis_rev >> 4) & 0x0F);
			print_uint(PRINT_JSON, "minor", "%u",
				   ccmis_rev & 0x0F);
			close_json_object();
		} else {
			printf("\t%-41s : %d.%d\n", "C-CMIS revision",
			       (ccmis_rev >> 4) & 0x0F, ccmis_rev & 0x0F);
		}
	}

	cmis_show_coherent_pm_fields(map);
	cmis_show_coherent_fec(map);
	cmis_show_host_fec(map);
	cmis_show_rx_power_adver(map);
	cmis_show_prov_adver(map);
	cmis_show_coherent_thresholds(map);
	cmis_show_media_prov(map);
	cmis_show_host_thresholds(map);
}

/*-----------------------------------------------------------------------
 * C-CMIS Coherent Flag Display (Pages 33h, 3Bh, 44h)
 * OIF-C-CMIS-01.3, Tables 13, 19, 25
 */

/* Descriptor for one coherent alarm flag */
struct ccmis_flag_field {
	const char *name;
	const char *json_name;
	__u8 flag_byte;    /* Byte offset in flag page (33h or 3Bh) */
	__u8 flag_bit;     /* Bit position within that byte */
	__u8 adver_byte;   /* Byte index (0-based) in Page 44h adver */
	__u8 adver_bit;    /* Bit position in advertisement byte */
};

/* Page 33h media lane flags — indexed by Page 44h bytes 0-3 */
static const struct ccmis_flag_field ccmis_media_flag_fields[] = {
	/* Byte 128 (44h adver byte 0) — Tx flags */
	{ "Tx Loss of Alignment",   "tx_loa",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_LOA_BIT,        0, 5 },
	{ "Tx Out of Alignment",    "tx_ooa",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_OOA_BIT,        0, 4 },
	{ "Tx CMU Loss of Lock",    "tx_lol_cmu",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_LOL_CMU_BIT,    0, 3 },
	{ "Tx Ref Clock LOL",       "tx_lol_refclk",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_LOL_REFCLK_BIT, 0, 2 },
	{ "Tx Deskew LOL",          "tx_lol_deskew",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_LOL_DESKEW_BIT, 0, 1 },
	{ "Tx FIFO Error",          "tx_fifo",
	  CCMIS_FLAGS_TX_OFFSET, CCMIS_FLAGS_TX_FIFO_BIT,       0, 0 },

	/* Byte 130 (44h adver byte 1) — Rx flags */
	{ "Rx Loss of Frame",       "rx_lof",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOF_BIT,        1, 7 },
	{ "Rx Loss of Multi Frame", "rx_lom",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOM_BIT,        1, 6 },
	{ "Rx Demodulator LOL",     "rx_lol_demod",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOL_DEMOD_BIT,  1, 5 },
	{ "Rx CD Compensation LOL", "rx_lol_cd",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOL_CD_BIT,     1, 4 },
	{ "Rx Loss of Alignment",   "rx_loa",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOA_BIT,        1, 3 },
	{ "Rx Out of Alignment",    "rx_ooa",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_OOA_BIT,        1, 2 },
	{ "Rx Deskew LOL",          "rx_lol_deskew",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOL_DESKEW_BIT, 1, 1 },
	{ "Rx FIFO Error",          "rx_lol_fifo",
	  CCMIS_FLAGS_RX_OFFSET, CCMIS_FLAGS_RX_LOL_FIFO_BIT,   1, 0 },

	/* Byte 131 (44h adver byte 2) — FEC flags */
	{ "FEC Excessive Degrade",  "rx_fed_pm",
	  CCMIS_FLAGS_RX_FEC_OFFSET, CCMIS_FLAGS_RX_FED_PM_BIT, 2, 1 },
	{ "FEC Detected Degrade",   "rx_fdd_pm",
	  CCMIS_FLAGS_RX_FEC_OFFSET, CCMIS_FLAGS_RX_FDD_PM_BIT, 2, 0 },

	/* Byte 132 (44h adver byte 3) — Degrade flags */
	{ "Remote Degrade",         "remote_degrade",
	  CCMIS_FLAGS_DEGRADE_OFFSET, CCMIS_FLAGS_RD_BIT,       3, 2 },
	{ "Local Degrade",          "local_degrade",
	  CCMIS_FLAGS_DEGRADE_OFFSET, CCMIS_FLAGS_LD_BIT,       3, 1 },
	{ "Remote PHY Fault",       "remote_phy_fault",
	  CCMIS_FLAGS_DEGRADE_OFFSET, CCMIS_FLAGS_RPF_BIT,      3, 0 },
};

#define NUM_CCMIS_MEDIA_FLAGS ARRAY_SIZE(ccmis_media_flag_fields)

/* Page 3Bh host data path flags — indexed by Page 44h bytes 4-8 */
static const struct ccmis_flag_field ccmis_host_flag_fields[] = {
	/* Byte 192 (44h adver byte 4) — Host FEC */
	{ "Host FEC Excessive Degrade", "host_fed_pm",
	  CCMIS_FLAGS_HOST_FEC_OFFSET, CCMIS_FLAGS_HOST_FED_PM_BIT, 4, 1 },
	{ "Host FEC Detected Degrade",  "host_fdd_pm",
	  CCMIS_FLAGS_HOST_FEC_OFFSET, CCMIS_FLAGS_HOST_FDD_PM_BIT, 4, 0 },

	/* Byte 193 (44h adver byte 5) — Host degrade */
	{ "Host Remote Degrade",  "host_rd",
	  CCMIS_FLAGS_HOST_DEGRADE_OFFSET, CCMIS_FLAGS_HOST_RD_BIT, 5, 1 },
	{ "Host Local Degrade",   "host_ld",
	  CCMIS_FLAGS_HOST_DEGRADE_OFFSET, CCMIS_FLAGS_HOST_LD_BIT, 5, 0 },

	/* Byte 194 (44h adver byte 6) — FlexE */
	{ "FlexE Remote PHY Fault",   "flexe_rpf",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_RPF_BIT,
	  6, 7 },
	{ "FlexE Group ID Mismatch",  "flexe_gid_mm",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_GIDMM_BIT,
	  6, 6 },
	{ "FlexE Instance Map Mismatch", "flexe_instmap_mm",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_INSTMAPMM_BIT,
	  6, 5 },
	{ "FlexE Calendar Mismatch", "flexe_cal_mm",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_CALMM_BIT,
	  6, 4 },
	{ "FlexE IID Mismatch",     "flexe_iid_mm",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_IIDMM_BIT,
	  6, 3 },
	{ "FlexE LOF",              "flexe_lof",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_LOF_BIT,
	  6, 2 },
	{ "FlexE LOM",              "flexe_lom",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_LOM_BIT,
	  6, 1 },
	{ "FlexE LOPB",             "flexe_lopb",
	  CCMIS_FLAGS_HOST_FLEXE_OFFSET, CCMIS_FLAGS_HOST_FLEXE_LOPB_BIT,
	  6, 0 },

	/* Byte 195 (44h adver byte 7) — Host Tx */
	{ "Host Tx LOA",  "host_tx_loa",
	  CCMIS_FLAGS_HOST_TX_OFFSET, CCMIS_FLAGS_HOST_TX_LOA_BIT, 7, 2 },
	{ "Host Tx RF",   "host_tx_rf",
	  CCMIS_FLAGS_HOST_TX_OFFSET, CCMIS_FLAGS_HOST_TX_RF_BIT,  7, 1 },
	{ "Host Tx LF",   "host_tx_lf",
	  CCMIS_FLAGS_HOST_TX_OFFSET, CCMIS_FLAGS_HOST_TX_LF_BIT,  7, 0 },

	/* Byte 196 (44h adver byte 8) — Host Rx */
	{ "Host Rx LOA",  "host_rx_loa",
	  CCMIS_FLAGS_HOST_RX_OFFSET, CCMIS_FLAGS_HOST_RX_LOA_BIT, 8, 2 },
	{ "Host Rx RF",   "host_rx_rf",
	  CCMIS_FLAGS_HOST_RX_OFFSET, CCMIS_FLAGS_HOST_RX_RF_BIT,  8, 1 },
	{ "Host Rx LF",   "host_rx_lf",
	  CCMIS_FLAGS_HOST_RX_OFFSET, CCMIS_FLAGS_HOST_RX_LF_BIT,  8, 0 },
};

#define NUM_CCMIS_HOST_FLAGS ARRAY_SIZE(ccmis_host_flag_fields)

/* Show Page 33h coherent media lane flags */
static void
cmis_show_coherent_media_flags(const struct cmis_memory_map *map,
			       const __u8 *alarm_adver)
{
	int bank, num_banks;
	unsigned int i;

	/* Determine number of active banks */
	num_banks = 0;
	for (bank = 0; bank < CMIS_MAX_BANKS; bank++) {
		if (map->upper_memory[bank][CCMIS_FLAGS_MEDIA_PAGE])
			num_banks = bank + 1;
	}
	if (num_banks == 0)
		return;

	if (is_json_context()) {
		open_json_object("coherent_media_flags");
	} else {
		printf("\t%-41s :\n",
		       "Coherent media flags (Page 33h, COR)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		const __u8 *page_33 =
			map->upper_memory[bank][CCMIS_FLAGS_MEDIA_PAGE];
		int lanes_in_bank;

		if (!page_33)
			continue;

		lanes_in_bank = cmis_lanes_per_bank(map, bank);

		for (i = 0; i < NUM_CCMIS_MEDIA_FLAGS; i++) {
			const struct ccmis_flag_field *f =
				&ccmis_media_flag_fields[i];
			bool advertised;
			bool value;
			int lane;

			advertised = (alarm_adver[f->adver_byte] &
				      (1 << f->adver_bit));
			if (!advertised)
				continue;

			value = !!(page_33[f->flag_byte] &
				   (1 << f->flag_bit));

			for (lane = 0; lane < lanes_in_bank; lane++) {
				int global_lane =
					bank * CMIS_CHANNELS_PER_BANK +
					lane + 1;

				if (is_json_context()) {
					char key[64];

					snprintf(key, sizeof(key),
						 "%s_lane%d",
						 f->json_name, global_lane);
					print_bool(PRINT_JSON, key,
						   "%s", value);
				} else {
					char label[64];

					snprintf(label, sizeof(label),
						 "%s (Lane %d)",
						 f->name, global_lane);
					printf("\t  %-39s : %s\n",
					       label,
					       value ? "On" : "Off");
				}
			}
		}
	}

	if (is_json_context())
		close_json_object();
}

/* Show Page 3Bh coherent host data path flags */
static void
cmis_show_coherent_host_flags(const struct cmis_memory_map *map,
			      const __u8 *alarm_adver)
{
	int bank, num_banks;
	unsigned int i;

	/* Determine number of active banks */
	num_banks = 0;
	for (bank = 0; bank < CMIS_MAX_BANKS; bank++) {
		if (map->upper_memory[bank][CCMIS_FLAGS_HOST_PAGE])
			num_banks = bank + 1;
	}
	if (num_banks == 0)
		return;

	if (is_json_context()) {
		open_json_object("coherent_host_flags");
	} else {
		printf("\t%-41s :\n",
		       "Coherent host flags (Page 3Bh, COR)");
	}

	for (bank = 0; bank < num_banks; bank++) {
		const __u8 *page_3B =
			map->upper_memory[bank][CCMIS_FLAGS_HOST_PAGE];
		int dp_num = bank + 1;

		if (!page_3B)
			continue;

		for (i = 0; i < NUM_CCMIS_HOST_FLAGS; i++) {
			const struct ccmis_flag_field *f =
				&ccmis_host_flag_fields[i];
			bool advertised;
			bool value;

			advertised = (alarm_adver[f->adver_byte] &
				      (1 << f->adver_bit));
			if (!advertised)
				continue;

			value = !!(page_3B[f->flag_byte] &
				   (1 << f->flag_bit));

			if (is_json_context()) {
				char key[64];

				snprintf(key, sizeof(key),
					 "%s_dp%d",
					 f->json_name, dp_num);
				print_bool(PRINT_JSON, key,
					   "%s", value);
			} else {
				char label[64];

				snprintf(label, sizeof(label),
					 "%s (DP %d)",
					 f->name, dp_num);
				printf("\t  %-39s : %s\n",
				       label,
				       value ? "On" : "Off");
			}
		}
	}

	if (is_json_context())
		close_json_object();
}

void cmis_show_coherent_flags(const struct cmis_memory_map *map)
{
	__u8 alarm_adver[CCMIS_ALARM_ADVER_LEN];

	if (!map->page_01h)
		return;

	/* Check if C-CMIS coherent pages are advertised (01h:142 bit 4) */
	if (!(map->page_01h[CMIS_PAGES_ADVER_OFFSET] &
	      CMIS_PAGES_ADVER_COHERENT))
		return;

	/* Load alarm advertisement from Page 44h, or assume all if absent */
	if (map->page_44h)
		memcpy(alarm_adver, &map->page_44h[CCMIS_ALARM_ADVER_OFFSET],
		       CCMIS_ALARM_ADVER_LEN);
	else
		memset(alarm_adver, 0xFF, CCMIS_ALARM_ADVER_LEN);

	cmis_show_coherent_media_flags(map, alarm_adver);
	cmis_show_coherent_host_flags(map, alarm_adver);
}

/*----------------------------------------------------------------------
 * Coherent Threshold Write Operations (Page 30h)
 *----------------------------------------------------------------------*/

/* IEEE 754 half-precision: double to F16 */
static __u16 double_to_f16(double val)
{
	int sign = 0;
	int exp;
	double frac;
	unsigned int mant;

	if (isnan(val))
		return 0x7E00;	/* NaN */

	if (val < 0.0) {
		sign = 1;
		val = -val;
	}

	if (isinf(val))
		return (sign << 15) | 0x7C00;

	if (val == 0.0)
		return (sign << 15);

	/* Decompose: val = frac * 2^exp, 0.5 <= frac < 1.0 */
	frac = frexp(val, &exp);

	/* F16: bias=15, stored exponent range 1..30 (denorm at 0, inf/nan at 31)
	 * Normal: value = 1.mant * 2^(exp-15)
	 * We have: frac * 2^exp = (2*frac) * 2^(exp-1) = (1 + 2*frac - 1) * 2^(exp-1)
	 * So biased_exp = exp - 1 + 15 = exp + 14
	 */
	exp += 14;

	if (exp >= 31) {
		/* Overflow -> infinity */
		return (sign << 15) | 0x7C00;
	}

	if (exp <= 0) {
		/* Denormal: value = 0.mant * 2^(-14) */
		int shift = 1 - exp;	/* how many extra bits to shift right */

		if (shift > 10)
			return (sign << 15);	/* underflow to zero */

		/* frac is in [0.5, 1.0), so 2*frac is in [1.0, 2.0) */
		mant = (unsigned int)(frac * (1 << (11 - shift)) + 0.5);
		if (mant > (unsigned int)(1 << (10 - shift)))
			mant = (1 << (10 - shift));
		return (sign << 15) | (mant & 0x3FF);
	}

	/* Normal: 2*frac is in [1.0, 2.0), subtract implicit 1 */
	mant = (unsigned int)((2.0 * frac - 1.0) * 1024.0 + 0.5);
	if (mant >= 1024) {
		mant = 0;
		exp++;
		if (exp >= 31)
			return (sign << 15) | 0x7C00;
	}

	return (sign << 15) | (exp << 10) | (mant & 0x3FF);
}

/* Threshold descriptor table */
static const struct {
	const char *name;
	__u8 offset;
	bool is_f16;  /* false = S16 * 0.01 dBm, true = F16 BER */
} thresh_descs[] = {
	{ "total-pwr-hi-alarm", CCMIS_THRESH_TOTAL_PWR_HI_ALARM, false },
	{ "total-pwr-lo-alarm", CCMIS_THRESH_TOTAL_PWR_LO_ALARM, false },
	{ "total-pwr-hi-warn",  CCMIS_THRESH_TOTAL_PWR_HI_WARN,  false },
	{ "total-pwr-lo-warn",  CCMIS_THRESH_TOTAL_PWR_LO_WARN,  false },
	{ "sig-pwr-hi-alarm",   CCMIS_THRESH_SIG_PWR_HI_ALARM,   false },
	{ "sig-pwr-lo-alarm",   CCMIS_THRESH_SIG_PWR_LO_ALARM,   false },
	{ "sig-pwr-hi-warn",    CCMIS_THRESH_SIG_PWR_HI_WARN,    false },
	{ "sig-pwr-lo-warn",    CCMIS_THRESH_SIG_PWR_LO_WARN,    false },
	{ "fdd-raise",          CCMIS_THRESH_FDD_RAISE,           true },
	{ "fdd-clear",          CCMIS_THRESH_FDD_CLEAR,           true },
	{ "fed-raise",          CCMIS_THRESH_FED_RAISE,           true },
	{ "fed-clear",          CCMIS_THRESH_FED_CLEAR,           true },
};

#define NUM_THRESH_DESCS ARRAY_SIZE(thresh_descs)

int cmis_coherent_set_threshold(struct cmd_context *ctx, int bank,
				const char *type, double value)
{
	struct module_eeprom request;
	__u8 buf[2];
	__u16 raw;
	unsigned int i;
	int ret;
	const char *found_name = NULL;
	__u8 offset = 0;
	bool is_f16 = false;

	for (i = 0; i < NUM_THRESH_DESCS; i++) {
		if (strcasecmp(type, thresh_descs[i].name) == 0) {
			found_name = thresh_descs[i].name;
			offset = thresh_descs[i].offset;
			is_f16 = thresh_descs[i].is_f16;
			break;
		}
	}

	if (!found_name) {
		fprintf(stderr, "Unknown threshold type: %s\n", type);
		fprintf(stderr, "Valid types:");
		for (i = 0; i < NUM_THRESH_DESCS; i++)
			fprintf(stderr, " %s", thresh_descs[i].name);
		fprintf(stderr, "\n");
		return -EINVAL;
	}

	if (is_f16) {
		raw = double_to_f16(value);
	} else {
		__s16 sraw = (__s16)(value * 100.0);

		raw = (__u16)sraw;
	}

	buf[0] = (raw >> 8) & 0xFF;
	buf[1] = raw & 0xFF;

	cmis_request_init(&request, bank, CCMIS_THRESH_PAGE, offset);
	request.length = 2;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", found_name);
		print_float(PRINT_JSON, "value", is_f16 ? "%e" : "%.2f",
			    value);
		print_uint(PRINT_JSON, "raw", "0x%04x", raw);
		close_json_object();
	} else {
		if (is_f16)
			printf("  Setting %s = %e (F16 raw=0x%04x)\n",
			       found_name, value, raw);
		else
			printf("  Setting %s = %.2f dBm (raw=0x%04x)\n",
			       found_name, value, raw);
	}

	return 0;
}

/* Enable/disable descriptor table */
static const struct {
	const char *name;
	__u8 offset;
	__u8 mask;
} thresh_enable_descs[] = {
	{ "total-pwr", CCMIS_THRESH_USE_CFG_OFFSET, CCMIS_THRESH_USE_CFG_TOTAL_PWR },
	{ "sig-pwr",   CCMIS_THRESH_USE_CFG_OFFSET, CCMIS_THRESH_USE_CFG_SIG_PWR },
	{ "fdd",       CCMIS_THRESH_FEC_ENABLE_OFFSET, CCMIS_THRESH_FDD_ENABLE },
	{ "fed",       CCMIS_THRESH_FEC_ENABLE_OFFSET, CCMIS_THRESH_FED_ENABLE },
};

#define NUM_THRESH_ENABLE_DESCS ARRAY_SIZE(thresh_enable_descs)

int cmis_coherent_threshold_enable(struct cmd_context *ctx, int bank,
				   const char *type, bool enable)
{
	struct module_eeprom request;
	__u8 buf, old;
	unsigned int i;
	int ret;
	const char *found_name = NULL;
	__u8 offset = 0, mask = 0;

	for (i = 0; i < NUM_THRESH_ENABLE_DESCS; i++) {
		if (strcasecmp(type, thresh_enable_descs[i].name) == 0) {
			found_name = thresh_enable_descs[i].name;
			offset = thresh_enable_descs[i].offset;
			mask = thresh_enable_descs[i].mask;
			break;
		}
	}

	if (!found_name) {
		fprintf(stderr, "Unknown threshold enable type: %s\n", type);
		fprintf(stderr, "Valid types:");
		for (i = 0; i < NUM_THRESH_ENABLE_DESCS; i++)
			fprintf(stderr, " %s", thresh_enable_descs[i].name);
		fprintf(stderr, "\n");
		return -EINVAL;
	}

	cmis_request_init(&request, bank, CCMIS_THRESH_PAGE, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	old = request.data[0];
	if (enable)
		buf = old | mask;
	else
		buf = old & ~mask;

	cmis_request_init(&request, bank, CCMIS_THRESH_PAGE, offset);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", found_name);
		print_bool(PRINT_JSON, "enabled", NULL, enable);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  %s %s: 0x%02x -> 0x%02x\n",
		       found_name, enable ? "enabled" : "disabled", old, buf);
	}
	return 0;
}

/*----------------------------------------------------------------------
 * Generic page-level RMW helper for coherent pages
 */
static int
ccmis_rmw_byte(struct cmd_context *ctx, int bank, __u8 page, __u8 offset,
	       __u8 mask, bool set, const char *name)
{
	struct module_eeprom request;
	__u8 buf, old;
	int ret;

	cmis_request_init(&request, bank, page, offset);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	old = request.data[0];
	buf = set ? (old | mask) : (old & ~mask);

	cmis_request_init(&request, bank, page, offset);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", name);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		printf("  %s: 0x%02x -> 0x%02x\n", name, old, buf);
	}
	return 0;
}

/*----------------------------------------------------------------------
 * Media Lane Provisioning (Page 31h)
 */

int cmis_coherent_set_tx_filter_enable(struct cmd_context *ctx, int bank,
				       __u8 lane_mask, bool enable)
{
	return ccmis_rmw_byte(ctx, bank, CCMIS_PROV_PAGE,
			      CCMIS_PROV_TX_FILTER_ENABLE,
			      lane_mask, enable, "TxFilterEnable");
}

int cmis_coherent_set_tx_filter_type(struct cmd_context *ctx, int bank,
				     int lane, __u8 type)
{
	struct module_eeprom request;
	__u8 buf, old, shift;
	int ret;

	if (lane < 0 || lane > 7 || type > 3)
		return -EINVAL;

	/* 2 bits per lane, packed 4 lanes per byte.
	 * Byte 0x81 holds lanes 0-3, byte 0x82 holds lanes 4-7.
	 */
	shift = (lane % 4) * 2;

	cmis_request_init(&request, bank, CCMIS_PROV_PAGE,
			  CCMIS_PROV_TX_FILTER_TYPE + lane / 4);
	request.length = 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	old = request.data[0];
	buf = (old & ~(0x03 << shift)) | ((type & 0x03) << shift);

	cmis_request_init(&request, bank, CCMIS_PROV_PAGE,
			  CCMIS_PROV_TX_FILTER_TYPE + lane / 4);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		static const char *type_names[] = {
			"None", "RRC", "RC", "Gaussian"
		};

		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", "TxFilterType");
		print_uint(PRINT_JSON, "lane", "%u", lane);
		print_string(PRINT_JSON, "type", "%s", type_names[type]);
		print_uint(PRINT_JSON, "old_value", "0x%02x", old);
		print_uint(PRINT_JSON, "new_value", "0x%02x", buf);
		close_json_object();
	} else {
		static const char *type_names[] = {
			"None", "RRC", "RC", "Gaussian"
		};

		printf("  TxFilterType lane %d: %s (0x%02x -> 0x%02x)\n",
		       lane, type_names[type], old, buf);
	}
	return 0;
}

int cmis_coherent_set_lf_insertion(struct cmd_context *ctx, int bank,
				   __u8 lane_mask, bool enable)
{
	return ccmis_rmw_byte(ctx, bank, CCMIS_PROV_PAGE,
			      CCMIS_PROV_LF_INSERTION,
			      lane_mask, enable, "LfInsertionOnLdEnable");
}

/*----------------------------------------------------------------------
 * Media Lane Flag Masks (Page 32h)
 */

int cmis_coherent_set_media_mask(struct cmd_context *ctx, int bank,
				 __u8 offset, __u8 mask, bool enable)
{
	char name[48];

	snprintf(name, sizeof(name), "MediaFlagMask[0x%02x]", offset);
	return ccmis_rmw_byte(ctx, bank, CCMIS_MEDIA_MASKS_PAGE,
			      offset, mask, enable, name);
}

/*----------------------------------------------------------------------
 * Host Interface Thresholds (Page 38h)
 */

static const struct {
	const char *name;
	__u8 offset;
} host_thresh_descs[] = {
	{ "host-fdd-raise", CCMIS_HOST_THRESH_FDD_RAISE },
	{ "host-fdd-clear", CCMIS_HOST_THRESH_FDD_CLEAR },
	{ "host-fed-raise", CCMIS_HOST_THRESH_FED_RAISE },
	{ "host-fed-clear", CCMIS_HOST_THRESH_FED_CLEAR },
};

#define NUM_HOST_THRESH_DESCS ARRAY_SIZE(host_thresh_descs)

int cmis_coherent_set_host_threshold(struct cmd_context *ctx, int bank,
				     const char *type, double value)
{
	struct module_eeprom request;
	__u8 buf[2];
	__u16 raw;
	unsigned int i;
	int ret;
	const char *found_name = NULL;
	__u8 offset = 0;

	for (i = 0; i < NUM_HOST_THRESH_DESCS; i++) {
		if (strcasecmp(type, host_thresh_descs[i].name) == 0) {
			found_name = host_thresh_descs[i].name;
			offset = host_thresh_descs[i].offset;
			break;
		}
	}

	if (!found_name) {
		fprintf(stderr, "Unknown host threshold type: %s\n", type);
		fprintf(stderr, "Valid types:");
		for (i = 0; i < NUM_HOST_THRESH_DESCS; i++)
			fprintf(stderr, " %s", host_thresh_descs[i].name);
		fprintf(stderr, "\n");
		return -EINVAL;
	}

	raw = double_to_f16(value);

	buf[0] = (raw >> 8) & 0xFF;
	buf[1] = raw & 0xFF;

	cmis_request_init(&request, bank, CCMIS_HOST_THRESH_PAGE, offset);
	request.length = 2;
	request.data = buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;

	if (is_json_context()) {
		open_json_object("write_result");
		print_string(PRINT_JSON, "register", "%s", found_name);
		print_float(PRINT_JSON, "value", "%e", value);
		print_uint(PRINT_JSON, "raw", "0x%04x", raw);
		close_json_object();
	} else {
		printf("  Setting %s = %e (F16 raw=0x%04x)\n",
		       found_name, value, raw);
	}

	return 0;
}

static const struct {
	const char *name;
	__u8 mask;
} host_thresh_enable_descs[] = {
	{ "host-fdd", CCMIS_HOST_THRESH_FDD_ENABLE },
	{ "host-fed", CCMIS_HOST_THRESH_FED_ENABLE },
};

#define NUM_HOST_THRESH_EN_DESCS ARRAY_SIZE(host_thresh_enable_descs)

int cmis_coherent_host_threshold_enable(struct cmd_context *ctx, int bank,
					const char *type, bool enable)
{
	unsigned int i;
	const char *found_name = NULL;
	__u8 mask = 0;

	for (i = 0; i < NUM_HOST_THRESH_EN_DESCS; i++) {
		if (strcasecmp(type, host_thresh_enable_descs[i].name) == 0) {
			found_name = host_thresh_enable_descs[i].name;
			mask = host_thresh_enable_descs[i].mask;
			break;
		}
	}

	if (!found_name) {
		fprintf(stderr, "Unknown host threshold enable type: %s\n",
			type);
		fprintf(stderr, "Valid types:");
		for (i = 0; i < NUM_HOST_THRESH_EN_DESCS; i++)
			fprintf(stderr, " %s",
				host_thresh_enable_descs[i].name);
		fprintf(stderr, "\n");
		return -EINVAL;
	}

	return ccmis_rmw_byte(ctx, bank, CCMIS_HOST_THRESH_PAGE,
			      CCMIS_HOST_THRESH_ENABLE_OFFSET,
			      mask, enable,
			      enable ? "HostThreshEnable" :
				       "HostThreshDisable");
}

/*----------------------------------------------------------------------
 * Host Data Path Flag Masks (Page 3Bh)
 */

int cmis_coherent_set_host_mask(struct cmd_context *ctx, int bank,
				__u8 offset, __u8 mask, bool enable)
{
	char name[48];

	snprintf(name, sizeof(name), "HostFlagMask[0x%02x]", offset);
	return ccmis_rmw_byte(ctx, bank, CCMIS_FLAGS_HOST_PAGE,
			      offset, mask, enable, name);
}
