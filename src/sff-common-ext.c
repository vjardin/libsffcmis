/*
 * sff-common-ext.c: SFF-8024 Rev 4.13 interface ID lookup tables
 *
 * Provides host, media, and extended specification compliance name
 * resolution for CMIS and SFF-8636 modules.
 *
 * Copyright (C) 2025 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <linux/types.h>
#include <stdint.h>
#include "sff-common-ext.h"

/*-----------------------------------------------------------------------
 * SFF-8024 interface ID lookup tables (Tables 4-4 through 4-10)
 *-----------------------------------------------------------------------*/

/* Module type codes (CMIS) -- needed by sff8024_media_id_name() */
#define SFF8024_MT_MMF			0x01
#define SFF8024_MT_SMF			0x02
#define SFF8024_MT_PASSIVE_COPPER	0x03
#define SFF8024_MT_ACTIVE_CABLE		0x04
#define SFF8024_MT_BASE_T		0x05

static const char *sff8024_id_lookup(const struct sff8024_intf_id *table,
				     int size, __u8 id)
{
	int i;

	if (id == 0)
		return NULL;
	if (id >= 0xC0)
		return "Vendor specific";

	for (i = 0; i < size; i++) {
		if (table[i].id == id)
			return table[i].name;
	}
	return NULL;
}

/* SFF-8024 Rev 4.13 Table 4-4: Extended Specification Compliance Codes */
static const struct sff8024_intf_id sff8024_ext_spec_ids[] = {
	{ 0x01, "100G AOC, retimed or 25GAUI C2M AOC (BER 5e-5)" },
	{ 0x02, "100GBASE-SR4 or 25GBASE-SR" },
	{ 0x03, "100GBASE-LR4 or 25GBASE-LR" },
	{ 0x04, "100GBASE-ER4 or 25GBASE-ER" },
	{ 0x05, "100GBASE-SR10" },
	{ 0x06, "100G CWDM4" },
	{ 0x07, "100G PSM4 Parallel SMF" },
	{ 0x08, "100G ACC, retimed or 25GAUI C2M ACC (BER 5e-5)" },
	{ 0x09, "Obsolete (100G CWDM4 MSA, different FEC requirements)" },
	{ 0x0B, "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with RS FEC" },
	{ 0x0C, "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R FEC" },
	{ 0x0D, "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC" },
	{ 0x0E, "10 Mb/s Single Pair Ethernet (802.3cg, Clause 146/147, 1000m copper)" },
	{ 0x10, "40GBASE-ER4" },
	{ 0x11, "4 x 10GBASE-SR" },
	{ 0x12, "40G PSM4 Parallel SMF" },
	{ 0x13, "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310nm SM)" },
	{ 0x14, "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550nm SM)" },
	{ 0x15, "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550nm SM)" },
	{ 0x16, "10GBASE-T with SFI electrical interface" },
	{ 0x17, "100G CLR4" },
	{ 0x18, "100G AOC, retimed or 25GAUI C2M AOC (BER 1e-12)" },
	{ 0x19, "100G ACC, retimed or 25GAUI C2M ACC (BER 1e-12)" },
	{ 0x1A, "100GE-DWDM2 (2 wavelengths, 1550nm DWDM, up to 80km)" },
	{ 0x1B, "100G 1550nm WDM (4 wavelengths)" },
	{ 0x1C, "10GBASE-T Short Reach (30m)" },
	{ 0x1D, "5GBASE-T" },
	{ 0x1E, "2.5GBASE-T" },
	{ 0x1F, "40G SWDM4" },
	{ 0x20, "100G SWDM4" },
	{ 0x21, "100G PAM4 BiDi" },
	{ 0x22, "4WDM-10 MSA (10km, 100G CWDM4 with RS(528,514) FEC)" },
	{ 0x23, "4WDM-20 MSA (20km, 100GBASE-LR4 with RS(528,514) FEC)" },
	{ 0x24, "4WDM-40 MSA (40km, APD receiver with RS(528,514) FEC)" },
	{ 0x25, "100GBASE-DR (Clause 140), CAUI-4 (no FEC)" },
	{ 0x26, "100G-FR or 100GBASE-FR1 (Clause 140), CAUI-4 (no FEC)" },
	{ 0x27, "100G-LR or 100GBASE-LR1 (Clause 140), CAUI-4 (no FEC)" },
	{ 0x28, "100GBASE-SR1 (Clause 167), CAUI-4 (no FEC)" },
	{ 0x29, "100GBASE-SR1, 200GBASE-SR2 or 400GBASE-SR4 (Clause 167)" },
	{ 0x2A, "100GBASE-FR1 (Clause 140) or 400GBASE-DR4-2 (Clause 124)" },
	{ 0x2B, "100GBASE-LR1 (Clause 140)" },
	{ 0x2C, "100G-LR1-20 MSA, CAUI-4 (no FEC)" },
	{ 0x2D, "100G-ER1-30 MSA, CAUI-4 (no FEC)" },
	{ 0x2E, "100G-ER1-40 MSA, CAUI-4 (no FEC)" },
	{ 0x2F, "100G-LR1-20 MSA" },
	{ 0x30, "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M (BER 1e-6)" },
	{ 0x31, "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M (BER 1e-6)" },
	{ 0x32, "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M (BER 2.6e-4 ACC, 1e-5 AUI)" },
	{ 0x33, "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M (BER 2.6e-4 AOC, 1e-5 AUI)" },
	{ 0x34, "100G-ER1-30 MSA" },
	{ 0x35, "100G-ER1-40 MSA" },
	{ 0x36, "100GBASE-VR1, 200GBASE-VR2 or 400GBASE-VR4 (Clause 167)" },
	{ 0x37, "10GBASE-BR (Clause 158)" },
	{ 0x38, "25GBASE-BR (Clause 159)" },
	{ 0x39, "50GBASE-BR (Clause 160)" },
	{ 0x3A, "100GBASE-VR1 (Clause 167), CAUI-4 (no FEC)" },
	{ 0x3F, "100GBASE-CR1, 200GBASE-CR2 or 400GBASE-CR4 (Clause 162)" },
	{ 0x40, "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4" },
	{ 0x41, "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4" },
	{ 0x42, "50GBASE-FR or 200GBASE-DR4" },
	{ 0x43, "200GBASE-FR4" },
	{ 0x44, "200G 1550 nm PSM4" },
	{ 0x45, "50GBASE-LR" },
	{ 0x46, "200GBASE-LR4" },
	{ 0x47, "400GBASE-DR4 (Clause 124), 400GAUI-4 C2M" },
	{ 0x48, "400GBASE-FR4 (Clause 151)" },
	{ 0x49, "400GBASE-LR4-6 (Clause 151)" },
	{ 0x4A, "50GBASE-ER (Clause 139)" },
	{ 0x4B, "400G-LR4-10" },
	{ 0x4C, "400GBASE-ZR (Clause 156), obsolete" },
	/* Fibre Channel */
	{ 0x7F, "256GFC-SW4 (FC-PI-7P)" },
	{ 0x80, "64GFC (FC-PI-7)" },
	{ 0x81, "128GFC (FC-PI-8)" },
};

const char *sff8024_ext_spec_compliance_name(__u8 id)
{
	return sff8024_id_lookup(sff8024_ext_spec_ids,
				SFF8024_TBL_SZ(sff8024_ext_spec_ids), id);
}

/* SFF-8024 Rev 4.13 Table 4-5: Host Electrical Interface IDs */
static const struct sff8024_intf_id sff8024_host_ids[] = {
	/* Ethernet */
	{ 0x01, "1000BASE-CX" },
	{ 0x02, "XAUI" },
	{ 0x03, "XFI" },
	{ 0x04, "SFI" },
	{ 0x05, "25GAUI C2M" },
	{ 0x06, "XLAUI C2M" },
	{ 0x07, "XLPPI" },
	{ 0x08, "LAUI-2 C2M" },
	{ 0x09, "50GAUI-2 C2M" },
	{ 0x0A, "50GAUI-1 C2M" },
	{ 0x0B, "CAUI-4 C2M" },
	{ 0x0C, "100GAUI-4 C2M" },
	{ 0x0D, "100GAUI-2 C2M" },
	{ 0x0E, "200GAUI-8 C2M" },
	{ 0x0F, "200GAUI-4 C2M" },
	{ 0x10, "400GAUI-16 C2M" },
	{ 0x11, "400GAUI-8 C2M" },
	{ 0x13, "10GBASE-CX4" },
	{ 0x14, "25GBASE-CR CA-L" },
	{ 0x15, "25GBASE-CR CA-S" },
	{ 0x16, "25GBASE-CR CA-N" },
	{ 0x17, "40GBASE-CR4" },
	{ 0x18, "50GBASE-CR" },
	{ 0x19, "100GBASE-CR10" },
	{ 0x1A, "100GBASE-CR4" },
	{ 0x1B, "100GBASE-CR2" },
	{ 0x1C, "200GBASE-CR4" },
	{ 0x1D, "400G CR8" },
	{ 0x1E, "200GBASE-CR1" },
	{ 0x1F, "400GBASE-CR2" },
	{ 0x20, "LEI-100G-PAM4-1 (LPO)" },
	{ 0x21, "LEI-200G-PAM4-2 (LPO)" },
	{ 0x22, "LEI-400G-PAM4-4 (LPO)" },
	{ 0x23, "LEI-800G-PAM4-8 (LPO)" },
	/* Fibre Channel */
	{ 0x25, "8GFC" },
	{ 0x26, "10GFC" },
	{ 0x27, "16GFC" },
	{ 0x28, "32GFC" },
	{ 0x29, "64GFC" },
	{ 0x2A, "128GFC (FC-PI-6P)" },
	{ 0x2B, "256GFC" },
	/* InfiniBand */
	{ 0x2C, "IB SDR" },
	{ 0x2D, "IB DDR" },
	{ 0x2E, "IB QDR" },
	{ 0x2F, "IB FDR" },
	{ 0x30, "IB EDR" },
	{ 0x31, "IB HDR" },
	{ 0x32, "IB NDR" },
	/* CPRI */
	{ 0x33, "E.96 CPRI" },
	{ 0x34, "E.99 CPRI" },
	{ 0x35, "E.119 CPRI" },
	{ 0x36, "E.238 CPRI" },
	/* OTN */
	{ 0x37, "OTL3.4" },
	{ 0x38, "OTL4.10" },
	{ 0x39, "OTL4.4" },
	{ 0x3A, "OTLC.4" },
	{ 0x3B, "FOIC1.4-MFI" },
	{ 0x3C, "FOIC1.2-MFI" },
	{ 0x3D, "FOIC2.8-MFI" },
	{ 0x3E, "FOIC2.4-MFI" },
	{ 0x3F, "FOIC4.16-MFI" },
	{ 0x40, "FOIC4.8-MFI" },
	{ 0x41, "CAUI-4 C2M w/o FEC" },
	{ 0x42, "CAUI-4 C2M w/ RS FEC" },
	{ 0x43, "50GBASE-CR2 w/ RS FEC" },
	{ 0x44, "50GBASE-CR2 w/ Fire code FEC" },
	{ 0x45, "50GBASE-CR2 w/o FEC" },
	{ 0x46, "100GBASE-CR1" },
	{ 0x47, "200GBASE-CR2" },
	{ 0x48, "400GBASE-CR4" },
	{ 0x49, "800G-ETC-CR8" },
	{ 0x4A, "128GFC (FC-PI-8)" },
	{ 0x4B, "100GAUI-1-S C2M" },
	{ 0x4C, "100GAUI-1-L C2M" },
	{ 0x4D, "200GAUI-2-S C2M" },
	{ 0x4E, "200GAUI-2-L C2M" },
	{ 0x4F, "400GAUI-4-S C2M" },
	{ 0x50, "400GAUI-4-L C2M" },
	{ 0x51, "800GAUI-8 S C2M" },
	{ 0x52, "800GAUI-8 L C2M" },
	{ 0x53, "OTL4.2" },
	{ 0x55, "1.6TAUI-16-S C2M" },
	{ 0x56, "1.6TAUI-16-L C2M" },
	{ 0x57, "800GBASE-CR4" },
	{ 0x58, "1.6TBASE-CR8" },
	/* PCIe */
	{ 0x70, "PCIe 4.0" },
	{ 0x71, "PCIe 5.0" },
	{ 0x72, "PCIe 6.0" },
	{ 0x73, "PCIe 7.0" },
	/* OIF */
	{ 0x74, "CEI-112G-LINEAR-PAM4" },
	{ 0x80, "200GAUI-1" },
	{ 0x81, "400GAUI-2" },
	{ 0x82, "800GAUI-4" },
	{ 0x83, "1.6TAUI-8" },
	{ 0x90, "EEI-100G-RTLR-1-S" },
	{ 0x91, "EEI-100G-RTLR-1-L" },
	{ 0x92, "EEI-200G-RTLR-2-S" },
	{ 0x93, "EEI-200G-RTLR-2-L" },
	{ 0x94, "EEI-400G-RTLR-4-S" },
	{ 0x95, "EEI-400G-RTLR-4-L" },
	{ 0x96, "EEI-800G-RTLR-8-S" },
	{ 0x97, "EEI-800G-RTLR-8-L" },
	/* InfiniBand (cont.) */
	{ 0xA0, "IB XDR" },
	/* OTN (cont.) */
	{ 0xB0, "FOIC1.1-MFI" },
	{ 0xB1, "FOIC4.4-MFI" },
	{ 0xB2, "FOIC8.8-MFI" },
	/* PON */
	{ 0xB7, "ITU-T G.9804.3" },
};

/* SFF-8024 Rev 4.13 Table 4-6: MMF media interface IDs */
static const struct sff8024_intf_id sff8024_mmf_media_ids[] = {
	/* Ethernet */
	{ 0x01, "10GBASE-SW" },
	{ 0x02, "10GBASE-SR" },
	{ 0x03, "25GBASE-SR" },
	{ 0x04, "40GBASE-SR4" },
	{ 0x05, "40GE SWDM4" },
	{ 0x06, "40GE BiDi" },
	{ 0x07, "50GBASE-SR" },
	{ 0x08, "100GBASE-SR10" },
	{ 0x09, "100GBASE-SR4" },
	{ 0x0A, "100GE SWDM4" },
	{ 0x0B, "100GE BiDi" },
	{ 0x0C, "100GBASE-SR2" },
	{ 0x0D, "100GBASE-SR1" },
	{ 0x0E, "200GBASE-SR4" },
	{ 0x0F, "400GBASE-SR16" },
	{ 0x10, "400GBASE-SR8" },
	{ 0x11, "400GBASE-SR4" },
	{ 0x12, "800GBASE-SR8" },
	{ 0x1A, "400GBASE-SR4.2 (400GE BiDi)" },
	{ 0x1B, "200GBASE-SR2" },
	{ 0x1C, "128GFC-MM (FC-PI-8)" },
	{ 0x1D, "100GBASE-VR1" },
	{ 0x1E, "200GBASE-VR2" },
	{ 0x1F, "400GBASE-VR4" },
	{ 0x20, "800GBASE-VR8" },
	{ 0x21, "800G-VR4.2" },
	{ 0x22, "800G-SR4.2" },
	{ 0x23, "1.6T-VR8.2" },
	{ 0x24, "1.6T-SR8.2" },
	/* Fibre Channel */
	{ 0x13, "8GFC-MM" },
	{ 0x14, "10GFC-MM" },
	{ 0x15, "16GFC-MM" },
	{ 0x16, "32GFC-MM" },
	{ 0x17, "64GFC-MM" },
	{ 0x18, "128GFC-MM4 (FC-PI-6P)" },
	{ 0x19, "256GFC-MM4" },
};

/* SFF-8024 Rev 4.13 Table 4-7: SMF media interface IDs */
static const struct sff8024_intf_id sff8024_smf_media_ids[] = {
	/* Ethernet */
	{ 0x01, "10GBASE-LW" },
	{ 0x02, "10GBASE-SR" },
	{ 0x03, "10G-ZW" },
	{ 0x04, "10GBASE-LR" },
	{ 0x05, "10GBASE-ER" },
	{ 0x06, "10G-ZR" },
	{ 0x07, "25GBASE-LR" },
	{ 0x08, "25GBASE-ER" },
	{ 0x09, "40GBASE-LR4" },
	{ 0x0A, "40GBASE-FR" },
	{ 0x0B, "50GBASE-FR" },
	{ 0x0C, "50GBASE-LR" },
	{ 0x0D, "100GBASE-LR4" },
	{ 0x0E, "100GBASE-ER4" },
	{ 0x0F, "100G PSM4" },
	{ 0x10, "100G CWDM4" },
	{ 0x11, "100G 4WDM-10" },
	{ 0x12, "100G 4WDM-20" },
	{ 0x13, "100G 4WDM-40" },
	{ 0x14, "100GBASE-DR" },
	{ 0x15, "100G-FR/100GBASE-FR1" },
	{ 0x16, "100G-LR/100GBASE-LR1" },
	{ 0x17, "200GBASE-DR4" },
	{ 0x18, "200GBASE-FR4" },
	{ 0x19, "200GBASE-LR4" },
	{ 0x1A, "400GBASE-FR8" },
	{ 0x1B, "400GBASE-LR8" },
	{ 0x1C, "400GBASE-DR4" },
	{ 0x1D, "400G-FR4/400GBASE-FR4" },
	{ 0x1E, "400G-LR4-10" },
	/* Fibre Channel */
	{ 0x1F, "8GFC-SM" },
	{ 0x20, "10GFC-SM" },
	{ 0x21, "16GFC-SM" },
	{ 0x22, "32GFC-SM" },
	{ 0x23, "64GFC-SM" },
	{ 0x24, "128GFC-PSM4" },
	{ 0x26, "128GFC-CWDM4" },
	/* OTN */
	{ 0x2C, "4I1-9D1F" },
	{ 0x2D, "4L1-9C1F" },
	{ 0x2E, "4L1-9D1F" },
	{ 0x2F, "C4S1-9D1F" },
	{ 0x30, "C4S1-4D1F" },
	{ 0x31, "4I1-4D1F" },
	{ 0x32, "8R1-4D1F" },
	{ 0x33, "8I1-4D1F" },
	{ 0x34, "100G CWDM4-OCP" },
	/* CPRI */
	{ 0x38, "10G-SR" },
	{ 0x39, "10G-LR" },
	{ 0x3A, "25G-SR" },
	{ 0x3B, "25G-LR" },
	{ 0x3C, "10G-LR-BiDi" },
	{ 0x3D, "25G-LR-BiDi" },
	/* OIF */
	{ 0x3E, "400ZR DWDM amplified" },
	{ 0x3F, "400ZR Unamplified" },
	{ 0x40, "50GBASE-ER" },
	{ 0x41, "200GBASE-ER4" },
	{ 0x42, "400GBASE-ER8" },
	{ 0x43, "400GBASE-LR4-6" },
	{ 0x44, "100GBASE-ZR" },
	{ 0x45, "128GFC-SM" },
	/* OpenZR+ */
	{ 0x46, "ZR400-OFEC-16QAM" },
	{ 0x47, "ZR300-OFEC-8QAM" },
	{ 0x48, "ZR200-OFEC-QPSK" },
	{ 0x49, "ZR100-OFEC-QPSK" },
	{ 0x4A, "100G-LR1-20" },
	{ 0x4B, "100G-ER1-30" },
	{ 0x4C, "100G-ER1-40" },
	{ 0x4D, "400GBASE-ZR" },
	{ 0x4E, "10GBASE-BR" },
	{ 0x4F, "25GBASE-BR" },
	{ 0x50, "50GBASE-BR" },
	{ 0x51, "FOIC1.4-DO" },
	{ 0x52, "FOIC2.8-DO" },
	{ 0x53, "FOIC4.8-DO" },
	{ 0x54, "FOIC2.4-DO" },
	{ 0x55, "400GBASE-DR4-2" },
	{ 0x56, "800GBASE-DR8" },
	{ 0x57, "800GBASE-DR8-2" },
	{ 0x58, "ZR400-OFEC-8QAM-HB" },
	{ 0x59, "ZR300-OFEC-8QAM-HA" },
	{ 0x5A, "ZR300-OFEC-8QAM-HB" },
	{ 0x5B, "ZR200-OFEC-QPSK-HA" },
	{ 0x5C, "ZR200-OFEC-QPSK-HB" },
	{ 0x5D, "ZR100-OFEC-QPSK-HA" },
	{ 0x5E, "ZR100-OFEC-QPSK-HB" },
	/* Open ROADM */
	{ 0x5F, "FLEXO-4-DO-16QAM/FOIC4.8-DO" },
	{ 0x60, "FLEXO-3-DO-8QAM/FOIC3.6-DO" },
	{ 0x61, "FLEXO-2-DO-QPSK/FOIC2.4-DO" },
	{ 0x62, "FLEXO-2-DO-16QAM/FOIC2.8-DO" },
	{ 0x63, "FLEXO-1-DO-QPSK/FOIC1.4-DO" },
	{ 0x64, "FLEXO-4e-DO-QPSK/FOIC4e.4-DO" },
	{ 0x65, "FLEXO-4-DO-QPSK/FOIC4.4-DO" },
	{ 0x66, "FLEXO-8e-DO-16QAM/FOIC8e.8-DO" },
	{ 0x67, "FLEXO-8-DO-16QAM/FOIC8.8-DO" },
	{ 0x68, "FLEXO-8e-DPO-16QAM/FOIC8e.8-DPO" },
	{ 0x69, "FLEXO-8-DPO-16QAM/FOIC8.8-DPO" },
	{ 0x6A, "FLEXO-6e-DPO-16QAM/FOIC6e.8-DPO" },
	{ 0x6B, "FLEXO-6-DPO-16QAM/FOIC6.8-DPO" },
	{ 0x6C, "800ZR-A DWDM" },
	{ 0x6D, "800ZR-B DWDM" },
	{ 0x6E, "800ZR-C DWDM" },
	{ 0x6F, "400G-ER4-30" },
	{ 0x70, "1I1-5D1F" },
	{ 0x71, "1R1-5D1F" },
	{ 0x72, "FOIC1.1-RS" },
	{ 0x73, "200GBASE-DR1" },
	{ 0x74, "200GBASE-DR1-2" },
	{ 0x75, "400GBASE-DR2" },
	{ 0x76, "400GBASE-DR2-2" },
	{ 0x77, "800GBASE-DR4" },
	{ 0x78, "800GBASE-DR4-2" },
	{ 0x79, "800GBASE-FR4-500" },
	{ 0x7A, "800GBASE-FR4" },
	{ 0x7B, "800GBASE-LR4" },
	{ 0x7C, "800GBASE-LR1" },
	{ 0x7D, "800GBASE-ER1-20" },
	{ 0x7E, "800GBASE-ER1" },
	{ 0x7F, "1.6TBASE-DR8" },
	{ 0x80, "1.6TBASE-DR8-2" },
	/* Open XR Optics */
	{ 0x81, "XR400-16QAM" },
	{ 0x82, "XR300-8QAM" },
	{ 0x83, "XR200-QPSK" },
	{ 0x84, "XR200-16QAM" },
	{ 0x85, "XR100-QPSK" },
	{ 0x86, "XR100-16QAM" },
	/* Open XR Optics Wide-Spacing */
	{ 0x87, "XR400-WS-16QAM" },
	{ 0x88, "XR200-WS-QPSK" },
	{ 0x89, "XR200-WS-16QAM" },
	{ 0x8A, "XR100-WS-QPSK" },
	{ 0x8B, "XR100-WS-16QAM" },
	{ 0x8C, "XR200-WS-BIDI-16QAM" },
	{ 0x8D, "XR100-WS-BIDI-QPSK" },
	{ 0x8E, "XR100-WS-BIDI-16QAM" },
	/* LPO */
	{ 0x8F, "100G-DR1-LPO" },
	{ 0x90, "200G-DR2-LPO" },
	{ 0x91, "400G-DR4-LPO" },
	{ 0x92, "800G-DR8-LPO" },
	/* OpenZR+ (cont.) */
	{ 0x35, "ZR400-OFEC-16QAM-HA" },
	{ 0x36, "ZR400-OFEC-16QAM-HB" },
	{ 0x37, "ZR400-OFEC-8QAM-HA" },
};

/* SFF-8024 Rev 4.13 Table 4-8: Passive/Linear Active Copper Cable */
static const struct sff8024_intf_id sff8024_passive_cu_ids[] = {
	{ 0x01, "Copper cable" },
	{ 0xBF, "Passive Loopback module" },
};

/* SFF-8024 Rev 4.13 Table 4-9: Active Cable assembly */
static const struct sff8024_intf_id sff8024_active_cable_ids[] = {
	{ 0x01, "BER < 1e-12" },
	{ 0x02, "BER < 5e-5" },
	{ 0x03, "BER < 2.6e-4" },
	{ 0x04, "BER < 1e-6" },
	{ 0xBF, "Active Loopback module" },
};

/* SFF-8024 Rev 4.13 Table 4-10: BASE-T media interface codes */
static const struct sff8024_intf_id sff8024_base_t_ids[] = {
	{ 0x01, "1000BASE-T" },
	{ 0x02, "2.5GBASE-T" },
	{ 0x03, "5GBASE-T" },
	{ 0x04, "10GBASE-T" },
	{ 0x05, "25GBASE-T" },
	{ 0x06, "40GBASE-T" },
	{ 0x07, "50GBASE-T" },
};

const char *sff8024_host_id_name(__u8 id)
{
	return sff8024_id_lookup(sff8024_host_ids,
				SFF8024_TBL_SZ(sff8024_host_ids), id);
}

const char *sff8024_media_id_name(__u8 module_type, __u8 id)
{
	switch (module_type) {
	case SFF8024_MT_MMF:
		return sff8024_id_lookup(sff8024_mmf_media_ids,
					SFF8024_TBL_SZ(sff8024_mmf_media_ids),
					id);
	case SFF8024_MT_SMF:
		return sff8024_id_lookup(sff8024_smf_media_ids,
					SFF8024_TBL_SZ(sff8024_smf_media_ids),
					id);
	case SFF8024_MT_PASSIVE_COPPER:
		return sff8024_id_lookup(sff8024_passive_cu_ids,
					SFF8024_TBL_SZ(sff8024_passive_cu_ids),
					id);
	case SFF8024_MT_ACTIVE_CABLE:
		return sff8024_id_lookup(sff8024_active_cable_ids,
					SFF8024_TBL_SZ(sff8024_active_cable_ids),
					id);
	case SFF8024_MT_BASE_T:
		return sff8024_id_lookup(sff8024_base_t_ids,
					SFF8024_TBL_SZ(sff8024_base_t_ids),
					id);
	default:
		return NULL;
	}
}
