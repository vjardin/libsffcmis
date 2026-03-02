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

/* Lower Memory Map (bytes 0-127) */

#define XFP_ID_OFFSET			0x00

#define XFP_SIGNAL_COND_CTRL		0x01
#define XFP_SCC_RATE_SEL_MASK		0xF0
#define XFP_SCC_RATE_SEL_SHIFT		4
#define XFP_SCC_LINESIDE_LOOPBACK	(1 << 1)
#define XFP_SCC_XFI_LOOPBACK		(1 << 2)
#define XFP_SCC_REFCLK_MODE		(1 << 0)

#define XFP_TEMP_HALRM			2
#define XFP_TEMP_LALRM			4
#define XFP_TEMP_HWARN			6
#define XFP_TEMP_LWARN			8
/* VCC thresholds in 100 uV units */
#define XFP_VCC_HALRM			10
#define XFP_VCC_LALRM			12
#define XFP_VCC_HWARN			14
#define XFP_VCC_LWARN			16
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

#define XFP_BER_ACCEPTABLE		70
#define XFP_BER_ACTUAL			71

#define XFP_FEC_AMPLITUDE		76
#define XFP_FEC_PHASE			77

#define XFP_ALRM_FLG			80
#define XFP_TEMP_HALRM_FLG		(1 << 7)
#define XFP_TEMP_LALRM_FLG		(1 << 6)
#define XFP_BIAS_HALRM_FLG		(1 << 3)
#define XFP_BIAS_LALRM_FLG		(1 << 2)
#define XFP_TX_PWR_HALRM_FLG		(1 << 1)
#define XFP_TX_PWR_LALRM_FLG		(1 << 0)
#define XFP_RX_PWR_HALRM_FLG		(1 << 7)
#define XFP_RX_PWR_LALRM_FLG		(1 << 6)
#define XFP_AUX1_HALRM_FLG		(1 << 5)
#define XFP_AUX1_LALRM_FLG		(1 << 4)
#define XFP_AUX2_HALRM_FLG		(1 << 3)
#define XFP_AUX2_LALRM_FLG		(1 << 2)

#define XFP_WARN_FLG			82
#define XFP_TEMP_HWARN_FLG		(1 << 7)
#define XFP_TEMP_LWARN_FLG		(1 << 6)
#define XFP_BIAS_HWARN_FLG		(1 << 3)
#define XFP_BIAS_LWARN_FLG		(1 << 2)
#define XFP_TX_PWR_HWARN_FLG		(1 << 1)
#define XFP_TX_PWR_LWARN_FLG		(1 << 0)
#define XFP_RX_PWR_HWARN_FLG		(1 << 7)
#define XFP_RX_PWR_LWARN_FLG		(1 << 6)
#define XFP_AUX1_HWARN_FLG		(1 << 5)
#define XFP_AUX1_LWARN_FLG		(1 << 4)
#define XFP_AUX2_HWARN_FLG		(1 << 3)
#define XFP_AUX2_LWARN_FLG		(1 << 2)

#define XFP_EXT_INT_FLG_84		84
#define XFP_TX_NR_FLG			(1 << 7)
#define XFP_TX_FAULT_FLG		(1 << 6)
#define XFP_TX_CDR_LOL_FLG		(1 << 5)
#define XFP_RX_NR_FLG			(1 << 4)
#define XFP_RX_LOS_FLG			(1 << 3)
#define XFP_RX_CDR_LOL_FLG		(1 << 2)
#define XFP_MOD_NR_FLG			(1 << 1)
#define XFP_RESET_COMPLETE_FLG		(1 << 0)

#define XFP_EXT_INT_FLG_85		85
#define XFP_APD_FAULT_FLG		(1 << 7)
#define XFP_TEC_FAULT_FLG		(1 << 6)
#define XFP_WL_UNLOCKED_FLG		(1 << 4)

#define XFP_EXT_INT_FLG_86		86
#define XFP_VCC5_HALRM_FLG		(1 << 7)
#define XFP_VCC5_LALRM_FLG		(1 << 6)
#define XFP_VCC3_HALRM_FLG		(1 << 5)
#define XFP_VCC3_LALRM_FLG		(1 << 4)
#define XFP_VCC2_HALRM_FLG		(1 << 3)
#define XFP_VCC2_LALRM_FLG		(1 << 2)
#define XFP_VEE5_HALRM_FLG		(1 << 1)
#define XFP_VEE5_LALRM_FLG		(1 << 0)

#define XFP_EXT_INT_FLG_87		87
#define XFP_VCC5_HWARN_FLG		(1 << 7)
#define XFP_VCC5_LWARN_FLG		(1 << 6)
#define XFP_VCC3_HWARN_FLG		(1 << 5)
#define XFP_VCC3_LWARN_FLG		(1 << 4)
#define XFP_VCC2_HWARN_FLG		(1 << 3)
#define XFP_VCC2_LWARN_FLG		(1 << 2)
#define XFP_VEE5_HWARN_FLG		(1 << 1)
#define XFP_VEE5_LWARN_FLG		(1 << 0)

/* Interrupt masks — same bit layout as flag bytes 80-87 */
#define XFP_INT_MASK_88			88
#define XFP_INT_MASK_89			89
#define XFP_INT_MASK_90			90
#define XFP_INT_MASK_91			91
#define XFP_INT_MASK_92			92
#define XFP_INT_MASK_93			93
#define XFP_INT_MASK_94			94
#define XFP_INT_MASK_95			95

#define XFP_GEN_CTRL_STATUS		110
#define XFP_GCS_TX_DISABLE_STATE	(1 << 7)
#define XFP_GCS_SOFT_TX_DISABLE		(1 << 6)
#define XFP_GCS_MOD_NR			(1 << 5)
#define XFP_GCS_P_DOWN_STATE		(1 << 4)
#define XFP_GCS_SOFT_P_DOWN		(1 << 3)
#define XFP_GCS_INTERRUPT		(1 << 2)
#define XFP_GCS_RX_LOS			(1 << 1)
#define XFP_GCS_DATA_NOT_READY		(1 << 0)

#define XFP_GEN_CTRL_STATUS_2		111
#define XFP_GCS2_TX_NR			(1 << 7)
#define XFP_GCS2_TX_FAULT		(1 << 6)
#define XFP_GCS2_TX_CDR_NOT_LOCKED	(1 << 5)
#define XFP_GCS2_RX_NR			(1 << 3)
#define XFP_GCS2_RX_CDR_NOT_LOCKED	(1 << 1)

#define XFP_AD_TEMP			96
#define XFP_AD_VCC			98
#define XFP_AD_BIAS			100
#define XFP_AD_TX_PWR			102
#define XFP_AD_RX_PWR			104
#define XFP_AD_AUX1			106
#define XFP_AD_AUX2			108

#define XFP_PAGE_SELECT			127

/* Upper Memory Table 01h — offsets relative to buffer read at 0x80 */

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
#define XFP_CC_EXT			95	/* 223 */

#define XFP_VENDOR_WL_25G		96	/* 224-225: 2.5G wavelength */
#define XFP_VENDOR_WL_125G		98	/* 226-227: 1.25G wavelength */

#define XFP_PWR_MAX			64	/* 192: max power (x20 mW) */
#define XFP_PWR_MAX_PDOWN		65	/* 193: P_Down power (x10 mW) */
#define XFP_PWR_SUPPLY_CURRENT		66	/* 194-195: supply currents */

#define XFP_EXT_ID_POWER_MASK		0xC0
#define XFP_EXT_ID_POWER_SHIFT		6
#define XFP_EXT_ID_CDR			(1 << 5)
#define XFP_EXT_ID_CLEI			(1 << 3)

#define XFP_DEVTECH_TX_MASK		0xF0
#define XFP_DEVTECH_TX_SHIFT		4
#define XFP_DEVTECH_WL_CTRL		(1 << 3)
#define XFP_DEVTECH_COOLED		(1 << 2)
#define XFP_DEVTECH_APD			(1 << 1)
#define XFP_DEVTECH_TUNABLE		(1 << 0)

#define XFP_CDR_9_95G			(1 << 7)
#define XFP_CDR_10_3G			(1 << 6)
#define XFP_CDR_10_5G			(1 << 5)
#define XFP_CDR_10_7G			(1 << 4)
#define XFP_CDR_11_1G			(1 << 3)
#define XFP_CDR_LINESIDE_LOOPBACK	(1 << 2)
#define XFP_CDR_XFI_LOOPBACK		(1 << 1)

#define XFP_DIAG_RX_PWR_TYPE		(1 << 3)
#define XFP_DIAG_BER_SUPPORT		(1 << 4)

#endif /* XFP_H__ */
