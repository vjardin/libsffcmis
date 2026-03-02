#ifndef CMIS_INTERNAL_H__
#define CMIS_INTERNAL_H__

/* cmis-internal.h — libsffcmis extensions beyond ethtool's cmis.h
 *
 * The base cmis.h is kept in sync with ethtool upstream.
 * All libsffcmis additions live here.
 */

#include "cmis.h"

/* Additional Module Types (Page 0) */
#define CMIS_MT_PASSIVE_COPPER			0x03
#define CMIS_MT_ACTIVE_CABLE			0x04
#define CMIS_MT_BASE_T				0x05

/* Application Descriptors (Lower Memory, bytes 86-117) */
#define CMIS_APP_DESC_START_OFFSET		0x56
#define CMIS_APP_DESC_SIZE			4
#define CMIS_MAX_APP_DESCS			8

/* Data Path State (Lower Memory) */
#define CMIS_DP_STATE_OFFSET			0x4C
#define CMIS_DP_STATE_MASK			0x0F
#define CMIS_DP_STATE_DEACTIVATED		0x01
#define CMIS_DP_STATE_INIT			0x02
#define CMIS_DP_STATE_DEINIT			0x03
#define CMIS_DP_STATE_ACTIVATED			0x04
#define CMIS_DP_STATE_TX_TURN_ON		0x05
#define CMIS_DP_STATE_TX_TURN_OFF		0x06
#define CMIS_DP_STATE_INITIALIZED		0x07

/* Module Global Controls extensions (Page 0, byte 26) */
#define CMIS_MODULE_CTL_BANK_BROADCAST		0x80  /* bit 7 */
#define CMIS_MODULE_CTL_SQUELCH_METHOD		0x20  /* bit 5 */
#define CMIS_MODULE_CTL_SW_RESET		0x08  /* bit 3, WO/SC */

/* Module-Level Masks (Page 0, bytes 31-36) */
#define CMIS_MODULE_MASK_OFFSET			0x1F  /* byte 31 */
#define CMIS_MODULE_MASK_CDB2			0x80
#define CMIS_MODULE_MASK_CDB1			0x40
#define CMIS_MODULE_MASK_DP_FW_ERR		0x04
#define CMIS_MODULE_MASK_MOD_FW_ERR		0x02
#define CMIS_MODULE_MASK_STATE_CHANGED		0x01
#define CMIS_MODULE_MASK_VCC_TEMP_OFFSET	0x20  /* byte 32 */
#define CMIS_MODULE_MASK_AUX12_OFFSET		0x21  /* byte 33 */
#define CMIS_MODULE_MASK_AUX3_CUSTOM_OFFSET	0x22  /* byte 34 */

/* Password Facilities (Page 0, bytes 118-125) */
#define CMIS_PASSWORD_CHANGE_OFFSET		0x76  /* bytes 118-121 WO/SC */
#define CMIS_PASSWORD_ENTRY_OFFSET		0x7A  /* bytes 122-125 WO/SC */

/*-----------------------------------------------------------------------
 * Upper Memory Page 0x01 extensions
 */

/* Hardware Revision (Page 1, bytes 130-131, Table 8-43) */
#define CMIS_MODULE_HW_MAJOR_OFFSET		0x82  /* byte 130 */
#define CMIS_MODULE_HW_MINOR_OFFSET		0x83  /* byte 131 */

/* ModSelWaitTime (Page 1, byte 143, Table 8-47) */
#define CMIS_MODSEL_WAIT_TIME_OFFSET		0x8F  /* byte 143 */
#define CMIS_MODSEL_WAIT_EXP_MASK		0xE0  /* bits 7-5 */
#define CMIS_MODSEL_WAIT_EXP_SHIFT		5
#define CMIS_MODSEL_WAIT_MANT_MASK		0x1F  /* bits 4-0 */

/* State Duration (Page 1, byte 144, Table 8-47/8-48) */
#define CMIS_STATE_DURATION_OFFSET		0x90  /* byte 144 */
#define CMIS_MAX_DUR_DPDEINIT_MASK		0xF0  /* bits 7-4 */
#define CMIS_MAX_DUR_DPDEINIT_SHIFT		4
#define CMIS_MAX_DUR_DPINIT_MASK		0x0F  /* bits 3-0 */

/* Module Characteristics (Page 1, bytes 145-154, Table 8-49) */
#define CMIS_MOD_CHARS_OFFSET			0x91  /* byte 145 */
#define CMIS_MOD_COOLING_IMPLEMENTED		0x80  /* bit 7 */
#define CMIS_MOD_TX_CLK_CAP_MASK		0x60  /* bits 6-5 */
#define CMIS_MOD_TX_CLK_CAP_SHIFT		5
#define CMIS_MOD_EPPS_SUPPORTED			0x10  /* bit 4 */
#define CMIS_MOD_PAGE15H_SUPPORTED		0x08  /* bit 3 */
#define CMIS_MOD_AUX3_OBSERVABLE		0x04  /* bit 2 */
#define CMIS_MOD_AUX2_OBSERVABLE		0x02  /* bit 1 */
#define CMIS_MOD_AUX1_OBSERVABLE		0x01  /* bit 0 */

#define CMIS_MOD_TEMP_MAX_OFFSET		0x92  /* byte 146, S8 degC */
#define CMIS_MOD_TEMP_MIN_OFFSET		0x93  /* byte 147, S8 degC */
#define CMIS_PROPAGATION_DELAY_OFFSET		0x94  /* bytes 148-149, U16 x 10 ns */
#define CMIS_OPERATING_VOLTAGE_MIN_OFFSET	0x96  /* byte 150, U8 x 20 mV */

#define CMIS_OPTICAL_DET_OFFSET			0x97  /* byte 151 */
#define CMIS_OPTICAL_DET_APD			0x80  /* bit 7: 0=PIN, 1=APD */
#define CMIS_RX_OUT_EQ_TYPE_MASK		0x60  /* bits 6-5 */
#define CMIS_RX_OUT_EQ_TYPE_SHIFT		5
#define CMIS_RX_PWR_MEAS_TYPE			0x10  /* bit 4: 0=OMA, 1=Pav */
#define CMIS_RX_LOS_TYPE			0x08  /* bit 3: 0=OMA, 1=Pav */
#define CMIS_RX_LOS_FAST			0x04  /* bit 2 */
#define CMIS_TX_DISABLE_FAST			0x02  /* bit 1 */
#define CMIS_TX_DISABLE_MODULE_WIDE		0x01  /* bit 0 */

#define CMIS_CDR_PWR_SAVED_OFFSET		0x98  /* byte 152, U8 x 0.01 W */
#define CMIS_RX_OUT_LEVELS_OFFSET		0x99  /* byte 153 */
#define CMIS_TX_INPUT_EQ_MAX_MASK		0x0F  /* bits 3-0 */
#define CMIS_RX_OUT_EQ_MAX_OFFSET		0x9A  /* byte 154 */
#define CMIS_RX_OUT_EQ_POST_MAX_MASK		0xF0  /* bits 7-4 */
#define CMIS_RX_OUT_EQ_POST_MAX_SHIFT		4
#define CMIS_RX_OUT_EQ_PRE_MAX_MASK		0x0F  /* bits 3-0 */

/* Additional Durations (Page 1, bytes 167-169, Table 8-56) */
#define CMIS_ADD_DUR1_OFFSET			0xA7  /* byte 167 */
#define CMIS_MAX_DUR_MOD_PWRDN_MASK		0xF0  /* bits 7-4 */
#define CMIS_MAX_DUR_MOD_PWRDN_SHIFT		4
#define CMIS_MAX_DUR_MOD_PWRUP_MASK		0x0F  /* bits 3-0 */
#define CMIS_ADD_DUR2_OFFSET			0xA8  /* byte 168 */
#define CMIS_MAX_DUR_TXTURNOFF_MASK		0xF0  /* bits 7-4 */
#define CMIS_MAX_DUR_TXTURNOFF_SHIFT		4
#define CMIS_MAX_DUR_TXTURNON_MASK		0x0F  /* bits 3-0 */
#define CMIS_ADD_DUR3_OFFSET			0xA9  /* byte 169 */
#define CMIS_MAX_DUR_BPC_MASK			0xF0  /* bits 7-4 */
#define CMIS_MAX_DUR_BPC_SHIFT			4

/* Supported Pages Advertising extensions (Page 1, byte 142 / 0x8E) */
#define CMIS_PAGES_ADVER_NETWORK_PATH		0x80  /* Bit 7: Pages 16h, 17h */
#define CMIS_PAGES_ADVER_VDM			0x40  /* Bit 6: Pages 20h-2Fh */
#define CMIS_PAGES_ADVER_DIAG			0x20  /* Bit 5: Pages 13h-14h */
#define CMIS_PAGES_ADVER_COHERENT		0x10  /* Bit 4: Pages 30h-4Fh (C-CMIS) */
#define CMIS_PAGES_ADVER_CMIS_FF		0x08  /* Bit 3: Page 05h */
#define CMIS_PAGES_ADVER_PAGE_03H		0x04  /* Bit 2: User Page 03h */

/* CDB Messaging Support Advertisement extensions */
#define CMIS_CDB_ADVER_AUTOPAGING_MASK		0x10  /* Bit 4: AutoPaging */
#define CMIS_CDB_ADVER_EXT_BUSY_MASK		0x1F  /* Bits 4-0: ExtMaxBusyTime */

#define CMIS_CDB_ADVER_BUSY_OFFSET		0xA6  /* Byte 166 */
#define CMIS_CDB_BUSY_SPEC_METHOD_MASK		0x80  /* Bit 7: 0=short, 1=ext */
#define CMIS_CDB_BUSY_TIME_MASK			0x7F  /* Bits 6-0: short method */

/*-----------------------------------------------------------------------
 * C-CMIS Coherent Pages (Pages 30h-4Fh)
 * OIF-C-CMIS-01.3
 */

/* Page 40h: C-CMIS Revision (byte 128) */
#define CCMIS_REV_PAGE				0x40
#define CCMIS_REV_OFFSET			0x80

/* Page 42h: PM Advertisement Bitmap (bytes 128-140, 13 bytes) */
#define CCMIS_PM_ADVER_PAGE			0x42
#define CCMIS_PM_ADVER_OFFSET			0x80
#define CCMIS_PM_ADVER_LEN			13

/* Page 35h: Media Lane Link Performance Monitoring (Bank 0)
 * All offsets relative to page base.  Each metric is an avg/min/max triplet.
 */
#define CCMIS_PM_PAGE				0x35

/* CD: S32 triplet (12 bytes), unit 1 ps/nm */
#define CCMIS_PM_CD_OFFSET			0x80
/* DGD: U16 triplet (6 bytes), unit 0.01 ps */
#define CCMIS_PM_DGD_OFFSET			0x8C
/* HG-SOPMD: U16 triplet (6 bytes), unit 0.01 ps^2 */
#define CCMIS_PM_HGSOPMD_OFFSET			0x92
/* PDL: U16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_PDL_OFFSET			0x98
/* OSNR: U16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_OSNR_OFFSET			0x9E
/* eSNR: U16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_ESNR_OFFSET			0xA4
/* CFO: S16 triplet (6 bytes), unit 1 MHz */
#define CCMIS_PM_CFO_OFFSET			0xAA
/* EVM: U16 triplet (6 bytes), unit 100/65535 % */
#define CCMIS_PM_EVM_OFFSET			0xB0
/* Tx Power: S16 triplet (6 bytes), unit 0.01 dBm */
#define CCMIS_PM_TXPOWER_OFFSET			0xB6
/* Rx Power: S16 triplet (6 bytes), unit 0.01 dBm */
#define CCMIS_PM_RXPOWER_OFFSET			0xBC
/* Rx Signal Power: S16 triplet (6 bytes), unit 0.01 dBm */
#define CCMIS_PM_RXSIGPOWER_OFFSET		0xC2
/* SOP-ROC: U16 triplet (6 bytes), unit 1 krad/s */
#define CCMIS_PM_SOPROC_OFFSET			0xC8
/* MER: U16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_MER_OFFSET			0xCE
/* Clock Recovery: S16 triplet (6 bytes), unit 100/32767 % */
#define CCMIS_PM_CLOCKREC_OFFSET		0xD4
/* LG-SOPMD: U16 triplet (6 bytes), unit 1 ps^2 */
#define CCMIS_PM_LGSOPMD_OFFSET			0xDA
/* SNR Margin: S16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_SNRMARGIN_OFFSET		0xE0
/* Q-Factor: U16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_QFACTOR_OFFSET			0xE6
/* Q-Margin: S16 triplet (6 bytes), unit 0.1 dB */
#define CCMIS_PM_QMARGIN_OFFSET			0xEC

/* Page 34h: Media FEC Performance Counters (Bank 0) */
#define CCMIS_FEC_PAGE				0x34

#define CCMIS_FEC_RXBITS_OFFSET			0x80  /* U64 */
#define CCMIS_FEC_RXBITS_SUBINT_OFFSET		0x88  /* U64 */
#define CCMIS_FEC_RXCORRBITS_OFFSET		0x90  /* U64 */
#define CCMIS_FEC_RXMINCORRBITS_SUBINT_OFFSET	0x98  /* U64 */
#define CCMIS_FEC_RXMAXCORRBITS_SUBINT_OFFSET	0xA0  /* U64 */
#define CCMIS_FEC_RXFRAMES_OFFSET		0xA8  /* U32 */
#define CCMIS_FEC_RXFRAMES_SUBINT_OFFSET	0xAC  /* U32 */
#define CCMIS_FEC_RXUNCORRERR_OFFSET		0xB0  /* U32 */
#define CCMIS_FEC_RXMINUNCORRERR_SUBINT_OFFSET	0xB4  /* U32 */
#define CCMIS_FEC_RXMAXUNCORRERR_SUBINT_OFFSET	0xB8  /* U32 */

/* Page 33h: Media Lane Coherent Flags (banked, RO/COR)
 * OIF-C-CMIS-01.3, Table 13
 */
#define CCMIS_FLAGS_MEDIA_PAGE			0x33

#define CCMIS_FLAGS_TX_OFFSET			0x80  /* Byte 128 */
#define CCMIS_FLAGS_TX_LOA_BIT			5
#define CCMIS_FLAGS_TX_OOA_BIT			4
#define CCMIS_FLAGS_TX_LOL_CMU_BIT		3
#define CCMIS_FLAGS_TX_LOL_REFCLK_BIT		2
#define CCMIS_FLAGS_TX_LOL_DESKEW_BIT		1
#define CCMIS_FLAGS_TX_FIFO_BIT			0

#define CCMIS_FLAGS_RX_OFFSET			0x82  /* Byte 130 */
#define CCMIS_FLAGS_RX_LOF_BIT			7
#define CCMIS_FLAGS_RX_LOM_BIT			6
#define CCMIS_FLAGS_RX_LOL_DEMOD_BIT		5
#define CCMIS_FLAGS_RX_LOL_CD_BIT		4
#define CCMIS_FLAGS_RX_LOA_BIT			3
#define CCMIS_FLAGS_RX_OOA_BIT			2
#define CCMIS_FLAGS_RX_LOL_DESKEW_BIT		1
#define CCMIS_FLAGS_RX_LOL_FIFO_BIT		0

#define CCMIS_FLAGS_RX_FEC_OFFSET		0x83  /* Byte 131 */
#define CCMIS_FLAGS_RX_FED_PM_BIT		1
#define CCMIS_FLAGS_RX_FDD_PM_BIT		0

#define CCMIS_FLAGS_DEGRADE_OFFSET		0x84  /* Byte 132 */
#define CCMIS_FLAGS_RD_BIT			2
#define CCMIS_FLAGS_LD_BIT			1
#define CCMIS_FLAGS_RPF_BIT			0

/* Page 3Bh: Host Data Path Coherent Flags (banked, RO/COR)
 * OIF-C-CMIS-01.3, Table 25
 * Latched flags at bytes 192-196, masks at bytes 128-132.
 */
#define CCMIS_FLAGS_HOST_PAGE			0x3B

#define CCMIS_FLAGS_HOST_FEC_OFFSET		0xC0  /* Byte 192 */
#define CCMIS_FLAGS_HOST_FED_PM_BIT		1
#define CCMIS_FLAGS_HOST_FDD_PM_BIT		0

#define CCMIS_FLAGS_HOST_DEGRADE_OFFSET		0xC1  /* Byte 193 */
#define CCMIS_FLAGS_HOST_RD_BIT			1
#define CCMIS_FLAGS_HOST_LD_BIT			0

#define CCMIS_FLAGS_HOST_FLEXE_OFFSET		0xC2  /* Byte 194 */
#define CCMIS_FLAGS_HOST_FLEXE_RPF_BIT		7
#define CCMIS_FLAGS_HOST_FLEXE_GIDMM_BIT	6
#define CCMIS_FLAGS_HOST_FLEXE_INSTMAPMM_BIT	5
#define CCMIS_FLAGS_HOST_FLEXE_CALMM_BIT	4
#define CCMIS_FLAGS_HOST_FLEXE_IIDMM_BIT	3
#define CCMIS_FLAGS_HOST_FLEXE_LOF_BIT		2
#define CCMIS_FLAGS_HOST_FLEXE_LOM_BIT		1
#define CCMIS_FLAGS_HOST_FLEXE_LOPB_BIT		0

#define CCMIS_FLAGS_HOST_TX_OFFSET		0xC3  /* Byte 195 */
#define CCMIS_FLAGS_HOST_TX_LOA_BIT		2
#define CCMIS_FLAGS_HOST_TX_RF_BIT		1
#define CCMIS_FLAGS_HOST_TX_LF_BIT		0

#define CCMIS_FLAGS_HOST_RX_OFFSET		0xC4  /* Byte 196 */
#define CCMIS_FLAGS_HOST_RX_LOA_BIT		2
#define CCMIS_FLAGS_HOST_RX_RF_BIT		1
#define CCMIS_FLAGS_HOST_RX_LF_BIT		0

/* Page 30h: Media Lane Coherent Configurable Thresholds (banked)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_THRESH_PAGE		0x30

#define CCMIS_THRESH_TOTAL_PWR_HI_ALARM	0x80  /* S16, 0.01 dBm */
#define CCMIS_THRESH_TOTAL_PWR_LO_ALARM	0x82
#define CCMIS_THRESH_TOTAL_PWR_HI_WARN	0x84
#define CCMIS_THRESH_TOTAL_PWR_LO_WARN	0x86
#define CCMIS_THRESH_SIG_PWR_HI_ALARM	0x88
#define CCMIS_THRESH_SIG_PWR_LO_ALARM	0x8A
#define CCMIS_THRESH_SIG_PWR_HI_WARN	0x8C
#define CCMIS_THRESH_SIG_PWR_LO_WARN	0x8E
#define CCMIS_THRESH_USE_CFG_OFFSET	0x90  /* byte 144 */
#define CCMIS_THRESH_USE_CFG_TOTAL_PWR	0x02  /* bit 1 */
#define CCMIS_THRESH_USE_CFG_SIG_PWR	0x01  /* bit 0 */
#define CCMIS_THRESH_FDD_RAISE		0xA0  /* F16 BER */
#define CCMIS_THRESH_FDD_CLEAR		0xA2
#define CCMIS_THRESH_FED_RAISE		0xA4
#define CCMIS_THRESH_FED_CLEAR		0xA6
#define CCMIS_THRESH_FEC_ENABLE_OFFSET	0xA8  /* byte 168 */
#define CCMIS_THRESH_FED_ENABLE		0x02  /* bit 1 */
#define CCMIS_THRESH_FDD_ENABLE		0x01  /* bit 0 */

/* Page 31h: Media Lane Provisioning (banked)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_PROV_PAGE			0x31

#define CCMIS_PROV_TX_FILTER_ENABLE	0x80  /* byte 128, 1 bit per lane */
#define CCMIS_PROV_TX_FILTER_TYPE	0x81  /* byte 129, 2 bits per lane */
#define CCMIS_PROV_TX_FILTER_ROLLOFF	0x83  /* bytes 131-132, U16 per lane? */
#define CCMIS_PROV_LF_INSERTION		0x84  /* byte 132, 1 bit per lane */

/* Page 32h: Media Lane Coherent Flag Masks (banked, RW)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_MEDIA_MASKS_PAGE		0x32

#define CCMIS_MEDIA_MASK_TX_OFFSET	0x80  /* Byte 128 (matches flags Page 33h:0x80) */
#define CCMIS_MEDIA_MASK_RX_OFFSET	0x82  /* Byte 130 */
#define CCMIS_MEDIA_MASK_RX_FEC_OFFSET	0x83  /* Byte 131 */
#define CCMIS_MEDIA_MASK_DEGRADE_OFFSET	0x84  /* Byte 132 */

/* Page 38h: Host Interface FDD/FED Thresholds (banked, RW)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_HOST_THRESH_PAGE		0x38

#define CCMIS_HOST_THRESH_FDD_RAISE	0x80  /* F16 BER */
#define CCMIS_HOST_THRESH_FDD_CLEAR	0x82
#define CCMIS_HOST_THRESH_FED_RAISE	0x84
#define CCMIS_HOST_THRESH_FED_CLEAR	0x86
#define CCMIS_HOST_THRESH_ENABLE_OFFSET	0x88  /* byte 136 */
#define CCMIS_HOST_THRESH_FED_ENABLE	0x02  /* bit 1 */
#define CCMIS_HOST_THRESH_FDD_ENABLE	0x01  /* bit 0 */

/* Page 3Bh: Host Data Path Coherent Flag Masks (banked, RW)
 * Masks at bytes 128-132, Flags at bytes 192-196 (already defined)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_HOST_MASKS_FEC_OFFSET	0x80  /* Byte 128 */
#define CCMIS_HOST_MASKS_DEGRADE_OFFSET	0x81  /* Byte 129 */
#define CCMIS_HOST_MASKS_FLEXE_OFFSET	0x82  /* Byte 130 */
#define CCMIS_HOST_MASKS_TX_OFFSET	0x83  /* Byte 131 */
#define CCMIS_HOST_MASKS_RX_OFFSET	0x84  /* Byte 132 */

/* Page 41h: Rx Signal Power Advertisement (non-banked)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_RX_PWR_ADVER_PAGE		0x41

#define CCMIS_RX_PWR_IMPL_OFFSET	0x80  /* byte 128 */
#define CCMIS_RX_PWR_IMPL_TOTAL	0x80  /* bit 7: total power threshold cfg */
#define CCMIS_RX_PWR_IMPL_SIGNAL	0x40  /* bit 6: signal power threshold cfg */
#define CCMIS_RX_LOS_TYPE_MASK		0x30  /* bits 5-4: Rx LOS type */
#define CCMIS_RX_LOS_TYPE_SHIFT		4
#define CCMIS_RX_PWR_TOTAL_HI_ALARM	0x82  /* S16, 0.01 dBm */
#define CCMIS_RX_PWR_TOTAL_LO_ALARM	0x84
#define CCMIS_RX_PWR_TOTAL_HI_WARN	0x86
#define CCMIS_RX_PWR_TOTAL_LO_WARN	0x88
#define CCMIS_RX_PWR_SIG_HI_ALARM	0x8A
#define CCMIS_RX_PWR_SIG_LO_ALARM	0x8C
#define CCMIS_RX_PWR_SIG_HI_WARN	0x8E
#define CCMIS_RX_PWR_SIG_LO_WARN	0x90

/* Page 43h: Media Lane Provisioning Advertisement (non-banked)
 * OIF-C-CMIS-01.3
 */
#define CCMIS_PROV_ADVER_PAGE		0x43

#define CCMIS_PROV_ADVER_OFFSET		0x80  /* byte 128 */
#define CCMIS_PROV_ADVER_TX_FILTER	0x80  /* bit 7: Tx filter control */
#define CCMIS_PROV_ADVER_LF_INSERTION	0x40  /* bit 6: LF insertion on LD */

/* Page 3Ah: Host FEC Performance Counters (banked)
 * OIF-C-CMIS-01.3, Table 18 — same structure as Page 34h plus extra fields
 */
#define CCMIS_HOST_FEC_PAGE			0x3A

#define CCMIS_HOST_FEC_TXBITS_OFFSET		0x80  /* U64 */
#define CCMIS_HOST_FEC_TXBITS_SUBINT_OFFSET	0x88  /* U64 */
#define CCMIS_HOST_FEC_TXCORRBITS_OFFSET	0x90  /* U64 */
#define CCMIS_HOST_FEC_TXMINCORRBITS_SUBINT_OFFSET 0x98  /* U64 */
#define CCMIS_HOST_FEC_TXMAXCORRBITS_SUBINT_OFFSET 0xA0  /* U64 */
#define CCMIS_HOST_FEC_TXFRAMES_OFFSET		0xA8  /* U32 */
#define CCMIS_HOST_FEC_TXFRAMES_SUBINT_OFFSET	0xAC  /* U32 */
#define CCMIS_HOST_FEC_TXUNCORRERR_OFFSET	0xB0  /* U32 */
#define CCMIS_HOST_FEC_TXMINUNCORRERR_SUBINT_OFFSET 0xB4  /* U32 */
#define CCMIS_HOST_FEC_TXMAXUNCORRERR_SUBINT_OFFSET 0xB8  /* U32 */
#define CCMIS_HOST_FEC_TXCORRFRAMES_OFFSET	0xBC  /* U32, C-CMIS 1.3+ */
#define CCMIS_HOST_FEC_TXCORRFRAMES_SUBINT_OFFSET 0xC0  /* U32, C-CMIS 1.3+ */

/* Page 44h: Alarm Advertisement (non-banked, RO)
 * OIF-C-CMIS-01.3, Table 19
 * Media alarm adver: bytes 128-131, Host alarm adver: bytes 132-136.
 */
#define CCMIS_ALARM_ADVER_PAGE			0x44
#define CCMIS_ALARM_ADVER_OFFSET		0x80  /* Byte 128 */
#define CCMIS_ALARM_ADVER_LEN			9     /* bytes 128-136 */

/*-----------------------------------------------------------------------
 * CDB (Command Data Block) Messaging - Page 00h Status & Page 9Fh
 * OIF-CMIS-05.3, Section 9
 */

/* CDB completion flags (Page 00h, byte 8) */
#define CMIS_CDB_COMPLETE_FLAGS_OFFSET		0x08
#define CMIS_CDB_COMPLETE_FLAG1			0x40
#define CMIS_CDB_COMPLETE_FLAG2			0x80

/* CDB status registers (Page 00h) */
#define CMIS_CDB_STATUS1_OFFSET			0x25
#define CMIS_CDB_STATUS2_OFFSET			0x26

/* CDB status byte bit fields */
#define CMIS_CDB_STATUS_BUSY			0x80
#define CMIS_CDB_STATUS_FAIL			0x40
#define CMIS_CDB_STATUS_RESULT_MASK		0x3F

/* CDB status result codes (when not busy, not failed) */
#define CMIS_CDB_STS_SUCCESS			0x01
#define CMIS_CDB_STS_ABORTED			0x03

/* CDB failure codes (when failed) */
#define CMIS_CDB_FAIL_UNKNOWN_CMD		0x01
#define CMIS_CDB_FAIL_PARAM_RANGE		0x02
#define CMIS_CDB_FAIL_ABORT_PREV		0x03
#define CMIS_CDB_FAIL_CMD_TIMEOUT		0x04
#define CMIS_CDB_FAIL_CHKCODE			0x05
#define CMIS_CDB_FAIL_PASSWORD			0x06
#define CMIS_CDB_FAIL_COMPAT			0x07

/* Page 9Fh: CDB message registers (offsets within page) */
#define CMIS_CDB_CMD_MSB			0x80
#define CMIS_CDB_CMD_LSB			0x81
#define CMIS_CDB_EPL_LEN_MSB			0x82
#define CMIS_CDB_EPL_LEN_LSB			0x83
#define CMIS_CDB_LPL_LEN			0x84
#define CMIS_CDB_CHK_CODE			0x85
#define CMIS_CDB_RPL_LEN			0x86
#define CMIS_CDB_RPL_CHK_CODE			0x87
#define CMIS_CDB_LPL_START			0x88

/* Standard CDB command IDs */
#define CMIS_CDB_CMD_QUERY_STATUS		0x0000
#define CMIS_CDB_CMD_ABORT			0x0004
#define CMIS_CDB_CMD_MODULE_FEATURES		0x0040
#define CMIS_CDB_CMD_FW_MGMT_FEATURES		0x0041
#define CMIS_CDB_CMD_PM_FEATURES		0x0042
#define CMIS_CDB_CMD_BERT_FEATURES		0x0043
#define CMIS_CDB_CMD_SECURITY_FEATURES		0x0044
#define CMIS_CDB_CMD_EXT_FEATURES		0x0045

/* Firmware management CDB commands */
#define CMIS_CDB_CMD_GET_FW_INFO		0x0100
#define CMIS_CDB_CMD_START_FW_DOWNLOAD		0x0101
#define CMIS_CDB_CMD_ABORT_FW_DOWNLOAD		0x0102
#define CMIS_CDB_CMD_WRITE_FW_BLOCK_LPL	0x0103
#define CMIS_CDB_CMD_WRITE_FW_BLOCK_EPL	0x0104
#define CMIS_CDB_CMD_COMPLETE_FW_DOWNLOAD	0x0107
#define CMIS_CDB_CMD_COPY_FW_IMAGE		0x0108
#define CMIS_CDB_CMD_RUN_FW_IMAGE		0x0109
#define CMIS_CDB_CMD_COMMIT_FW_IMAGE		0x010A

/* Security CDB commands */
#define CMIS_CDB_CMD_GET_CERT_LPL		0x0400
#define CMIS_CDB_CMD_GET_CERT_EPL		0x0401
#define CMIS_CDB_CMD_SET_DIGEST_LPL		0x0402
#define CMIS_CDB_CMD_SET_DIGEST_EPL		0x0403
#define CMIS_CDB_CMD_GET_SIGNATURE_LPL		0x0404
#define CMIS_CDB_CMD_GET_SIGNATURE_EPL		0x0405

/* CDB PM commands (Section 9.8) */
#define CMIS_CDB_CMD_CONTROL_PM			0x0200
#define CMIS_CDB_CMD_PM_FEATURE_INFO		0x0201
#define CMIS_CDB_CMD_GET_MODULE_PM		0x0210
#define CMIS_CDB_CMD_GET_MODULE_PM_EPL		0x0211
#define CMIS_CDB_CMD_GET_HOST_PM		0x0212
#define CMIS_CDB_CMD_GET_HOST_PM_EPL		0x0213
#define CMIS_CDB_CMD_GET_MEDIA_PM		0x0214
#define CMIS_CDB_CMD_GET_MEDIA_PM_EPL		0x0215
#define CMIS_CDB_CMD_GET_DP_PM			0x0216
#define CMIS_CDB_CMD_GET_DP_PM_EPL		0x0217

/* Vendor-specific CDB command range */
#define CMIS_CDB_CMD_VENDOR_START		0x8000
#define CMIS_CDB_CMD_VENDOR_END			0xFFFF

/* The maximum number of supported Banks. Relevant documents:
 * [1] CMIS Rev. 5, page. 128, section 8.4.4, Table 8-40
 */
#define CMIS_MAX_BANKS		4
#define CMIS_CHANNELS_PER_BANK	8
#define CMIS_MAX_CHANNEL_NUM	(CMIS_MAX_BANKS * CMIS_CHANNELS_PER_BANK)

/*-----------------------------------------------------------------------
 * Diagnostics Pages (Pages 13h-14h, banked) — OIF-CMIS-05.3 Section 8.10
 */

/* Page 13h: Diagnostic Capabilities and Controls */
#define CMIS_DIAG_LOOPBACK_CAPS		0x80  /* Byte 128 */
#define CMIS_DIAG_LOOPBACK_MEDIA_OUT	0x80  /* bit 7 */
#define CMIS_DIAG_LOOPBACK_MEDIA_IN	0x40  /* bit 6 */
#define CMIS_DIAG_LOOPBACK_HOST_OUT	0x20  /* bit 5 */
#define CMIS_DIAG_LOOPBACK_HOST_IN	0x10  /* bit 4 */
#define CMIS_DIAG_LOOPBACK_PL_MEDIA	0x02  /* bit 1: per-lane media */
#define CMIS_DIAG_LOOPBACK_PL_HOST	0x01  /* bit 0: per-lane host */

#define CMIS_DIAG_MEAS_CAPS		0x81  /* Byte 129 */
#define CMIS_DIAG_MEAS_GATING		0x80  /* bit 7 */
#define CMIS_DIAG_MEAS_GATING_ACC_2MS	0x40  /* bit 6: accuracy < 2ms */
#define CMIS_DIAG_MEAS_AUTO_RESTART	0x20  /* bit 5 */
#define CMIS_DIAG_MEAS_PERIODIC_UPD	0x10  /* bit 4 */

#define CMIS_DIAG_REPORT_CAPS		0x82  /* Byte 130 */
#define CMIS_DIAG_REPORT_BER		0x80  /* bit 7: BER results */
#define CMIS_DIAG_REPORT_ERR_CNT	0x40  /* bit 6: error counting */
#define CMIS_DIAG_REPORT_SNR_HOST	0x20  /* bit 5: host SNR */
#define CMIS_DIAG_REPORT_SNR_MEDIA	0x10  /* bit 4: media SNR */
#define CMIS_DIAG_REPORT_FEC_HOST	0x08  /* bit 3: host FEC */
#define CMIS_DIAG_REPORT_FEC_MEDIA	0x04  /* bit 2: media FEC */

#define CMIS_DIAG_PATTERN_LOC		0x83  /* Byte 131 */
#define CMIS_DIAG_PATTERN_GEN_HOST	0x80  /* bit 7 */
#define CMIS_DIAG_PATTERN_GEN_MEDIA	0x40  /* bit 6 */
#define CMIS_DIAG_PATTERN_CHK_HOST	0x20  /* bit 5 */
#define CMIS_DIAG_PATTERN_CHK_MEDIA	0x10  /* bit 4 */
#define CMIS_DIAG_PATTERN_GEN_HOST_POST	0x08  /* bit 3: post-FEC gen */
#define CMIS_DIAG_PATTERN_CHK_HOST_PRE	0x04  /* bit 2: pre-FEC chk */
#define CMIS_DIAG_PATTERN_GEN_MEDIA_POST 0x02  /* bit 1 */
#define CMIS_DIAG_PATTERN_CHK_MEDIA_PRE	0x01  /* bit 0 */

/* Pattern capabilities bitmaps (bytes 132-142, 16 patterns) */
#define CMIS_DIAG_PATTERN_HOST_GEN	0x84  /* Bytes 132-133 */
#define CMIS_DIAG_PATTERN_HOST_CHK	0x86  /* Bytes 134-135 */
#define CMIS_DIAG_PATTERN_MEDIA_GEN	0x88  /* Bytes 136-137 */
#define CMIS_DIAG_PATTERN_MEDIA_CHK	0x8A  /* Bytes 138-139 */

/* Page 13h: PRBS Generator/Checker Controls (bytes 144-177, RW) */
/* Host-side generator */
#define CMIS_DIAG_HOST_GEN_ENABLE	0x90  /* Byte 144: 1 bit/lane */
#define CMIS_DIAG_HOST_GEN_INVERT	0x91  /* Byte 145 */
#define CMIS_DIAG_HOST_GEN_SWAP_SYMBOL	0x92  /* Byte 146 */
#define CMIS_DIAG_HOST_GEN_PRE_FEC	0x93  /* Byte 147 */
#define CMIS_DIAG_HOST_GEN_PAT_SEL	0x94  /* Bytes 148-151: 4-bit nibbles */

/* Media-side generator */
#define CMIS_DIAG_MEDIA_GEN_ENABLE	0x98  /* Byte 152 */
#define CMIS_DIAG_MEDIA_GEN_INVERT	0x99  /* Byte 153 */
#define CMIS_DIAG_MEDIA_GEN_SWAP_SYMBOL	0x9A  /* Byte 154 */
#define CMIS_DIAG_MEDIA_GEN_PRE_FEC	0x9B  /* Byte 155 */
#define CMIS_DIAG_MEDIA_GEN_PAT_SEL	0x9C  /* Bytes 156-159 */

/* Host-side checker */
#define CMIS_DIAG_HOST_CHK_ENABLE	0xA0  /* Byte 160 */
#define CMIS_DIAG_HOST_CHK_INVERT	0xA1  /* Byte 161 */
#define CMIS_DIAG_HOST_CHK_SWAP_SYMBOL	0xA2  /* Byte 162 */
#define CMIS_DIAG_HOST_CHK_POST_FEC	0xA3  /* Byte 163 */
#define CMIS_DIAG_HOST_CHK_PAT_SEL	0xA4  /* Bytes 164-167 */

/* Media-side checker */
#define CMIS_DIAG_MEDIA_CHK_ENABLE	0xA8  /* Byte 168 */
#define CMIS_DIAG_MEDIA_CHK_INVERT	0xA9  /* Byte 169 */
#define CMIS_DIAG_MEDIA_CHK_SWAP_SYMBOL	0xAA  /* Byte 170 */
#define CMIS_DIAG_MEDIA_CHK_POST_FEC	0xAB  /* Byte 171 */
#define CMIS_DIAG_MEDIA_CHK_PAT_SEL	0xAC  /* Bytes 172-175 */

/* Clocking and measurement control */
#define CMIS_DIAG_CLOCK_CONTROL	0xB0  /* Byte 176 */
#define CMIS_DIAG_MEAS_CONTROL		0xB1  /* Byte 177 */
#define CMIS_DIAG_MEAS_START_STOP	0x80  /* bit 7: 1=start, 0=stop */
#define CMIS_DIAG_MEAS_RESET_ERR	0x20  /* bit 5: ResetErrorInformation */

/* Loopback controls (bytes 180-183) */
#define CMIS_DIAG_LOOPBACK_MEDIA_OUT_CTL 0xB4  /* Byte 180 */
#define CMIS_DIAG_LOOPBACK_MEDIA_IN_CTL  0xB5  /* Byte 181 */
#define CMIS_DIAG_LOOPBACK_HOST_OUT_CTL  0xB6  /* Byte 182 */
#define CMIS_DIAG_LOOPBACK_HOST_IN_CTL   0xB7  /* Byte 183 */

/* Page 14h: Diagnostic Results */
#define CMIS_DIAG_SELECTOR		0x80  /* Byte 128: DiagnosticsSelector */
#define CMIS_DIAG_DATA_START		0xC0  /* Byte 192: data area (64 bytes) */

/* DiagnosticsSelector values */
#define CMIS_DIAG_SEL_BER_RT		0x01  /* Real-time BER (F16) */
#define CMIS_DIAG_SEL_ERR_CNT_H	0x02  /* Host error count (U64) */
#define CMIS_DIAG_SEL_BITS_CNT_H	0x03  /* Host bits count (U64) */
#define CMIS_DIAG_SEL_ERR_CNT_M	0x04  /* Media error count (U64) */
#define CMIS_DIAG_SEL_BITS_CNT_M	0x05  /* Media bits count (U64) */
#define CMIS_DIAG_SEL_SNR		0x06  /* SNR (U16 LE, 1/256 dB) */
#define CMIS_DIAG_SEL_BER_GATED	0x11  /* Gated BER (F16) */
#define CMIS_DIAG_SEL_ERR_CNT_HG	0x12  /* Gated host error count */
#define CMIS_DIAG_SEL_BITS_CNT_HG	0x13  /* Gated host bits count */
#define CMIS_DIAG_SEL_ERR_CNT_MG	0x14  /* Gated media error count */
#define CMIS_DIAG_SEL_BITS_CNT_MG	0x15  /* Gated media bits count */

/* Diagnostics Masks (Page 13h, bytes 206-213) */
#define CMIS_DIAG_MASK_REF_CLK_LOSS	0xCE  /* Byte 206, bit 7 */
#define CMIS_DIAG_MASK_GATING_HOST	0xD0  /* Byte 208 */
#define CMIS_DIAG_MASK_GATING_MEDIA	0xD1  /* Byte 209 */
#define CMIS_DIAG_MASK_GEN_LOL_HOST	0xD2  /* Byte 210 */
#define CMIS_DIAG_MASK_GEN_LOL_MEDIA	0xD3  /* Byte 211 */
#define CMIS_DIAG_MASK_CHK_LOL_HOST	0xD4  /* Byte 212 */
#define CMIS_DIAG_MASK_CHK_LOL_MEDIA	0xD5  /* Byte 213 */

/* Host Scratchpad (Page 13h, bytes 184-191) */
#define CMIS_DIAG_SCRATCHPAD_OFFSET	0xB8  /* 8 bytes, RW */
#define CMIS_DIAG_SCRATCHPAD_SIZE	8

/* User Pattern (Page 13h, bytes 224-255) */
#define CMIS_DIAG_USER_PATTERN_OFFSET	0xE0  /* 32 bytes, RW */
#define CMIS_DIAG_USER_PATTERN_SIZE	32

/* Number of PRBS patterns (Table 8-105) */
#define CMIS_DIAG_NUM_PRBS_PATTERNS	16

/*-----------------------------------------------------------------------
 * Data Path Control (Page 10h, banked) — OIF-CMIS-05.3 Table 8-47
 */
#define CMIS_DP_DEINIT_OFFSET		0x80  /* DPDeinitLane<1-8>, 1 bit each */
#define CMIS_INPUT_POL_FLIP_TX		0x81  /* InputPolarityFlipTx<1-8> */
#define CMIS_OUTPUT_DIS_TX		0x82  /* OutputDisableTx<1-8> */
#define CMIS_AUTO_SQUELCH_DIS_TX	0x83  /* AutoSquelchDisableTx */
#define CMIS_OUTPUT_SQUELCH_FORCE_TX	0x84  /* OutputSquelchForceTx */
#define CMIS_ADAPT_INPUT_EQ_FREEZE_TX	0x85  /* AdaptiveInputEqFreezeTx */
#define CMIS_ADAPT_INPUT_EQ_STORE_TX	0x87  /* AdaptiveInputEqStoreTx (WO) 2 bytes */
#define CMIS_OUTPUT_POL_FLIP_RX		0x89  /* OutputPolarityFlipRx<1-8> */
#define CMIS_OUTPUT_DIS_RX		0x8A  /* OutputDisableRx<1-8> */
#define CMIS_AUTO_SQUELCH_DIS_RX	0x8B  /* AutoSquelchDisableRx<1-8> */

/* Staged Control Set 0 (SCS0) — Page 10h */
#define CMIS_SCS0_APPLY_DPINIT		0x8F  /* ApplyDPInit<1-8> WO */
#define CMIS_SCS0_APPLY_IMMEDIATE	0x90  /* ApplyImmediate<1-8> WO */
#define CMIS_SCS0_DPCONFIG_LANE(n)	(0x91 + (n))  /* n=0..7 */
#define CMIS_SCS0_ADAPT_EQ_TX		0x99  /* AdaptiveInputEqEnableTx */
#define CMIS_SCS0_ADAPT_EQ_RECALL_TX	0x9A  /* AdaptiveInputEqRecallTx (2 bytes, 2 bits/lane) */
#define CMIS_SCS0_FIXED_INPUT_TX(n)	(0x9C + (n) / 2) /* HostControlledInputEqTargetTx 4-bit/lane */
#define CMIS_SCS0_CDR_ENABLE_TX		0xA0  /* CDREnableTx<1-8> */
#define CMIS_SCS0_CDR_ENABLE_RX		0xA1  /* CDREnableRx<1-8> */
#define CMIS_SCS0_RX_OUT_EQ_PRE(n)	(0xA2 + (n) / 2) /* OutputEqPreCursorTargetRx 4-bit/lane */
#define CMIS_SCS0_RX_OUT_EQ_POST(n)	(0xA6 + (n) / 2) /* OutputEqPostCursorTargetRx 4-bit/lane */
#define CMIS_SCS0_RX_OUT_AMP(n)		(0xAA + (n) / 2) /* OutputAmplitudeTargetRx 4-bit/lane */

/* Unidirectional Apply Triggers (conditional) */
#define CMIS_SCS0_APPLY_IMMEDIATE_TX	0xB0  /* ApplyImmediateTx WO */
#define CMIS_SCS0_APPLY_IMMEDIATE_RX	0xB1  /* ApplyImmediateRx WO */

/* Lane-Specific Masks (Page 10h, bytes 213-232) */
#define CMIS_MASK_DP_STATE_CHANGED	0xD5  /* DPStateChangedMask<1-8> */
#define CMIS_MASK_TX_FAULT		0xD6  /* TxFaultMask<1-8> */
#define CMIS_MASK_TX_LOS		0xD7  /* TxLOSMask<1-8> */
#define CMIS_MASK_TX_CDR_LOL		0xD8  /* TxCDRLOLMask<1-8> */
#define CMIS_MASK_TX_ADAPT_EQ_FAULT	0xD9  /* TxAdaptEqFaultMask<1-8> */
#define CMIS_MASK_TX_PWR_HI_ALARM	0xDA  /* TxPwrHiAlarmMask<1-8> */
#define CMIS_MASK_TX_PWR_LO_ALARM	0xDB  /* TxPwrLoAlarmMask<1-8> */
#define CMIS_MASK_TX_PWR_HI_WARN	0xDC  /* TxPwrHiWarnMask<1-8> */
#define CMIS_MASK_TX_PWR_LO_WARN	0xDD  /* TxPwrLoWarnMask<1-8> */
#define CMIS_MASK_TX_BIAS_HI_ALARM	0xDE  /* TxBiasHiAlarmMask<1-8> */
#define CMIS_MASK_TX_BIAS_LO_ALARM	0xDF  /* TxBiasLoAlarmMask<1-8> */
#define CMIS_MASK_TX_BIAS_HI_WARN	0xE0  /* TxBiasHiWarnMask<1-8> */
#define CMIS_MASK_TX_BIAS_LO_WARN	0xE1  /* TxBiasLoWarnMask<1-8> */
#define CMIS_MASK_RX_LOS		0xE2  /* RxLOSMask<1-8> */
#define CMIS_MASK_RX_CDR_LOL		0xE3  /* RxCDRLOLMask<1-8> */
#define CMIS_MASK_RX_PWR_HI_ALARM	0xE4  /* RxPwrHiAlarmMask<1-8> */
#define CMIS_MASK_RX_PWR_LO_ALARM	0xE5  /* RxPwrLoAlarmMask<1-8> */
#define CMIS_MASK_RX_PWR_HI_WARN	0xE6  /* RxPwrHiWarnMask<1-8> */
#define CMIS_MASK_RX_PWR_LO_WARN	0xE7  /* RxPwrLoWarnMask<1-8> */
#define CMIS_MASK_RX_OUTPUT_STATUS	0xE8  /* RxOutputStatusChangedMask<1-8> */

/* DPConfigLane<n> bit fields */
#define CMIS_DPCONFIG_APPSEL_MASK	0xF0
#define CMIS_DPCONFIG_APPSEL_SHIFT	4
#define CMIS_DPCONFIG_DPID_MASK		0x0E
#define CMIS_DPCONFIG_DPID_SHIFT	1
#define CMIS_DPCONFIG_EXPLICIT_MASK	0x01

/*-----------------------------------------------------------------------
 * Data Path Status (Page 11h extended, banked) — OIF-CMIS-05.3 Table 8-50
 */
#define CMIS_DP_STATE_HOST_OFFSET	0x80  /* DPStateHostLane<1-8>, 4-bit nibbles */
#define CMIS_OUTPUT_STATUS_RX		0x84  /* OutputStatusRx<1-8>, 1 bit each */
#define CMIS_OUTPUT_STATUS_TX		0x85  /* OutputStatusTx<1-8>, 1 bit each */
#define CMIS_DP_STATE_CHANGED_FLAG	0x86  /* DPStateChangedFlag<1-8> */

/* Active Control Set (ACS, RO) — Page 11h */
#define CMIS_ACS_DPCONFIG_LANE(n)	(0xCE + (n))  /* n=0..7 */
#define CMIS_ACS_ADAPT_EQ_TX		0xD6  /* AdaptiveInputEqEnableTx */
#define CMIS_ACS_CDR_ENABLE_TX		0xDD  /* CDREnableTx<1-8> */
#define CMIS_ACS_CDR_ENABLE_RX		0xDE  /* CDREnableRx<1-8> */

/* Config Status per lane — Page 11h */
#define CMIS_CONFIG_STATUS_LANE_OFFSET	0xCA  /* ConfigStatusLane<1-8>, 4 bits each */
#define CMIS_CONFIG_STATUS_MASK		0x0F

/* Config status codes (Table 8-50) */
#define CMIS_CFGSTAT_UNDEFINED		0x0
#define CMIS_CFGSTAT_SUCCESS		0x1
#define CMIS_CFGSTAT_REJECTED		0x2
#define CMIS_CFGSTAT_REJECTED_INVALID	0x3
#define CMIS_CFGSTAT_IN_PROGRESS	0x4
#define CMIS_CFGSTAT_REJECTED_DPID	0x5
#define CMIS_CFGSTAT_REJECTED_APPSEL	0x6
#define CMIS_CFGSTAT_REJECTED_LANES_IN_USE 0x7
#define CMIS_CFGSTAT_REJECTED_PARTIAL	0x8
#define CMIS_CFGSTAT_CUSTOM_START	0xC

/* Per-lane latched flags (Page 11h, bytes 135-152, COR) — Table 8-50 */
#define CMIS_FLAG_TX_FAULT		0x87  /* L-TxFaultLane<1-8> */
#define CMIS_FLAG_TX_LOS		0x88  /* L-TxLOSLane<1-8> */
#define CMIS_FLAG_TX_CDR_LOL		0x89  /* L-TxCDRLOLLane<1-8> */
#define CMIS_FLAG_TX_ADAPT_EQ_FAULT	0x8A  /* L-TxAdaptEqFaultLane<1-8> */
#define CMIS_FLAG_TX_PWR_HI_ALARM	0x8B  /* L-TxPwrHiAlarmLane<1-8> */
#define CMIS_FLAG_TX_PWR_LO_ALARM	0x8C  /* L-TxPwrLoAlarmLane<1-8> */
#define CMIS_FLAG_TX_PWR_HI_WARN	0x8D  /* L-TxPwrHiWarnLane<1-8> */
#define CMIS_FLAG_TX_PWR_LO_WARN	0x8E  /* L-TxPwrLoWarnLane<1-8> */
#define CMIS_FLAG_TX_BIAS_HI_ALARM	0x8F  /* L-TxBiasHiAlarmLane<1-8> */
#define CMIS_FLAG_TX_BIAS_LO_ALARM	0x90  /* L-TxBiasLoAlarmLane<1-8> */
#define CMIS_FLAG_TX_BIAS_HI_WARN	0x91  /* L-TxBiasHiWarnLane<1-8> */
#define CMIS_FLAG_TX_BIAS_LO_WARN	0x92  /* L-TxBiasLoWarnLane<1-8> */
#define CMIS_FLAG_RX_LOS		0x93  /* L-RxLOSLane<1-8> */
#define CMIS_FLAG_RX_CDR_LOL		0x94  /* L-RxCDRLOLLane<1-8> */
#define CMIS_FLAG_RX_PWR_HI_ALARM	0x95  /* L-RxPwrHiAlarmLane<1-8> */
#define CMIS_FLAG_RX_PWR_LO_ALARM	0x96  /* L-RxPwrLoAlarmLane<1-8> */
#define CMIS_FLAG_RX_PWR_HI_WARN	0x97  /* L-RxPwrHiWarnLane<1-8> */
#define CMIS_FLAG_RX_PWR_LO_WARN	0x98  /* L-RxPwrLoWarnLane<1-8> */

/* DPInitPending — Page 11h */
#define CMIS_DP_INIT_PENDING		0xEB  /* DPInitPendingLane<1-8> */

/*-----------------------------------------------------------------------
 * Data Path Latency (Page 15h, banked) — OIF-CMIS-05.3 Table 8-131
 */
#define CMIS_DP_LATENCY_ADVER_OFFSET	0x91  /* Page 01h byte 145 */
#define CMIS_DP_LATENCY_ADVER_MASK	0x08  /* bit 3: Page 15h supported */

#define CMIS_DP_RX_LATENCY_LANE(n)	(0xE0 + 2 * (n))  /* U16, 1 ns */
#define CMIS_DP_TX_LATENCY_LANE(n)	(0xF0 + 2 * (n))  /* U16, 1 ns */

/* MediaLaneMapping — Page 11h, bytes 240-255 */
#define CMIS_MEDIA_LANE_MAP_TX(n)	(0xF0 + (n))  /* n=0..7 */
#define CMIS_MEDIA_LANE_MAP_RX(n)	(0xF8 + (n))  /* n=0..7 */

/*-----------------------------------------------------------------------
 * Network Path Control/Status (Pages 16h-17h, banked) — OIF-CMIS-05.3 8.15
 */

/* Page 16h: NP Staged Control Set 0 (bytes 128-135) */
#define CMIS_NP_SCS0_CONFIG_LANE(n)	(0x80 + (n))  /* n=0..7 */
/* NP Staged Control Set 1 (bytes 136-143) */
#define CMIS_NP_SCS1_CONFIG_LANE(n)	(0x88 + (n))  /* n=0..7 */

/* NPConfigLane<i> bit fields (Table 8-133) */
#define CMIS_NP_NPID_MASK		0xF0
#define CMIS_NP_NPID_SHIFT		4
#define CMIS_NP_INUSE_MASK		0x01

/* NP Initialization Control (byte 160) */
#define CMIS_NP_DEINIT_OFFSET		0xA0  /* NPDeinitLane<1-8>, 1 bit each */

/* Signal source selection (bytes 162-163) */
#define CMIS_NP_HPSOURCE_RX		0xA2  /* 0=NP signal, 1=internal repl */
#define CMIS_NP_NPSOURCE_TX		0xA3  /* 0=HP signal, 1=internal repl */

/* Apply triggers (bytes 176-177, WO) */
#define CMIS_NP_APPLY_SCS0		0xB0
#define CMIS_NP_APPLY_SCS1		0xB1

/* NP Configuration Status (bytes 178-181, 4-bit nibbles) */
#define CMIS_NP_CONFIG_STATUS_OFFSET	0xB2

/* NPConfigStatus codes (Table 8-141) */
#define CMIS_NP_CFGSTAT_UNDEFINED		0x0
#define CMIS_NP_CFGSTAT_SUCCESS			0x1
#define CMIS_NP_CFGSTAT_REJECTED		0x2
#define CMIS_NP_CFGSTAT_REJECTED_INVALID_APPSEL	0x3
#define CMIS_NP_CFGSTAT_REJECTED_INVALID_NP	0x4
#define CMIS_NP_CFGSTAT_REJECTED_LANES_IN_USE	0x6
#define CMIS_NP_CFGSTAT_REJECTED_PARTIAL	0x7
#define CMIS_NP_CFGSTAT_IN_PROGRESS		0xC

/* NP Active Control Set (bytes 192-199, RO) */
#define CMIS_NP_ACS_CONFIG_LANE(n)	(0xC0 + (n))  /* n=0..7 */

/* NP State per lane (bytes 200-203, 4-bit nibbles) */
#define CMIS_NP_STATE_OFFSET		0xC8

/* NP State encoding (Table 8-144) */
#define CMIS_NP_STATE_DEACTIVATED	0x1
#define CMIS_NP_STATE_INIT		0x2
#define CMIS_NP_STATE_DEINIT		0x3
#define CMIS_NP_STATE_ACTIVATED		0x4
#define CMIS_NP_STATE_TX_TURN_ON	0x5
#define CMIS_NP_STATE_TX_TURN_OFF	0x6
#define CMIS_NP_STATE_INITIALIZED	0x7

/* NP Init Pending (byte 204, 1 bit per lane) */
#define CMIS_NP_INIT_PENDING		0xCC

/* NPSM Max Durations (bytes 224-225, Table 8-146) */
#define CMIS_NP_MAX_DURATION_INIT	0xE0  /* bits 3-0=NPInit, 7-4=NPDeinit */
#define CMIS_NP_MAX_DURATION_TXON	0xE1  /* bits 3-0=NPTxTurnOn, 7-4=Off */

/* Misc options (byte 226, Table 8-147) */
#define CMIS_NP_MISC_OPTIONS		0xE2
#define CMIS_NP_REPLACE_HP_TX_SUPPORTED	0x02  /* bit 1 */
#define CMIS_NP_REPLACE_HP_RX_SUPPORTED	0x01  /* bit 0 */

/* Page 17h: NP Flags and Masks (Table 8-152) */
#define CMIS_NP_STATE_CHANGED_FLAG	0x80  /* byte 128, 1 bit per lane */
#define CMIS_NP_STATE_CHANGED_MASK	0xC0  /* byte 192, 1 bit per lane */

/*-----------------------------------------------------------------------
 * User EEPROM (Page 03h, optional) — OIF-CMIS-05.3 Section 8.3
 */
#define CMIS_USER_EEPROM_PAGE		0x03
#define CMIS_USER_EEPROM_OFFSET		0x80  /* bytes 128-255 (128 bytes) */
#define CMIS_USER_EEPROM_SIZE		128
#define CMIS_USER_EEPROM_MAX_WRITE	8     /* max 8 bytes per write */

/*-----------------------------------------------------------------------
 * Host Lane Switching (Page 1Dh, banked) — OIF-CMIS-05.3 Section 8.18
 */
#define CMIS_LANE_SWITCH_REDIR_LANE(n)	(0x88 + (n))  /* n=0..7, RedirectionOfLane */
#define CMIS_LANE_SWITCH_ENABLE		0x98  /* byte 152, bit 0 */
#define CMIS_LANE_SWITCH_COMMIT		0xA0  /* byte 160, bit 0, WO/SC */
#define CMIS_LANE_SWITCH_RESULT		0xA8  /* byte 168, RO */

/*-----------------------------------------------------------------------
 * VDM Pages (non-banked, CMIS 8.22)
 */
#define CMIS_VDM_ADVER_PAGE		0x2F
#define CMIS_VDM_SUPPORT_OFFSET		0x80  /* bits 1-0: num groups - 1 */
#define CMIS_VDM_SUPPORT_MASK		0x03
#define CMIS_VDM_FINE_INTERVAL_MSB	0x81  /* U16, 0.1 ms */
#define CMIS_VDM_FINE_INTERVAL_LSB	0x82
#define CMIS_VDM_FREEZE_OFFSET		0x90  /* bit 7: FreezeRequest */
#define CMIS_VDM_FREEZE_REQUEST		0x80
#define CMIS_VDM_FREEZE_DONE		0x40
#define CMIS_VDM_POWER_SAVING		0x40  /* byte 144, bit 6: PowerSavingMode */
#define CMIS_VDM_UNFREEZE_OFFSET	0x91
#define CMIS_VDM_UNFREEZE_DONE		0x80

#define CMIS_VDM_DESC_PAGE_BASE		0x20  /* Pages 20h-23h */
#define CMIS_VDM_SAMPLE_PAGE_BASE	0x24  /* Pages 24h-27h */
#define CMIS_VDM_THRESH_PAGE_BASE	0x28  /* Pages 28h-2Bh */
#define CMIS_VDM_FLAGS_PAGE		0x2C
#define CMIS_VDM_MASKS_PAGE		0x2D

#define CMIS_VDM_INSTANCES_PER_GROUP	64
#define CMIS_VDM_MAX_GROUPS		4

/* Host Lane Switching Advertisement (Page 01h, byte 252) */
#define CMIS_LANE_SWITCH_ADVER_OFFSET	0xFC
#define CMIS_LANE_SWITCH_ADVER_MASK	0x80  /* bit 7 */

/*-----------------------------------------------------------------------
 * Extended Module Capabilities (Page 01h, bytes 155-162)
 * OIF-CMIS-05.3 Table 8-41
 */
#define CMIS_EXT_CAPS_OFFSET		0x9B  /* byte 155 */

/* Byte 155 (0x9B) */
#define CMIS_ECAP_FORCED_SQUELCH	0x80  /* bit 7 */
#define CMIS_ECAP_AUTO_SQUELCH_DIS	0x40  /* bit 6 */
#define CMIS_ECAP_OUTPUT_DIS_TX		0x20  /* bit 5 */
#define CMIS_ECAP_INPUT_POL_FLIP	0x10  /* bit 4 */
#define CMIS_ECAP_OUTPUT_POL_FLIP	0x08  /* bit 3 */
#define CMIS_ECAP_BANK_BROADCAST	0x04  /* bit 2 */

/* Byte 156 (0x9C): Tx flag advertisement bitmask */
#define CMIS_ECAP_TX_FLAGS_OFFSET	0x9C

/* Byte 157 (0x9D): already defined as CMIS_DIAG_FLAGS_TX_OFFSET */
/* Byte 158 (0x9E): already defined as CMIS_DIAG_FLAGS_RX_OFFSET */

/* Byte 159 (0x9F) */
#define CMIS_ECAP_MONITORS_OFFSET	0x9F
#define CMIS_ECAP_CUSTOM_MON_TX	0x80  /* bit 7 */
#define CMIS_ECAP_CUSTOM_MON_RX	0x40  /* bit 6 */

/* Byte 160 (0xA0) */
#define CMIS_ECAP_CDR_EQ_OFFSET		0xA0
#define CMIS_ECAP_CDR_BYPASS_TX		0x80  /* bit 7 */
#define CMIS_ECAP_CDR_BYPASS_RX		0x40  /* bit 6 */
#define CMIS_ECAP_ADAPT_EQ_TX		0x20  /* bit 5 */
#define CMIS_ECAP_FIXED_EQ_TX		0x10  /* bit 4 */
#define CMIS_ECAP_EQ_RECALL_BUF_TX	0x0C  /* bits 3-2 */
#define CMIS_ECAP_EQ_RECALL_SHIFT_TX	2

/* Byte 161 (0xA1): already defined as CMIS_SIG_INTEG_TX_OFFSET */
/* Byte 162 (0xA2): already defined as CMIS_SIG_INTEG_RX_OFFSET */

/* C-CMIS coherent pages go up to Page 44h. */
#define CMIS_MAX_PAGES		0x45

struct cmis_memory_map {
	const __u8 *lower_memory;
	const __u8 *upper_memory[CMIS_MAX_BANKS][CMIS_MAX_PAGES];
	int media_lane_count;
};

/* Return the number of media lanes to display for a given bank.
 * Falls back to CMIS_CHANNELS_PER_BANK when media_lane_count is unknown.
 */
static inline int cmis_lanes_per_bank(const struct cmis_memory_map *map,
				      int bank)
{
	int total = map->media_lane_count;
	int remaining;

	if (total <= 0)
		return CMIS_CHANNELS_PER_BANK;

	remaining = total - bank * CMIS_CHANNELS_PER_BANK;
	if (remaining <= 0)
		return 0;
	return remaining < CMIS_CHANNELS_PER_BANK ? remaining
						   : CMIS_CHANNELS_PER_BANK;
}

#define CMIS_PAGE_SIZE		0x80
#define CMIS_I2C_ADDRESS	0x50

struct module_eeprom;

/*
 * Quirk flags -- set at runtime when buggy modules are detected.
 * Checked by library and test code to enable workarounds.
 */
extern int cmis_quirk_coherent_adver;	/* 01h:142 bit 4 missing on coherent */

void cmis_quirk_check_coherent(struct cmd_context *ctx,
			       struct cmis_memory_map *map);

void cmis_request_init(struct module_eeprom *request, u8 bank,
		       u8 page, u32 offset);

void cmis_show_all_common(const struct cmis_memory_map *map);
int cmis_memory_map_init_pages(struct cmd_context *ctx,
			       struct cmis_memory_map *map);
int cmis_num_banks_get(const struct cmis_memory_map *map,
		       int *p_num_banks);

#endif /* CMIS_INTERNAL_H__ */
