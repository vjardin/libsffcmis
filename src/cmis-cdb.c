/*
 * cmis-cdb.c: CDB (Command Data Block) messaging and PM retrieval
 *
 * CDB command execution (OIF-CMIS-05.3, Section 9) and Performance
 * Monitoring via CDB commands 0x0200-0x0217 (Section 9.8).
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"
#include "sff-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-cdb.h"
#include "json_print.h"
#include "module-common.h"

/*----------------------------------------------------------------------
 * CDB command execution infrastructure
 *----------------------------------------------------------------------*/

/*
 * Read the CDB status byte from Page 00h:0x25 (instance 1).
 * Reads only 6 bytes (0x20-0x25) to fit in one CH341 chunk.
 * Returns the raw status byte or -1 on I2C error.
 */
static int
cdb_read_status(struct cmd_context *ctx)
{
	struct module_eeprom request;
	int ret;

	cmis_request_init(&request, 0, 0x0, 0x20);
	request.length = CMIS_CDB_STATUS1_OFFSET - 0x20 + 1;
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return -1;
	return request.data[CMIS_CDB_STATUS1_OFFSET - 0x20];
}

/*
 * Wait for CDB command completion by polling CdbStatus1.
 * Returns the final status byte, or -1 on timeout/I2C error.
 */
static int
cdb_wait_completion(struct cmd_context *ctx, int timeout_ms)
{
	int elapsed = 0;
	int status;

	while (elapsed < timeout_ms) {
		usleep(10000);	/* 10 ms poll interval */
		elapsed += 10;

		status = cdb_read_status(ctx);
		if (status < 0)
			continue;
		if (!(status & CMIS_CDB_STATUS_BUSY))
			return status;
	}
	return -1;
}

const char *
cmis_cdb_status_str(int status)
{
	if (status < 0)
		return "I2C error / timeout";
	if (status & CMIS_CDB_STATUS_BUSY)
		return "Busy";
	if (status & CMIS_CDB_STATUS_FAIL) {
		switch (status & CMIS_CDB_STATUS_RESULT_MASK) {
		case CMIS_CDB_FAIL_UNKNOWN_CMD:	return "Unknown command";
		case CMIS_CDB_FAIL_PARAM_RANGE:	return "Parameter range error";
		case CMIS_CDB_FAIL_ABORT_PREV:	return "Previous CMD not aborted";
		case CMIS_CDB_FAIL_CMD_TIMEOUT:	return "Command timeout";
		case CMIS_CDB_FAIL_CHKCODE:	return "Check code error";
		case CMIS_CDB_FAIL_PASSWORD:	return "Password error";
		case CMIS_CDB_FAIL_COMPAT:	return "Incompatible state";
		default:			return "Failed (unknown reason)";
		}
	}
	switch (status & CMIS_CDB_STATUS_RESULT_MASK) {
	case CMIS_CDB_STS_SUCCESS:	return "Success";
	case CMIS_CDB_STS_ABORTED:	return "Aborted";
	default:			return "Unknown status";
	}
}

/*
 * Read Page 01h bytes 165-166 and decode the CDB max busy timeout.
 * Returns the timeout in milliseconds, or 5000 ms as fallback.
 *
 * Byte 166 bit 7 = MaxBusySpecMethod:
 *   0 (short):    TCDBB = (80 - min(80, X)) ms, X = bits 6-0
 *   1 (extended): TCDBB = max(1, X) * 160 ms,   X = byte 165 bits 4-0
 */
static int
cdb_get_busy_timeout(struct cmd_context *ctx)
{
	struct module_eeprom request;
	int ret;
	__u8 byte_165, byte_166;
	int timeout_ms;

	cmis_request_init(&request, 0, 0x1, CMIS_CDB_ADVER_TRIGGER_OFFSET);
	request.length = 2;  /* bytes 165-166 */
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return 5000;

	byte_165 = request.data[0];
	byte_166 = request.data[1];

	if (byte_166 & CMIS_CDB_BUSY_SPEC_METHOD_MASK) {
		/* Extended method */
		int x = byte_165 & CMIS_CDB_ADVER_EXT_BUSY_MASK;

		if (x < 1)
			x = 1;
		timeout_ms = x * 160;
	} else {
		/* Short method */
		int x = byte_166 & CMIS_CDB_BUSY_TIME_MASK;

		if (x > 80)
			x = 80;
		timeout_ms = 80 - x;
	}

	return timeout_ms > 0 ? timeout_ms : 5000;
}

int
cmis_cdb_send_command_lpl(struct cmd_context *ctx, __u16 cmd_id,
			  const __u8 *lpl_data, __u8 lpl_len,
			  __u8 *reply, __u8 *rpl_len)
{
	struct module_eeprom request;
	__u8 hdr[8];
	int ret, status;
	__u8 chksum;
	int i;
	int timeout_ms;

	/* Read spec-defined max busy timeout from Page 01h */
	timeout_ms = cdb_get_busy_timeout(ctx);

	/* First check that CDB is not busy */
	status = cdb_read_status(ctx);
	if (status < 0)
		return -1;
	if (status & CMIS_CDB_STATUS_BUSY) {
		fprintf(stderr, "CDB busy, waiting...\n");
		status = cdb_wait_completion(ctx, timeout_ms);
		if (status < 0 || (status & CMIS_CDB_STATUS_BUSY))
			return -1;
	}

	/* Build CDB header (Page 9Fh layout):
	 * 0x80-0x81  CMDID (big-endian) — trigger point
	 * 0x82-0x83  EPLLength = 0
	 * 0x84       LPLLength
	 * 0x85       CdbChkCode = ~(sum of CMDID + EPL len + LPL len + LPL data)
	 * 0x86       RPLLength (pre-clear)
	 * 0x87       RPLChkCode (pre-clear)
	 */
	hdr[0] = (cmd_id >> 8) & 0xFF;
	hdr[1] = cmd_id & 0xFF;
	hdr[2] = 0;	/* EPL length MSB */
	hdr[3] = 0;	/* EPL length LSB */
	hdr[4] = lpl_len;
	chksum = hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4];
	for (i = 0; i < lpl_len; i++)
		chksum += lpl_data[i];
	hdr[5] = ~chksum;
	hdr[6] = 0;
	hdr[7] = 0;

	/* Write LPL payload first if present */
	if (lpl_len > 0 && lpl_data) {
		cmis_request_init(&request, 0, 0x9F, CMIS_CDB_LPL_START);
		request.length = lpl_len;
		request.data = (__u8 *)lpl_data;
		ret = set_eeprom_page(ctx, &request);
		if (ret < 0) {
			fprintf(stderr, "CDB: failed to write LPL payload\n");
			return -1;
		}
	}

	/* Write body (bytes 0x82-0x87), then CMDID (0x80-0x81).
	 * This order works for both trigger methods.
	 */
	cmis_request_init(&request, 0, 0x9F, CMIS_CDB_EPL_LEN_MSB);
	request.length = 6;
	request.data = hdr + 2;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "CDB: failed to write command body\n");
		return -1;
	}

	/* Write CMDID to trigger the command */
	cmis_request_init(&request, 0, 0x9F, CMIS_CDB_CMD_MSB);
	request.length = 2;
	request.data = hdr;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "CDB: failed to write CMDID trigger\n");
		return -1;
	}

	/* Wait for completion */
	status = cdb_wait_completion(ctx, timeout_ms);
	if (status < 0)
		return -1;

	/* Read reply if requested and command succeeded */
	if (reply && !(status & CMIS_CDB_STATUS_FAIL)) {
		cmis_request_init(&request, 0, 0x9F, CMIS_CDB_RPL_LEN);
		request.length = 2;
		ret = get_eeprom_page(ctx, &request);
		if (ret < 0)
			return status;

		__u8 rlen = request.data[0];
		if (rpl_len)
			*rpl_len = rlen;

		if (rlen > 0 && rlen <= 120) {
			cmis_request_init(&request, 0, 0x9F,
					  CMIS_CDB_LPL_START);
			request.length = rlen;
			ret = get_eeprom_page(ctx, &request);
			if (ret < 0)
				return status;
			memcpy(reply, request.data, rlen);
		}
	}

	return status;
}

int
cmis_cdb_send_command(struct cmd_context *ctx, __u16 cmd_id,
		      __u8 *reply, __u8 *rpl_len)
{
	return cmis_cdb_send_command_lpl(ctx, cmd_id, NULL, 0, reply, rpl_len);
}

int
cmis_cdb_read_status(struct cmd_context *ctx)
{
	return cdb_read_status(ctx);
}

/*
 * Known standard CDB command names.
 */
struct cdb_cmd_info {
	__u16 cmd_id;
	const char *name;
};

static const struct cdb_cmd_info cdb_std_cmds[] = {
	{ 0x0000, "Query Status" },
	{ 0x0001, "Enter Password" },
	{ 0x0002, "Change Password" },
	{ 0x0003, "Read Module Capability Map" },
	{ 0x0004, "Abort" },
	{ 0x0040, "Module Features" },
	{ 0x0041, "Firmware Management Features" },
	{ 0x0042, "Performance Monitoring Features" },
	{ 0x0043, "BERT and Diagnostics Features" },
	{ 0x0044, "Security Features" },
	{ 0x0045, "Externally Defined Features" },
	{ 0x0100, "Get Firmware Info" },
	{ 0x0101, "Start Firmware Download" },
	{ 0x0102, "Abort Firmware Download" },
	{ 0x0103, "Write Firmware Block (LPL)" },
	{ 0x0104, "Write Firmware Block (EPL)" },
	{ 0x0105, "Read Firmware Block (LPL)" },
	{ 0x0106, "Read Firmware Block (EPL)" },
	{ 0x0107, "Complete Firmware Download" },
	{ 0x0108, "Copy Firmware Image" },
	{ 0x0109, "Run Firmware Image" },
	{ 0x010A, "Commit Firmware Image" },
	{ 0x0200, "Control PM" },
	{ 0x0201, "Get PM Feature Information" },
	{ 0x0210, "Get Module PM (LPL)" },
	{ 0x0211, "Get Module PM (EPL)" },
	{ 0x0212, "Get PM Host Side (LPL)" },
	{ 0x0213, "Get PM Host Side (EPL)" },
	{ 0x0214, "Get PM Media Side (LPL)" },
	{ 0x0215, "Get PM Media Side (EPL)" },
	{ 0x0216, "Get Data Path PM (LPL)" },
	{ 0x0217, "Get Data Path PM (EPL)" },
	{ 0x0400, "Get Certificate (LPL)" },
	{ 0x0401, "Get Certificate (EPL)" },
	{ 0x0402, "Set Digest (LPL)" },
	{ 0x0403, "Set Digest (EPL)" },
	{ 0x0404, "Get Signature (LPL)" },
	{ 0x0405, "Get Signature (EPL)" },
};

#define NUM_STD_CMDS ARRAY_SIZE(cdb_std_cmds)

const char *
cmis_cdb_cmd_name(__u16 cmd_id)
{
	unsigned int i;

	for (i = 0; i < NUM_STD_CMDS; i++)
		if (cdb_std_cmds[i].cmd_id == cmd_id)
			return cdb_std_cmds[i].name;

	if (cmd_id >= CMIS_CDB_CMD_VENDOR_START)
		return "Vendor-specific";
	return "Unknown";
}

/*----------------------------------------------------------------------
 * CDB Performance Monitoring (Section 9.8)
 *----------------------------------------------------------------------*/

/* F16 (IEEE 754 half-precision) to double */
static double
pm_f16_to_double(__u16 raw)
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

static inline __u16
pm_get_u16(const __u8 *p)
{
	return ((__u16)p[0] << 8) | p[1];
}

static inline __s16
pm_get_s16(const __u8 *p)
{
	return (__s16)((__u16)p[0] << 8 | p[1]);
}

/*
 * Query CDB PM Feature Information (CMD 0x0201).
 *
 * Reply (4 bytes):
 *   Byte 0: HostSideMonitors (bit 0: SNR, bit 1: PAM4 LTP)
 *   Byte 1: MediaSideMonitors (bit 0: SNR, bit 1: LTP)
 *   Bytes 2-3: Reserved
 */
static int
cdb_show_pm_features(struct cmd_context *ctx,
		     __u8 *host_feat, __u8 *media_feat)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;

	*host_feat = 0;
	*media_feat = 0;

	if (!is_json_context())
		printf("\nPM Features (CMD 0x0201):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_PM_FEATURE_INFO,
				       reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		return -1;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context()) {
			open_json_object("pm_features");
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
			close_json_object();
		} else {
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		}
		return -1;
	}

	if (rpl_len < 2) {
		if (!is_json_context())
			printf("  No PM features advertised\n");
		return 0;
	}

	*host_feat = reply[0];
	*media_feat = reply[1];

	if (is_json_context()) {
		open_json_object("pm_features");
		print_bool(PRINT_JSON, "host_snr", NULL,
			   !!(*host_feat & 0x01));
		print_bool(PRINT_JSON, "host_pam4_ltp", NULL,
			   !!(*host_feat & 0x02));
		print_bool(PRINT_JSON, "host_pre_fec_ber", NULL,
			   !!(*host_feat & 0x04));
		print_bool(PRINT_JSON, "media_snr", NULL,
			   !!(*media_feat & 0x01));
		print_bool(PRINT_JSON, "media_ltp", NULL,
			   !!(*media_feat & 0x02));
		close_json_object();
	} else {
		if (*host_feat == 0 && *media_feat == 0) {
			printf("  No PM features advertised\n");
			return 0;
		}

		printf("  Host Side:  ");
		if (*host_feat & 0x01)
			printf(" SNR");
		if (*host_feat & 0x02)
			printf(" PAM4-LTP");
		if (*host_feat & 0x04)
			printf(" Pre-FEC-BER");
		if (*host_feat == 0)
			printf(" (none)");
		printf("\n");

		printf("  Media Side: ");
		if (*media_feat & 0x01)
			printf(" SNR");
		if (*media_feat & 0x02)
			printf(" LTP");
		if (*media_feat == 0)
			printf(" (none)");
		printf("\n");
	}

	return 0;
}

/*
 * Print a PM table header row.
 */
static void
pm_print_header(bool show_lane, const char *lane_label)
{
	if (is_json_context())
		return;

	if (show_lane)
		printf("  %-4s  %-20s  %10s  %10s  %10s  %10s  %s\n",
		       lane_label, "Observable",
		       "Min", "Avg", "Max", "Current", "Unit");
	else
		printf("  %-20s  %10s  %10s  %10s  %10s  %s\n",
		       "Observable",
		       "Min", "Avg", "Max", "Current", "Unit");
	if (show_lane)
		printf("  %-4s  %-20s  %10s  %10s  %10s  %10s  %s\n",
		       "----", "--------------------",
		       "----------", "----------",
		       "----------", "----------", "----");
	else
		printf("  %-20s  %10s  %10s  %10s  %10s  %s\n",
		       "--------------------",
		       "----------", "----------",
		       "----------", "----------", "----");
}

static void
pm_print_record_temp(int lane, const char *name,
		     const __u8 *data, bool has_current, bool show_lane)
{
	double mn = pm_get_s16(data + 0) / 256.0;
	double av = pm_get_s16(data + 2) / 256.0;
	double mx = pm_get_s16(data + 4) / 256.0;

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.2f", mn);
		print_float(PRINT_JSON, "avg", "%.2f", av);
		print_float(PRINT_JSON, "max", "%.2f", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.2f",
				    pm_get_s16(data + 6) / 256.0);
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", "degC");
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.2f  %10.2f  %10.2f",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.2f  %10.2f  %10.2f",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.2f", pm_get_s16(data + 6) / 256.0);
	else
		printf("  %10s", "-");
	printf("  degC\n");
}

static void
pm_print_record_vcc(int lane, const char *name,
		    const __u8 *data, bool has_current, bool show_lane)
{
	double mn = pm_get_u16(data + 0) * 0.0001;
	double av = pm_get_u16(data + 2) * 0.0001;
	double mx = pm_get_u16(data + 4) * 0.0001;

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.4f", mn);
		print_float(PRINT_JSON, "avg", "%.4f", av);
		print_float(PRINT_JSON, "max", "%.4f", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.4f",
				    pm_get_u16(data + 6) * 0.0001);
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", "V");
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.4f  %10.4f  %10.4f",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.4f  %10.4f  %10.4f",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.4f", pm_get_u16(data + 6) * 0.0001);
	else
		printf("  %10s", "-");
	printf("  V\n");
}

static void
pm_print_record_snr(int lane, const char *name,
		    const __u8 *data, bool has_current, bool show_lane)
{
	double mn = pm_get_u16(data + 0) / 256.0;
	double av = pm_get_u16(data + 2) / 256.0;
	double mx = pm_get_u16(data + 4) / 256.0;

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.2f", mn);
		print_float(PRINT_JSON, "avg", "%.2f", av);
		print_float(PRINT_JSON, "max", "%.2f", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.2f",
				    pm_get_u16(data + 6) / 256.0);
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", "dB");
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.2f  %10.2f  %10.2f",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.2f  %10.2f  %10.2f",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.2f", pm_get_u16(data + 6) / 256.0);
	else
		printf("  %10s", "-");
	printf("  dB\n");
}

static void
pm_print_record_f16(int lane, const char *name,
		    const __u8 *data, bool has_current, bool show_lane,
		    const char *unit)
{
	double mn = pm_f16_to_double(pm_get_u16(data + 0));
	double av = pm_f16_to_double(pm_get_u16(data + 2));
	double mx = pm_f16_to_double(pm_get_u16(data + 4));

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.3e", mn);
		print_float(PRINT_JSON, "avg", "%.3e", av);
		print_float(PRINT_JSON, "max", "%.3e", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.3e",
				    pm_f16_to_double(pm_get_u16(data + 6)));
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", unit);
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.3e  %10.3e  %10.3e",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.3e  %10.3e  %10.3e",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.3e", pm_f16_to_double(pm_get_u16(data + 6)));
	else
		printf("  %10s", "-");
	printf("  %s\n", unit);
}

static void
pm_print_record_bias(int lane, const char *name,
		     const __u8 *data, bool has_current, bool show_lane)
{
	double mn = pm_get_u16(data + 0) * 2.0;
	double av = pm_get_u16(data + 2) * 2.0;
	double mx = pm_get_u16(data + 4) * 2.0;

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.0f", mn);
		print_float(PRINT_JSON, "avg", "%.0f", av);
		print_float(PRINT_JSON, "max", "%.0f", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.0f",
				    pm_get_u16(data + 6) * 2.0);
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", "uA");
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.0f  %10.0f  %10.0f",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.0f  %10.0f  %10.0f",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.0f", pm_get_u16(data + 6) * 2.0);
	else
		printf("  %10s", "-");
	printf("  uA\n");
}

static void
pm_print_record_power(int lane, const char *name,
		      const __u8 *data, bool has_current, bool show_lane)
{
	double mn = pm_get_u16(data + 0) * 0.1;
	double av = pm_get_u16(data + 2) * 0.1;
	double mx = pm_get_u16(data + 4) * 0.1;

	if (is_json_context()) {
		open_json_object(NULL);
		if (show_lane)
			print_int(PRINT_JSON, "lane", "%d", lane);
		print_string(PRINT_JSON, "observable", "%s", name);
		print_float(PRINT_JSON, "min", "%.1f", mn);
		print_float(PRINT_JSON, "avg", "%.1f", av);
		print_float(PRINT_JSON, "max", "%.1f", mx);
		if (has_current)
			print_float(PRINT_JSON, "current", "%.1f",
				    pm_get_u16(data + 6) * 0.1);
		else
			print_null(PRINT_JSON, "current", "%s", NULL);
		print_string(PRINT_JSON, "unit", "%s", "uW");
		close_json_object();
		return;
	}

	if (show_lane)
		printf("  %-4d  %-20s  %10.1f  %10.1f  %10.1f",
		       lane, name, mn, av, mx);
	else
		printf("  %-20s  %10.1f  %10.1f  %10.1f",
		       name, mn, av, mx);
	if (has_current)
		printf("  %10.1f", pm_get_u16(data + 6) * 0.1);
	else
		printf("  %10s", "-");
	printf("  uW\n");
}

/*
 * Module PM (CMD 0x0210) — Section 9.8.5
 *
 * LPL request (5 bytes):
 *   Byte 0: RecordType (bit 0: 1=include current, bit 7: ClearOnRead)
 *   Byte 1: Observables bitmask
 *   Bytes 2-4: Reserved
 */
static void
cdb_show_module_pm(struct cmd_context *ctx)
{
	__u8 lpl[5] = { 0 };
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int off;
	bool has_current;

	if (is_json_context())
		open_json_object("module_pm");
	else
		printf("\nModule PM (CMD 0x0210):\n");

	lpl[0] = 0x01;  /* RecordType: include current value */
	lpl[1] = 0x03;  /* Temperature + Vcc */

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_GET_MODULE_PM,
					   lpl, 5, reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (rpl_len < 1) {
		if (!is_json_context())
			printf("  Empty reply\n");
		if (is_json_context())
			close_json_object();
		return;
	}

	has_current = reply[0] & 0x01;
	off = 1;

	if (is_json_context()) {
		print_bool(PRINT_JSON, "has_current", NULL, has_current);
		open_json_array("records", NULL);
	} else {
		pm_print_header(false, NULL);
	}

	if (off + (has_current ? 8 : 6) <= rpl_len) {
		pm_print_record_temp(0, "Temperature",
				     reply + off, has_current, false);
		off += has_current ? 8 : 6;
	}

	if (off + (has_current ? 8 : 6) <= rpl_len) {
		pm_print_record_vcc(0, "Vcc",
				    reply + off, has_current, false);
	}

	if (is_json_context()) {
		close_json_array(NULL);
		close_json_object();
	}
}

/*
 * Host Side PM (CMD 0x0212) — Section 9.8.7
 *
 * LPL request (20 bytes):
 *   Byte 0:    RecordType
 *   Bytes 1-3: Reserved
 *   Bytes 4-7: LanesBitmask (U32 BE)
 *   Byte 8:    Observables
 *   Bytes 9-19: Reserved
 */
static void
cdb_show_host_pm(struct cmd_context *ctx, __u8 host_feat)
{
	__u8 lpl[20] = { 0 };
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int off, lane, rec_size;
	bool has_current;

	if (host_feat == 0)
		return;

	if (is_json_context())
		open_json_object("host_pm");
	else
		printf("\nHost Side PM (CMD 0x0212):\n");

	lpl[0] = 0x01;
	lpl[4] = 0xFF;
	lpl[5] = 0xFF;
	lpl[6] = 0xFF;
	lpl[7] = 0xFF;
	lpl[8] = host_feat;

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_GET_HOST_PM,
					   lpl, 20, reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (rpl_len < 2) {
		if (!is_json_context())
			printf("  Empty reply\n");
		if (is_json_context())
			close_json_object();
		return;
	}

	has_current = reply[0] & 0x01;
	rec_size = has_current ? 8 : 6;

	if (is_json_context())
		open_json_array("records", NULL);
	else
		pm_print_header(true, "Lane");

	off = 1;
	lane = 1;
	while (off + rec_size <= rpl_len) {
		if (host_feat & 0x01) {
			if (off + rec_size > rpl_len)
				break;
			pm_print_record_snr(lane, "SNR",
					    reply + off, has_current, true);
			off += rec_size;
		}
		if (host_feat & 0x02) {
			if (off + rec_size > rpl_len)
				break;
			pm_print_record_f16(lane, "PAM4 LTP",
					    reply + off, has_current, true,
					    "dB");
			off += rec_size;
		}
		if (host_feat & 0x04) {
			if (off + rec_size > rpl_len)
				break;
			pm_print_record_f16(lane, "Pre-FEC BER",
					    reply + off, has_current, true,
					    "");
			off += rec_size;
		}
		lane++;
	}

	if (is_json_context()) {
		close_json_array(NULL);
		close_json_object();
	}
}

/*
 * Media Side PM (CMD 0x0214) — Section 9.8.9
 *
 * LPL request (20 bytes):
 *   Byte 0:    RecordType
 *   Bytes 1-3: Reserved
 *   Bytes 4-7: LanesBitmask (U32 BE)
 *   Byte 8:    Observables byte 1 (from features)
 *   Byte 9:    Observables byte 2 (Bias/TxPwr/RxPwr/LaserTemp)
 *   Bytes 10-19: Reserved
 */
static void
cdb_show_media_pm(struct cmd_context *ctx, __u8 media_feat)
{
	__u8 lpl[20] = { 0 };
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int off, lane, rec_size;
	bool has_current;

	if (is_json_context())
		open_json_object("media_pm");
	else
		printf("\nMedia Side PM (CMD 0x0214):\n");

	lpl[0] = 0x01;
	lpl[4] = 0xFF;
	lpl[5] = 0xFF;
	lpl[6] = 0xFF;
	lpl[7] = 0xFF;
	lpl[8] = media_feat;
	lpl[9] = 0x0F;  /* Bias + TxPwr + RxPwr + LaserTemp */

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_GET_MEDIA_PM,
					   lpl, 20, reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (rpl_len < 2) {
		if (!is_json_context())
			printf("  Empty reply\n");
		if (is_json_context())
			close_json_object();
		return;
	}

	has_current = reply[0] & 0x01;
	rec_size = has_current ? 8 : 6;

	if (is_json_context())
		open_json_array("records", NULL);
	else
		pm_print_header(true, "Lane");

	off = 1;
	lane = 1;
	while (off + rec_size <= rpl_len) {
		if (media_feat & 0x01) {
			if (off + rec_size > rpl_len)
				break;
			pm_print_record_snr(lane, "SNR",
					    reply + off, has_current, true);
			off += rec_size;
		}
		if (media_feat & 0x02) {
			if (off + rec_size > rpl_len)
				break;
			pm_print_record_f16(lane, "LTP",
					    reply + off, has_current, true,
					    "dB");
			off += rec_size;
		}
		/* Observables byte 2: always request all 4 */
		if (off + rec_size > rpl_len)
			goto next_lane;
		pm_print_record_bias(lane, "Tx Bias",
				     reply + off, has_current, true);
		off += rec_size;

		if (off + rec_size > rpl_len)
			goto next_lane;
		pm_print_record_power(lane, "Tx Power",
				      reply + off, has_current, true);
		off += rec_size;

		if (off + rec_size > rpl_len)
			goto next_lane;
		pm_print_record_power(lane, "Rx Power",
				      reply + off, has_current, true);
		off += rec_size;

		if (off + rec_size > rpl_len)
			goto next_lane;
		pm_print_record_temp(lane, "Laser Temp",
				     reply + off, has_current, true);
		off += rec_size;

next_lane:
		lane++;
	}

	if (is_json_context()) {
		close_json_array(NULL);
		close_json_object();
	}
}

/*
 * Data Path PM (CMD 0x0216) — Section 9.8.11
 *
 * LPL request (20 bytes):
 *   Byte 0:    RecordType
 *   Bytes 1-3: Reserved
 *   Bytes 4-7: DataPathsBitmask (U32 BE)
 *   Byte 8:    Observables (bit 0: FERC, bit 1: Pre-FEC BER)
 *   Bytes 9-19: Reserved
 */
static void
cdb_show_dp_pm(struct cmd_context *ctx)
{
	__u8 lpl[20] = { 0 };
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int off, dp, rec_size;
	bool has_current;

	if (is_json_context())
		open_json_object("dp_pm");
	else
		printf("\nData Path PM (CMD 0x0216):\n");

	lpl[0] = 0x01;
	lpl[4] = 0xFF;
	lpl[5] = 0xFF;
	lpl[6] = 0xFF;
	lpl[7] = 0xFF;
	lpl[8] = 0x03;  /* FERC + Pre-FEC BER */

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_GET_DP_PM,
					   lpl, 20, reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (rpl_len < 2) {
		if (!is_json_context())
			printf("  Empty reply\n");
		if (is_json_context())
			close_json_object();
		return;
	}

	has_current = reply[0] & 0x01;
	rec_size = has_current ? 8 : 6;

	if (is_json_context())
		open_json_array("records", NULL);
	else
		pm_print_header(true, "DP");

	off = 1;
	dp = 1;
	while (off + rec_size <= rpl_len) {
		if (off + rec_size > rpl_len)
			break;
		pm_print_record_f16(dp, "FERC",
				    reply + off, has_current, true, "");
		off += rec_size;

		if (off + rec_size > rpl_len)
			break;
		pm_print_record_f16(dp, "Pre-FEC BER",
				    reply + off, has_current, true, "");
		off += rec_size;

		dp++;
	}

	if (is_json_context()) {
		close_json_array(NULL);
		close_json_object();
	}
}

/*
 * Read a CMIS upper memory page.  Returns a pointer adjusted so that
 * ptr[0x80..0xFF] maps to the page data (matching spec offsets).
 */
static const __u8 *
cdb_read_cmis_page(struct cmd_context *ctx, __u8 bank, __u8 page)
{
	struct module_eeprom req;

	cmis_request_init(&req, bank, page, CMIS_PAGE_SIZE);
	if (get_eeprom_page(ctx, &req) < 0)
		return NULL;
	return req.data - CMIS_PAGE_SIZE;
}

int
cmis_cdb_show_pm(struct cmd_context *ctx)
{
	const __u8 *page_01;
	__u8 cdb_adver, instances;
	__u8 host_feat = 0, media_feat = 0;

	page_01 = cdb_read_cmis_page(ctx, 0, 0x1);
	if (!page_01) {
		fprintf(stderr, "Error: cannot read Page 01h\n");
		return -1;
	}

	cdb_adver = page_01[CMIS_CDB_ADVER_OFFSET];
	instances = (cdb_adver & CMIS_CDB_ADVER_INSTANCES_MASK) >> 6;

	if (instances == 0) {
		if (!is_json_context())
			printf("CDB not supported by this module.\n");
		return 0;
	}

	if (is_json_context())
		open_json_object("cdb_pm");
	else {
		printf("CDB Performance Monitoring\n");
		printf("==========================\n");
	}

	cdb_show_pm_features(ctx, &host_feat, &media_feat);
	cdb_show_module_pm(ctx);

	if (host_feat)
		cdb_show_host_pm(ctx, host_feat);

	cdb_show_media_pm(ctx, media_feat);
	cdb_show_dp_pm(ctx);

	if (is_json_context())
		close_json_object();

	return 0;
}

/*----------------------------------------------------------------------
 * CDB Feature Discovery (Section 9.4)
 *----------------------------------------------------------------------*/

/*
 * Query CDB Module Features (CMD 0040h) — OIF-CMIS-05.3 Table 9-8.
 *
 * Reply format (LPL):
 *   Bytes 0-1:  Reserved
 *   Bytes 2-33: CDB command support bitmap (256 bits)
 *     Byte K (K=2..33) covers CMDs ((K-2)*8) to ((K-2)*8+7)
 *     Bit B of byte K indicates CMD ((K-2)*8+B) is supported
 *   Bytes 34-35: MaxCompletionTime (U16 ms)
 */
void
cmis_cdb_show_module_features(struct cmd_context *ctx)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int i, bit;
	int count = 0;
	char cmd_str[16];

	if (is_json_context())
		open_json_object("module_features");
	else
		printf("\nCDB Module Features (CMD 0040h):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_MODULE_FEATURES,
				  reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context())
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
	else
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

	if (rpl_len < 34) {
		if (!is_json_context()) {
			printf("  Reply too short (%d bytes, expected >= 34)\n",
			       rpl_len);
			if (rpl_len > 0) {
				printf("  Raw reply:");
				for (i = 0; i < rpl_len; i++)
					printf(" %02x", reply[i]);
				printf("\n");
			}
		}
		if (is_json_context())
			close_json_object();
		return;
	}

	if (rpl_len >= 36) {
		__u16 max_time = (reply[34] << 8) | reply[35];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_completion_time_ms",
				   "%u", max_time);
		else
			printf("  Max completion time: %u ms\n", max_time);
	}

	if (is_json_context())
		open_json_array("supported_commands", NULL);
	else
		printf("  Supported standard CDB commands:\n");

	for (i = 0; i < 32; i++) {
		for (bit = 0; bit < 8; bit++) {
			if (reply[2 + i] & (1 << bit)) {
				__u16 cmd_id = i * 8 + bit;

				if (is_json_context()) {
					open_json_object(NULL);
					snprintf(cmd_str, sizeof(cmd_str),
						 "0x%04X", cmd_id);
					print_string(PRINT_JSON, "id", "%s",
						     cmd_str);
					print_string(PRINT_JSON, "name", "%s",
						     cmis_cdb_cmd_name(cmd_id));
					close_json_object();
				} else {
					printf("    CMD 0x%04X  %s\n",
					       cmd_id,
					       cmis_cdb_cmd_name(cmd_id));
				}
				count++;
			}
		}
	}

	if (is_json_context()) {
		close_json_array(NULL);
		print_int(PRINT_JSON, "command_count", "%d", count);
		close_json_object();
	} else {
		printf("  Total: %d standard commands supported\n", count);

		printf("  Support bitmap (bytes 2-33):");
		for (i = 0; i < 32; i++) {
			if (i % 16 == 0)
				printf("\n    %04x:", i * 8);
			printf(" %02x", reply[2 + i]);
		}
		printf("\n");
	}
}

/*
 * Query CDB Firmware Management Features (CMD 0041h).
 *
 * Reply format (LPL) — OIF-CMIS-05.3 Table 9-10:
 *   Byte 0:    Reserved
 *   Byte 1:    Flags (bits 0-4: Readback, SkipErased, Copy, Abort, MaxDurCoding)
 *   Byte 2:    StartCmdPayloadSize
 *   Byte 3:    ErasedByte
 *   Bytes 4-5: MaxImageSize (U16, units of 4 KB)
 *   Bytes 6-7: BlockSize (U16, bytes)
 *   Byte 8-9:  Reserved
 *   Byte 10:   ReadWriteLengthExtension
 *   Byte 11:   WriteMechanism (0x01=LPL, 0x10=EPL, 0x11=both)
 *   Byte 12:   ReadMechanism (same encoding)
 *   Byte 13:   HitlessRestart
 *   Bytes 14-15: MaxDurationStart (U16, M*ms)
 *   Bytes 16-17: MaxDurationAbort (U16, M*ms)
 *   Bytes 18-19: MaxDurationWrite (U16, M*ms)
 *   Bytes 20-21: MaxDurationComplete (U16, M*ms)
 *   Bytes 22-23: MaxDurationCopy (U16, M*ms)
 */
void
cmis_cdb_show_fw_mgmt_features(struct cmd_context *ctx)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int dur_mult = 1;

	if (is_json_context())
		open_json_object("fw_mgmt_features");
	else
		printf("\nCDB Firmware Management Features (CMD 0041h):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_FW_MGMT_FEATURES,
				  reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context())
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
	else
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

	if (rpl_len >= 2) {
		dur_mult = (reply[1] & 0x08) ? 10 : 1;

		if (is_json_context()) {
			print_bool(PRINT_JSON, "image_readback", NULL,
				   !!(reply[1] & 0x01));
			print_bool(PRINT_JSON, "skip_erased_blocks", NULL,
				   !!(reply[1] & 0x04));
			print_bool(PRINT_JSON, "copy_command", NULL,
				   !!(reply[1] & 0x08));
			print_bool(PRINT_JSON, "abort_command", NULL,
				   !!(reply[1] & 0x10));
		} else {
			printf("  Flags: 0x%02x\n", reply[1]);
			printf("    Image readback:       %s\n",
			       (reply[1] & 0x01) ? "supported" : "no");
			printf("    Skip erased blocks:   %s\n",
			       (reply[1] & 0x04) ? "supported" : "no");
			printf("    Copy command:         %s\n",
			       (reply[1] & 0x08) ? "supported" : "no");
			printf("    Abort command:        %s\n",
			       (reply[1] & 0x10) ? "supported" : "no");
			printf("    MaxDuration coding:   M=%d\n", dur_mult);
		}
	}
	if (rpl_len >= 3) {
		if (is_json_context())
			print_uint(PRINT_JSON, "start_payload_size_bytes",
				   "%u", reply[2]);
		else
			printf("  Start CMD payload size: %u bytes\n",
			       reply[2]);
	}
	if (rpl_len >= 4) {
		if (is_json_context())
			print_hhu(PRINT_JSON, "erased_byte_value",
				  "0x%02x", reply[3]);
		else
			printf("  Erased byte value: 0x%02x\n", reply[3]);
	}
	if (rpl_len >= 6) {
		__u16 max_img = (reply[4] << 8) | reply[5];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_image_size_kb",
				   "%u", max_img * 4);
		else
			printf("  Max image size: %u x 4 KB = %u KB\n",
			       max_img, max_img * 4);
	}
	if (rpl_len >= 8) {
		__u16 blk_size = (reply[6] << 8) | reply[7];

		if (is_json_context())
			print_uint(PRINT_JSON, "block_size_bytes",
				   "%u", (unsigned int)blk_size);
		else
			printf("  Block size: %u bytes\n", blk_size);
	}
	if (rpl_len >= 11) {
		if (is_json_context())
			print_uint(PRINT_JSON, "rw_length_extension",
				   "%u", reply[10]);
		else
			printf("  Read/Write length ext:  %u\n", reply[10]);
	}
	if (rpl_len >= 12) {
		__u8 wm = reply[11];

		if (is_json_context()) {
			print_bool(PRINT_JSON, "write_lpl", NULL,
				   !!(wm & 0x01));
			print_bool(PRINT_JSON, "write_epl", NULL,
				   !!(wm & 0x10));
		} else {
			printf("  Write mechanism:        0x%02x (%s%s%s)\n",
			       wm,
			       (wm & 0x01) ? "LPL" : "",
			       (wm == 0x11) ? "+" : "",
			       (wm & 0x10) ? "EPL" : "");
		}
	}
	if (rpl_len >= 13) {
		__u8 rm = reply[12];

		if (is_json_context()) {
			print_bool(PRINT_JSON, "read_lpl", NULL,
				   !!(rm & 0x01));
			print_bool(PRINT_JSON, "read_epl", NULL,
				   !!(rm & 0x10));
		} else {
			printf("  Read mechanism:         0x%02x (%s%s%s)\n",
			       rm,
			       (rm & 0x01) ? "LPL" : "",
			       (rm == 0x11) ? "+" : "",
			       (rm & 0x10) ? "EPL" : "");
		}
	}
	if (rpl_len >= 14) {
		if (is_json_context())
			print_bool(PRINT_JSON, "hitless_restart", NULL,
				   !!reply[13]);
		else
			printf("  Hitless restart:        %s\n",
			       reply[13] ? "supported" : "no");
	}
	if (rpl_len >= 16) {
		__u16 dur = (reply[14] << 8) | reply[15];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_duration_start_ms",
				   "%u", dur * dur_mult);
		else
			printf("  Max duration start:     %u ms\n",
			       dur * dur_mult);
	}
	if (rpl_len >= 18) {
		__u16 dur = (reply[16] << 8) | reply[17];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_duration_abort_ms",
				   "%u", dur * dur_mult);
		else
			printf("  Max duration abort:     %u ms\n",
			       dur * dur_mult);
	}
	if (rpl_len >= 20) {
		__u16 dur = (reply[18] << 8) | reply[19];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_duration_write_ms",
				   "%u", dur * dur_mult);
		else
			printf("  Max duration write:     %u ms\n",
			       dur * dur_mult);
	}
	if (rpl_len >= 22) {
		__u16 dur = (reply[20] << 8) | reply[21];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_duration_complete_ms",
				   "%u", dur * dur_mult);
		else
			printf("  Max duration complete:  %u ms\n",
			       dur * dur_mult);
	}
	if (rpl_len >= 24) {
		__u16 dur = (reply[22] << 8) | reply[23];

		if (is_json_context())
			print_uint(PRINT_JSON, "max_duration_copy_ms",
				   "%u", dur * dur_mult);
		else
			printf("  Max duration copy:      %u ms\n",
			       dur * dur_mult);
	}

	if (is_json_context()) {
		close_json_object();
	} else if (rpl_len > 0) {
		int i;

		printf("  Raw reply:");
		for (i = 0; i < rpl_len; i++) {
			if (i % 16 == 0)
				printf("\n    ");
			printf(" %02x", reply[i]);
		}
		printf("\n");
	}
}

/*
 * Get Firmware Info (CMD 0100h) - OIF-CMIS-05.3 Table 9-19.
 * Returns firmware image A/B status, versions, and optional factory/boot info.
 */
/* Helper to emit a firmware image JSON object */
static void
fw_info_json_image(const char *key, const __u8 *reply, int ver_off,
		   int extra_off, __u8 rpl_len, bool running,
		   bool committed, bool valid)
{
	char ver[32];
	char extra[33];

	open_json_object(key);
	print_bool(PRINT_JSON, "running", NULL, running);
	print_bool(PRINT_JSON, "committed", NULL, committed);
	print_bool(PRINT_JSON, "valid", NULL, valid);

	if (rpl_len >= ver_off + 4) {
		__u16 build = (reply[ver_off + 2] << 8) |
			       reply[ver_off + 3];

		snprintf(ver, sizeof(ver), "%u.%u.%u",
			 reply[ver_off], reply[ver_off + 1], build);
		print_string(PRINT_JSON, "version", "%s", ver);
	}
	if (rpl_len >= extra_off + 32) {
		memcpy(extra, &reply[extra_off], 32);
		extra[32] = '\0';
		for (int i = 31; i >= 0 && (extra[i] == ' ' ||
		     extra[i] == '\0'); i--)
			extra[i] = '\0';
		if (extra[0])
			print_string(PRINT_JSON, "extra", "%s", extra);
	}
	close_json_object();
}

void
cmis_cdb_show_fw_info(struct cmd_context *ctx)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	__u8 fw_status, img_info;
	char extra[33];

	if (is_json_context())
		open_json_object("fw_info");
	else
		printf("\nCDB Get Firmware Info (CMD 0x0100h):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_GET_FW_INFO,
				  reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context())
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
	else
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

	if (rpl_len < 2) {
		if (!is_json_context())
			printf("  Reply too short (%d bytes)\n", rpl_len);
		if (is_json_context())
			close_json_object();
		return;
	}

	fw_status = reply[0];
	img_info = reply[1];

	if (is_json_context()) {
		char hex_str[8];

		snprintf(hex_str, sizeof(hex_str), "0x%02x", fw_status);
		print_string(PRINT_JSON, "fw_status_byte", "%s", hex_str);

		if (img_info & 0x01)
			fw_info_json_image("image_a", reply, 2, 6, rpl_len,
					   !!(fw_status & 0x01),
					   !!(fw_status & 0x02),
					   !(fw_status & 0x04));
		if (img_info & 0x02)
			fw_info_json_image("image_b", reply, 38, 42, rpl_len,
					   !!(fw_status & 0x10),
					   !!(fw_status & 0x20),
					   !(fw_status & 0x40));
		if (img_info & 0x04)
			fw_info_json_image("factory_boot", reply, 74, 78,
					   rpl_len, false, false, true);

		close_json_object();
		return;
	}

	/* Text output — unchanged */
	printf("  Firmware Status: 0x%02x\n", fw_status);
	printf("    Image A: %s, %s, %s\n",
	       (fw_status & 0x01) ? "running" : "not running",
	       (fw_status & 0x02) ? "committed" : "uncommitted",
	       (fw_status & 0x04) ? "invalid" : "valid");
	printf("    Image B: %s, %s, %s\n",
	       (fw_status & 0x10) ? "running" : "not running",
	       (fw_status & 0x20) ? "committed" : "uncommitted",
	       (fw_status & 0x40) ? "invalid" : "valid");

	printf("  Image Information: 0x%02x\n", img_info);
	printf("    Image A present:       %s\n",
	       (img_info & 0x01) ? "yes" : "no");
	printf("    Image B present:       %s\n",
	       (img_info & 0x02) ? "yes" : "no");
	printf("    Factory/Boot present:  %s\n",
	       (img_info & 0x04) ? "yes" : "no");

	if ((img_info & 0x01) && rpl_len >= 6) {
		__u16 build = (reply[4] << 8) | reply[5];

		printf("  Image A version: %u.%u.%u\n",
		       reply[2], reply[3], build);
		if (rpl_len >= 38) {
			memcpy(extra, &reply[6], 32);
			extra[32] = '\0';
			for (int i = 31; i >= 0 && (extra[i] == ' ' ||
			     extra[i] == '\0'); i--)
				extra[i] = '\0';
			if (extra[0])
				printf("    Extra: %s\n", extra);
		}
	}

	if ((img_info & 0x02) && rpl_len >= 42) {
		__u16 build = (reply[40] << 8) | reply[41];

		printf("  Image B version: %u.%u.%u\n",
		       reply[38], reply[39], build);
		if (rpl_len >= 74) {
			memcpy(extra, &reply[42], 32);
			extra[32] = '\0';
			for (int i = 31; i >= 0 && (extra[i] == ' ' ||
			     extra[i] == '\0'); i--)
				extra[i] = '\0';
			if (extra[0])
				printf("    Extra: %s\n", extra);
		}
	}

	if ((img_info & 0x04) && rpl_len >= 78) {
		__u16 build = (reply[76] << 8) | reply[77];

		printf("  Factory/Boot version: %u.%u.%u\n",
		       reply[74], reply[75], build);
		if (rpl_len >= 110) {
			memcpy(extra, &reply[78], 32);
			extra[32] = '\0';
			for (int i = 31; i >= 0 && (extra[i] == ' ' ||
			     extra[i] == '\0'); i--)
				extra[i] = '\0';
			if (extra[0])
				printf("    Extra: %s\n", extra);
		}
	} else if (!(img_info & 0x04)) {
		printf("  Factory/Boot: not present\n");
	}

	if (rpl_len > 0) {
		int i;

		printf("  Raw reply:");
		for (i = 0; i < rpl_len; i++) {
			if (i % 16 == 0)
				printf("\n    ");
			printf(" %02x", reply[i]);
		}
		printf("\n");
	}
}

/*
 * Generic feature query for CMD 0042h-0045h.
 * These commands return a similar format to CMD 0040h (support bitmap).
 */
void
cmis_cdb_show_generic_features(struct cmd_context *ctx, __u16 cmd_id,
			       const char *name)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int i;
	char json_key[100];

	if (is_json_context()) {
		convert_json_field_name(name, json_key);
		open_json_object(json_key);
	} else {
		printf("\nCDB %s (CMD 0x%04Xh):\n", name, cmd_id);
	}

	status = cmis_cdb_send_command(ctx, cmd_id, reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context()) {
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
		if (rpl_len > 0) {
			open_json_array("raw_data", NULL);
			for (i = 0; i < rpl_len; i++)
				print_uint(PRINT_JSON, NULL, "%u", reply[i]);
			close_json_array(NULL);
		}
		close_json_object();
	} else {
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

		if (rpl_len > 0) {
			printf("  Raw reply:");
			for (i = 0; i < rpl_len; i++) {
				if (i % 16 == 0)
					printf("\n    ");
				printf(" %02x", reply[i]);
			}
			printf("\n");
		}
	}
}

/*----------------------------------------------------------------------
 * BERT and Diagnostics Features (CMD 0x0043) — OIF-CMIS-05.3 Table 9-13
 *----------------------------------------------------------------------*/

void
cmis_cdb_show_bert_features(struct cmd_context *ctx)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int i, bit, count = 0;
	char cmd_str[16];

	if (is_json_context())
		open_json_object("bert_features");
	else
		printf("\nCDB BERT and Diagnostics Features (CMD 0x0043h):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_BERT_FEATURES,
				       reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context())
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
	else
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

	if (rpl_len >= 32) {
		if (is_json_context())
			open_json_array("supported_commands", NULL);
		else
			printf("  Supported BERT/Diagnostics commands (0x0300-0x03FF):\n");

		for (i = 0; i < 32; i++) {
			for (bit = 0; bit < 8; bit++) {
				if (reply[i] & (1 << bit)) {
					__u16 cmd_id = 0x0300 + i * 8 + bit;

					if (is_json_context()) {
						open_json_object(NULL);
						snprintf(cmd_str,
							 sizeof(cmd_str),
							 "0x%04X", cmd_id);
						print_string(PRINT_JSON, "id",
							     "%s", cmd_str);
						print_string(PRINT_JSON,
							     "name", "%s",
							     cmis_cdb_cmd_name(cmd_id));
						close_json_object();
					} else {
						printf("    CMD 0x%04X  %s\n",
						       cmd_id,
						       cmis_cdb_cmd_name(cmd_id));
					}
					count++;
				}
			}
		}

		if (is_json_context()) {
			close_json_array(NULL);
		} else {
			if (count == 0)
				printf("    (none)\n");
			else
				printf("  Total: %d BERT commands supported\n",
				       count);
		}
	}

	if (is_json_context()) {
		print_int(PRINT_JSON, "command_count", "%d", count);
		close_json_object();
	} else if (rpl_len > 0) {
		printf("  Raw reply:");
		for (i = 0; i < rpl_len; i++) {
			if (i % 16 == 0)
				printf("\n    ");
			printf(" %02x", reply[i]);
		}
		printf("\n");
	}
}

/*----------------------------------------------------------------------
 * Security Features (CMD 0x0044) — OIF-CMIS-05.3 Table 9-14
 *----------------------------------------------------------------------*/

void
cmis_cdb_show_security_features(struct cmd_context *ctx)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;
	int i, bit, count = 0;
	char cmd_str[16];

	if (is_json_context())
		open_json_object("security_features");
	else
		printf("\nCDB Security Features (CMD 0x0044h):\n");

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_SECURITY_FEATURES,
				       reply, &rpl_len);
	if (status < 0) {
		fprintf(stderr, "  Failed: I2C error or timeout\n");
		if (is_json_context())
			close_json_object();
		return;
	}
	if (status & CMIS_CDB_STATUS_FAIL) {
		if (is_json_context())
			print_string(PRINT_JSON, "error", "%s",
				     cmis_cdb_status_str(status));
		else
			printf("  Failed: %s (status 0x%02x)\n",
			       cmis_cdb_status_str(status), status);
		if (is_json_context())
			close_json_object();
		return;
	}

	if (is_json_context())
		print_string(PRINT_JSON, "status", "%s",
			     cmis_cdb_status_str(status));
	else
		printf("  Status: %s (reply %d bytes)\n",
		       cmis_cdb_status_str(status), rpl_len);

	if (rpl_len >= 1) {
		if (is_json_context())
			open_json_array("supported_commands", NULL);
		else
			printf("  Supported Security commands (0x0400-0x04FF):\n");

		for (bit = 0; bit < 8; bit++) {
			if (reply[0] & (1 << bit)) {
				__u16 cmd_id = 0x0400 + bit;

				if (is_json_context()) {
					open_json_object(NULL);
					snprintf(cmd_str, sizeof(cmd_str),
						 "0x%04X", cmd_id);
					print_string(PRINT_JSON, "id", "%s",
						     cmd_str);
					print_string(PRINT_JSON, "name", "%s",
						     cmis_cdb_cmd_name(cmd_id));
					close_json_object();
				} else {
					printf("    CMD 0x%04X  %s\n",
					       cmd_id,
					       cmis_cdb_cmd_name(cmd_id));
				}
				count++;
			}
		}
		for (i = 1; i < 32 && i < rpl_len; i++) {
			for (bit = 0; bit < 8; bit++) {
				if (reply[i] & (1 << bit)) {
					__u16 cmd_id = 0x0400 + i * 8 + bit;

					if (is_json_context()) {
						open_json_object(NULL);
						snprintf(cmd_str,
							 sizeof(cmd_str),
							 "0x%04X", cmd_id);
						print_string(PRINT_JSON, "id",
							     "%s", cmd_str);
						print_string(PRINT_JSON,
							     "name", "%s",
							     cmis_cdb_cmd_name(cmd_id));
						close_json_object();
					} else {
						printf("    CMD 0x%04X  %s\n",
						       cmd_id,
						       cmis_cdb_cmd_name(cmd_id));
					}
					count++;
				}
			}
		}

		if (is_json_context()) {
			close_json_array(NULL);
		} else {
			if (count == 0)
				printf("    (none)\n");
			else
				printf("  Total: %d security commands supported\n",
				       count);
		}
	}

	if (is_json_context()) {
		print_int(PRINT_JSON, "command_count", "%d", count);
		if (rpl_len >= 33)
			print_uint(PRINT_JSON, "num_certificates", "%u",
				   reply[32]);
		if (rpl_len >= 34)
			print_bool(PRINT_JSON, "certificate_chain", NULL,
				   !!reply[33]);
		if (rpl_len >= 35) {
			const char *fmt;

			switch (reply[34]) {
			case 0: fmt = "DER (X.509)"; break;
			case 1: fmt = "PEM"; break;
			default: fmt = "unknown"; break;
			}
			print_string(PRINT_JSON, "certificate_format", "%s",
				     fmt);
		}
		if (rpl_len >= 38) {
			int nc = reply[32];
			int j;

			if (nc > 4)
				nc = 4;
			open_json_array("certificate_lengths", NULL);
			for (j = 0; j < nc && rpl_len >= 38 + j * 2; j++) {
				__u16 clen = (reply[36 + j * 2] << 8) |
					      reply[37 + j * 2];
				print_uint(PRINT_JSON, NULL, "%u",
					   (unsigned int)clen);
			}
			close_json_array(NULL);
		}
		close_json_object();
	} else {
		if (rpl_len >= 33)
			printf("  Number of certificates: %u\n", reply[32]);
		if (rpl_len >= 34)
			printf("  Certificate chain:      %s\n",
			       reply[33] ? "supported" : "no");
		if (rpl_len >= 35) {
			const char *fmt;

			switch (reply[34]) {
			case 0: fmt = "DER (X.509)"; break;
			case 1: fmt = "PEM"; break;
			default: fmt = "unknown"; break;
			}
			printf("  Certificate format:     %s (0x%02x)\n",
			       fmt, reply[34]);
		}
		if (rpl_len >= 38) {
			int nc = rpl_len >= 33 ? reply[32] : 0;
			int j;

			if (nc > 4)
				nc = 4;
			for (j = 0; j < nc && rpl_len >= 38 + j * 2; j++) {
				__u16 clen = (reply[36 + j * 2] << 8) |
					      reply[37 + j * 2];
				printf("  Certificate %d length:   %u bytes\n",
				       j, clen);
			}
		}

		if (rpl_len > 0) {
			printf("  Raw reply:");
			for (i = 0; i < rpl_len; i++) {
				if (i % 16 == 0)
					printf("\n    ");
				printf(" %02x", reply[i]);
			}
			printf("\n");
		}
	}
}

/*----------------------------------------------------------------------
 * Firmware Update (CMD 0x0101-0x010A) — OIF-CMIS-05.3 Section 9.7
 *----------------------------------------------------------------------*/

/* FW management capabilities, populated from CMD 0x0041 reply */
struct cmis_fw_caps {
	__u8  start_payload_size;
	__u8  erased_byte;
	__u8  write_mechanism;
	__u8  read_mechanism;
	__u8  hitless_restart;
	__u8  abort_supported;
	__u8  copy_supported;
	__u16 max_dur_start_ms;
	__u16 max_dur_write_ms;
	__u16 max_dur_complete_ms;
	int   dur_multiplier;
	int   max_lpl_write;
};

/* Query CMD 0x0041 and populate caps struct */
static int
cdb_fw_get_caps(struct cmd_context *ctx, struct cmis_fw_caps *caps)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;

	memset(caps, 0, sizeof(*caps));
	caps->dur_multiplier = 1;
	caps->max_lpl_write = 116;  /* Default LPL payload max */

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_FW_MGMT_FEATURES,
				       reply, &rpl_len);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL))
		return -1;

	if (rpl_len < 2)
		return -1;

	caps->abort_supported = (reply[1] & 0x10) ? 1 : 0;
	caps->copy_supported = (reply[1] & 0x08) ? 1 : 0;
	caps->dur_multiplier = (reply[1] & 0x08) ? 10 : 1;

	if (rpl_len >= 3)
		caps->start_payload_size = reply[2];
	if (rpl_len >= 4)
		caps->erased_byte = reply[3];
	if (rpl_len >= 12)
		caps->write_mechanism = reply[11];
	if (rpl_len >= 13)
		caps->read_mechanism = reply[12];
	if (rpl_len >= 14)
		caps->hitless_restart = reply[13];
	if (rpl_len >= 16)
		caps->max_dur_start_ms =
			((reply[14] << 8) | reply[15]) * caps->dur_multiplier;
	if (rpl_len >= 20)
		caps->max_dur_write_ms =
			((reply[18] << 8) | reply[19]) * caps->dur_multiplier;
	if (rpl_len >= 22)
		caps->max_dur_complete_ms =
			((reply[20] << 8) | reply[21]) * caps->dur_multiplier;

	return 0;
}

/* CMD 0x0101: Start firmware download */
static int
cdb_fw_start(struct cmd_context *ctx, const struct cmis_fw_caps *caps,
	     const __u8 *image, __u32 image_size)
{
	__u8 lpl[120];
	int lpl_len;
	int status;

	memset(lpl, 0, sizeof(lpl));

	/* Bytes 0-3: image size (U32 big-endian) */
	lpl[0] = (image_size >> 24) & 0xFF;
	lpl[1] = (image_size >> 16) & 0xFF;
	lpl[2] = (image_size >> 8) & 0xFF;
	lpl[3] = image_size & 0xFF;

	/* Bytes 4+: vendor-specific header from start of image */
	lpl_len = 4;
	if (caps->start_payload_size > 0 && image_size > 0) {
		int copy_len = caps->start_payload_size;

		if (copy_len > 116)
			copy_len = 116;
		if (copy_len > (int)image_size)
			copy_len = (int)image_size;
		memcpy(lpl + 4, image, copy_len);
		lpl_len += copy_len;
	}

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_START_FW_DOWNLOAD,
					   lpl, lpl_len, NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW start failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	return 0;
}

/* CMD 0x0103: Write one LPL block (up to 116 bytes) */
static int
cdb_fw_write_block(struct cmd_context *ctx, __u32 block_addr,
		   const __u8 *data, __u8 len)
{
	__u8 lpl[120];
	int status;

	if (len > 116)
		len = 116;

	/* Bytes 0-3: block address (U32 big-endian) */
	lpl[0] = (block_addr >> 24) & 0xFF;
	lpl[1] = (block_addr >> 16) & 0xFF;
	lpl[2] = (block_addr >> 8) & 0xFF;
	lpl[3] = block_addr & 0xFF;

	memcpy(lpl + 4, data, len);

	status = cmis_cdb_send_command_lpl(ctx,
					   CMIS_CDB_CMD_WRITE_FW_BLOCK_LPL,
					   lpl, 4 + len, NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW write block at 0x%x failed: %s (0x%02x)\n",
			block_addr, cmis_cdb_status_str(status), status);
		return -1;
	}
	return 0;
}

/* CMD 0x0107: Complete and validate firmware download */
static int
cdb_fw_complete(struct cmd_context *ctx)
{
	int status;

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_COMPLETE_FW_DOWNLOAD,
				       NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW complete failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	return 0;
}

int
cmis_cdb_fw_download(struct cmd_context *ctx, const char *filepath)
{
	FILE *fp;
	__u8 *image = NULL;
	long file_size;
	struct cmis_fw_caps caps;
	__u32 offset, block_size;
	int ret = -1;
	int progress, last_progress = -1;

	/* Read entire file into memory */
	fp = fopen(filepath, "rb");
	if (!fp) {
		fprintf(stderr, "Cannot open firmware file: %s\n", filepath);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (file_size <= 0 || file_size > 16 * 1024 * 1024) {
		fprintf(stderr, "Invalid firmware file size: %ld bytes\n",
			file_size);
		fclose(fp);
		return -1;
	}

	image = malloc(file_size);
	if (!image) {
		fprintf(stderr, "Out of memory for %ld bytes\n", file_size);
		fclose(fp);
		return -1;
	}

	if (fread(image, 1, file_size, fp) != (size_t)file_size) {
		fprintf(stderr, "Failed to read firmware file\n");
		goto out;
	}
	fclose(fp);
	fp = NULL;

	/* Query FW management capabilities */
	if (cdb_fw_get_caps(ctx, &caps) < 0) {
		fprintf(stderr, "Cannot query firmware management features\n");
		goto out;
	}

	if (!(caps.write_mechanism & 0x01)) {
		fprintf(stderr, "Module does not support LPL firmware write\n");
		goto out;
	}

	/* Show current FW info */
	if (!is_json_context()) {
		printf("Current firmware state:\n");
		cmis_cdb_show_fw_info(ctx);

		printf("\nFirmware file: %s (%ld bytes)\n",
		       filepath, file_size);
		printf("Write mechanism: LPL, block size: 116 bytes\n");
	}

	/* Start download */
	if (!is_json_context())
		printf("Starting firmware download...\n");
	if (cdb_fw_start(ctx, &caps, image, (__u32)file_size) < 0)
		goto out;

	/* Write blocks */
	block_size = 116;
	for (offset = 0; offset < (__u32)file_size; offset += block_size) {
		__u8 len = ((__u32)file_size - offset > block_size)
			   ? block_size : (__u32)file_size - offset;

		if (cdb_fw_write_block(ctx, offset, image + offset, len) < 0) {
			fprintf(stderr, "Download failed at offset 0x%x\n",
				offset);
			goto out;
		}

		progress = (int)(((__u64)offset + len) * 100 / file_size);
		if (!is_json_context() &&
		    progress / 10 != last_progress / 10) {
			printf("  Progress: %d%%\n", progress);
			last_progress = progress;
		}
	}

	/* Complete download */
	if (!is_json_context())
		printf("Completing firmware download...\n");
	if (cdb_fw_complete(ctx) < 0)
		goto out;

	if (!is_json_context()) {
		/* Show updated FW info */
		printf("\nUpdated firmware state:\n");
		cmis_cdb_show_fw_info(ctx);

		printf("\nDownload complete. Use --fw-run to activate.\n");
	}
	ret = 0;

out:
	free(image);
	if (fp)
		fclose(fp);
	return ret;
}

int
cmis_cdb_fw_run(struct cmd_context *ctx, __u8 mode)
{
	__u8 lpl[4];
	int status;

	lpl[0] = mode;

	if (!is_json_context())
		printf("Running firmware image (mode=%u)...\n", mode);
	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_RUN_FW_IMAGE,
					   lpl, 1, NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW run failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	if (!is_json_context()) {
		printf("FW run command sent successfully\n");
		cmis_cdb_show_fw_info(ctx);
	}
	return 0;
}

int
cmis_cdb_fw_commit(struct cmd_context *ctx)
{
	int status;

	if (!is_json_context())
		printf("Committing running firmware image...\n");
	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_COMMIT_FW_IMAGE,
				       NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW commit failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	if (!is_json_context()) {
		printf("FW commit successful\n");
		cmis_cdb_show_fw_info(ctx);
	}
	return 0;
}

int
cmis_cdb_fw_copy(struct cmd_context *ctx, __u8 direction)
{
	__u8 lpl[4];
	int status;

	lpl[0] = direction;

	if (!is_json_context())
		printf("Copying firmware image (direction=0x%02x)...\n",
		       direction);
	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_COPY_FW_IMAGE,
					   lpl, 1, NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW copy failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	if (!is_json_context()) {
		printf("FW copy successful\n");
		cmis_cdb_show_fw_info(ctx);
	}
	return 0;
}

int
cmis_cdb_fw_abort(struct cmd_context *ctx)
{
	int status;

	if (!is_json_context())
		printf("Aborting firmware download...\n");
	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_ABORT_FW_DOWNLOAD,
				       NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "FW abort failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	if (!is_json_context())
		printf("FW download aborted\n");
	return 0;
}

/*----------------------------------------------------------------------
 * Security / IDevID (CMD 0x0400-0x0405) — OIF-CMIS-05.3 Section 9.11
 *----------------------------------------------------------------------*/

int
cmis_cdb_get_certificate(struct cmd_context *ctx, __u8 cert_index,
			 __u8 *cert_buf, int buf_size, int *cert_len)
{
	__u8 lpl[4];
	__u8 reply[120];
	__u8 rpl_len;
	int status;
	int total = 0;
	__u8 segment = 0;

	*cert_len = 0;

	while (total < buf_size) {
		memset(lpl, 0, sizeof(lpl));
		lpl[0] = cert_index;
		lpl[1] = segment;

		rpl_len = 0;
		status = cmis_cdb_send_command_lpl(ctx,
						   CMIS_CDB_CMD_GET_CERT_LPL,
						   lpl, 2, reply, &rpl_len);
		if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
			if (segment > 0 && total > 0)
				break;  /* May be end of cert */
			fprintf(stderr,
				"Get certificate %d seg %d failed: %s\n",
				cert_index, segment,
				cmis_cdb_status_str(status));
			return -1;
		}

		if (rpl_len == 0)
			break;

		if (total + rpl_len > buf_size)
			rpl_len = buf_size - total;

		memcpy(cert_buf + total, reply, rpl_len);
		total += rpl_len;

		/* If reply is less than max LPL, we got the last segment */
		if (rpl_len < 120)
			break;

		segment++;
	}

	*cert_len = total;
	return 0;
}

void
cmis_cdb_show_certificates(struct cmd_context *ctx)
{
	__u8 sec_reply[120];
	__u8 rpl_len = 0;
	int status;
	int num_certs, j;
	const char *cert_fmt;

	/* Query security features to discover certificate count */
	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_SECURITY_FEATURES,
				       sec_reply, &rpl_len);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "Cannot query security features\n");
		return;
	}

	num_certs = (rpl_len >= 33) ? sec_reply[32] : 0;
	cert_fmt = (rpl_len >= 35) ?
		   (sec_reply[34] == 0 ? "DER (X.509)" :
		    sec_reply[34] == 1 ? "PEM" : "unknown") : "unknown";

	if (is_json_context()) {
		open_json_object("certificates");
		print_int(PRINT_JSON, "count", "%d", num_certs);
		print_string(PRINT_JSON, "format", "%s", cert_fmt);

		if (num_certs == 0) {
			close_json_object();
			return;
		}

		open_json_array("items", NULL);
		for (j = 0; j < num_certs && j < 4; j++) {
			__u8 cert_buf[4096];
			int cert_len = 0;
			int ret, i;

			open_json_object(NULL);
			print_int(PRINT_JSON, "index", "%d", j);
			print_string(PRINT_JSON, "type", "%s",
				     j == 0 ? "leaf" : "chain");

			ret = cmis_cdb_get_certificate(ctx, j, cert_buf,
						       sizeof(cert_buf),
						       &cert_len);
			if (ret < 0) {
				print_string(PRINT_JSON, "error", "%s",
					     "failed to retrieve");
				close_json_object();
				continue;
			}

			print_int(PRINT_JSON, "length", "%d", cert_len);
			if (cert_len > 0) {
				open_json_array("data", NULL);
				for (i = 0; i < cert_len; i++)
					print_uint(PRINT_JSON, NULL, "%u",
						   cert_buf[i]);
				close_json_array(NULL);
			}
			close_json_object();
		}
		close_json_array(NULL);
		close_json_object();
		return;
	}

	if (num_certs == 0) {
		printf("No certificates available\n");
		return;
	}

	printf("Module reports %d certificate(s)\n", num_certs);
	if (rpl_len >= 35)
		printf("  Certificate format: %s\n", cert_fmt);

	for (j = 0; j < num_certs && j < 4; j++) {
		__u8 cert_buf[4096];
		int cert_len = 0;
		int ret, i;

		printf("\nCertificate %d (%s):\n", j,
		       j == 0 ? "leaf" : "chain");

		ret = cmis_cdb_get_certificate(ctx, j, cert_buf,
					       sizeof(cert_buf), &cert_len);
		if (ret < 0) {
			printf("  Failed to retrieve\n");
			continue;
		}

		printf("  Length: %d bytes\n", cert_len);
		if (cert_len > 0) {
			printf("  Data:");
			for (i = 0; i < cert_len; i++) {
				if (i % 16 == 0)
					printf("\n    %04x:", i);
				printf(" %02x", cert_buf[i]);
			}
			printf("\n");
		}
	}
}

int
cmis_cdb_set_digest(struct cmd_context *ctx, const __u8 *digest, __u8 len)
{
	int status;

	if (len > 120)
		len = 120;

	status = cmis_cdb_send_command_lpl(ctx, CMIS_CDB_CMD_SET_DIGEST_LPL,
					   digest, len, NULL, NULL);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "Set digest failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}
	return 0;
}

int
cmis_cdb_get_signature(struct cmd_context *ctx, __u8 *sig_buf,
		       int buf_size, int *sig_len)
{
	__u8 reply[120];
	__u8 rpl_len = 0;
	int status;

	*sig_len = 0;

	status = cmis_cdb_send_command(ctx, CMIS_CDB_CMD_GET_SIGNATURE_LPL,
				       reply, &rpl_len);
	if (status < 0 || (status & CMIS_CDB_STATUS_FAIL)) {
		fprintf(stderr, "Get signature failed: %s (0x%02x)\n",
			cmis_cdb_status_str(status), status);
		return -1;
	}

	if (rpl_len > buf_size)
		rpl_len = buf_size;

	memcpy(sig_buf, reply, rpl_len);
	*sig_len = rpl_len;
	return 0;
}

/*----------------------------------------------------------------------
 * CDB Feature Discovery (combined)
 *----------------------------------------------------------------------*/

int
cmis_cdb_show_features(struct cmd_context *ctx)
{
	if (is_json_context())
		open_json_object("cdb_features");

	cmis_cdb_show_module_features(ctx);
	cmis_cdb_show_fw_mgmt_features(ctx);
	cmis_cdb_show_fw_info(ctx);
	cmis_cdb_show_generic_features(ctx, CMIS_CDB_CMD_PM_FEATURES,
				       "Performance Monitoring Features");
	cmis_cdb_show_bert_features(ctx);
	cmis_cdb_show_security_features(ctx);
	cmis_cdb_show_generic_features(ctx, CMIS_CDB_CMD_EXT_FEATURES,
				       "Externally Defined Features");

	if (is_json_context())
		close_json_object();

	return 0;
}
