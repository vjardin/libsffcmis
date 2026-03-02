/*
 * xfp.h: INF-8077i Rev 4.5 offset constants for XFP modules.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 *
 * XFP uses a single I2C address (0x50) with 256-byte address space:
 *   Lower memory (0x00-0x7F): diagnostics, thresholds, flags, A/D, control
 *   Upper memory (0x80-0xFF): table-selected (Table 01h = Serial ID)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef XFP_H__
#define XFP_H__

#define XFP_PAGE_SIZE			0x80
#define XFP_I2C_ADDRESS			0x50

/*-----------------------------------------------------------------------
 * Lower Memory Map (bytes 0-127)
 *-----------------------------------------------------------------------*/

/* Byte 0: Identifier (same as upper memory byte 128) */
#define XFP_ID_OFFSET			0x00

/* Byte 1: Signal Conditioner Control */
#define XFP_SIGNAL_COND_CTRL		0x01

/* Bytes 2-57: Alarm and Warning Thresholds (Table 35) */
#define XFP_TEMP_HALRM			2
#define XFP_TEMP_LALRM			4
#define XFP_TEMP_HWARN			6
#define XFP_TEMP_LWARN			8
/* 10-17: Reserved A/D flag thresholds */
#define XFP_BIAS_HALRM			18
#define XFP_BIAS_LALRM			20
#define XFP_BIAS_HWARN			22
#define XFP_BIAS_LWARN			24
#define XFP_TX_PWR_HALRM		26
#define XFP_TX_PWR_LALRM		28
#define XFP_TX_PWR_HWARN		30
#define XFP_TX_PWR_LWARN		32
#define XFP_RX_PWR_HALRM		34
#define XFP_RX_PWR_LALRM		36
#define XFP_RX_PWR_HWARN		38
#define XFP_RX_PWR_LWARN		40
#define XFP_AUX1_HALRM			42
#define XFP_AUX1_LALRM			44
#define XFP_AUX1_HWARN			46
#define XFP_AUX1_LWARN			48
#define XFP_AUX2_HALRM			50
#define XFP_AUX2_LALRM			52
#define XFP_AUX2_HWARN			54
#define XFP_AUX2_LWARN			56

/* Bytes 80-87: Latched Interrupt Flags (Table 39) */
#define XFP_ALRM_FLG			80
/* Byte 80 bits: */
#define XFP_TEMP_HALRM_FLG		(1 << 7)
#define XFP_TEMP_LALRM_FLG		(1 << 6)
#define XFP_BIAS_HALRM_FLG		(1 << 3)
#define XFP_BIAS_LALRM_FLG		(1 << 2)
#define XFP_TX_PWR_HALRM_FLG		(1 << 1)
#define XFP_TX_PWR_LALRM_FLG		(1 << 0)
/* Byte 81 bits: */
#define XFP_RX_PWR_HALRM_FLG		(1 << 7)
#define XFP_RX_PWR_LALRM_FLG		(1 << 6)
#define XFP_AUX1_HALRM_FLG		(1 << 5)
#define XFP_AUX1_LALRM_FLG		(1 << 4)
#define XFP_AUX2_HALRM_FLG		(1 << 3)
#define XFP_AUX2_LALRM_FLG		(1 << 2)

#define XFP_WARN_FLG			82
/* Byte 82 bits: */
#define XFP_TEMP_HWARN_FLG		(1 << 7)
#define XFP_TEMP_LWARN_FLG		(1 << 6)
#define XFP_BIAS_HWARN_FLG		(1 << 3)
#define XFP_BIAS_LWARN_FLG		(1 << 2)
#define XFP_TX_PWR_HWARN_FLG		(1 << 1)
#define XFP_TX_PWR_LWARN_FLG		(1 << 0)
/* Byte 83 bits: */
#define XFP_RX_PWR_HWARN_FLG		(1 << 7)
#define XFP_RX_PWR_LWARN_FLG		(1 << 6)
#define XFP_AUX1_HWARN_FLG		(1 << 5)
#define XFP_AUX1_LWARN_FLG		(1 << 4)
#define XFP_AUX2_HWARN_FLG		(1 << 3)
#define XFP_AUX2_LWARN_FLG		(1 << 2)

/* Bytes 96-109: A/D Readout Values (Table 41) */
#define XFP_AD_TEMP			96
#define XFP_AD_RSVD			98
#define XFP_AD_BIAS			100
#define XFP_AD_TX_PWR			102
#define XFP_AD_RX_PWR			104
#define XFP_AD_AUX1			106
#define XFP_AD_AUX2			108

/* Byte 127: Page Select */
#define XFP_PAGE_SELECT			127

/*-----------------------------------------------------------------------
 * Upper Memory Table 01h - Serial ID (bytes 128-255)
 * All offsets relative to start of upper memory buffer.
 *-----------------------------------------------------------------------*/

/* Use offsets relative to a buffer read at 0x80, so byte 128 = index 0 */
#define XFP_ID_UPPER			0	/* 128 */
#define XFP_EXT_ID			1	/* 129 */
#define XFP_CONNECTOR			2	/* 130 */
#define XFP_TRANSCEIVER_START		3	/* 131 */
#define XFP_TRANSCEIVER_END		10	/* 138 */
#define XFP_ENCODING			11	/* 139 */
#define XFP_BR_MIN			12	/* 140 */
#define XFP_BR_MAX			13	/* 141 */
#define XFP_LENGTH_SMF_KM		14	/* 142 */
#define XFP_LENGTH_EBW_50UM		15	/* 143 */
#define XFP_LENGTH_50UM			16	/* 144 */
#define XFP_LENGTH_625UM		17	/* 145 */
#define XFP_LENGTH_COPPER		18	/* 146 */
#define XFP_DEVICE_TECH			19	/* 147 */
#define XFP_VENDOR_NAME_START		20	/* 148 */
#define XFP_VENDOR_NAME_END		35	/* 163 */
#define XFP_CDR_SUPPORT			36	/* 164 */
#define XFP_VENDOR_OUI			37	/* 165 */
#define XFP_VENDOR_PN_START		40	/* 168 */
#define XFP_VENDOR_PN_END		55	/* 183 */
#define XFP_VENDOR_REV_START		56	/* 184 */
#define XFP_VENDOR_REV_END		57	/* 185 */
#define XFP_WAVELENGTH			58	/* 186-187 */
#define XFP_WAVELENGTH_TOL		60	/* 188-189 */
#define XFP_MAX_CASE_TEMP		62	/* 190 */
#define XFP_CC_BASE			63	/* 191 */
#define XFP_VENDOR_SN_START		68	/* 196 */
#define XFP_VENDOR_SN_END		83	/* 211 */
#define XFP_DATE_CODE_START		84	/* 212 */
#define XFP_DATE_CODE_END		91	/* 219 */
#define XFP_DIAG_MON_TYPE		92	/* 220 */
#define XFP_ENHANCED_OPTIONS		93	/* 221 */
#define XFP_AUX_MONITORING		94	/* 222 */

/* Extended identifier (byte 129) bit fields */
#define XFP_EXT_ID_POWER_MASK		0xC0
#define XFP_EXT_ID_POWER_SHIFT		6
#define XFP_EXT_ID_CDR			(1 << 5)
#define XFP_EXT_ID_CLEI			(1 << 3)

/* Diagnostic monitoring type (byte 220) bit fields */
#define XFP_DIAG_RX_PWR_TYPE		(1 << 3)

#endif /* XFP_H__ */
