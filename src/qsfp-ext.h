/*
 * qsfp-ext.h: SFF-8636 extension defines and function declarations.
 *
 * Copyright (C) 2025 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QSFP_EXT_H__
#define QSFP_EXT_H__

#include "qsfp.h"

/* SAS/SATA 24G — byte 133, bit 7 */
#define SFF8636_SAS_24G				(1 << 7)

/* FC Speed — byte 140, bits 3 and 1 */
#define SFF8636_FC_SPEED_3200_MBPS		(1 << 3)
#define SFF8636_FC_SPEED_EXTENDED		(1 << 1)

/* Secondary Extended Spec Compliance — byte 116 */
#define SFF8636_SEC_EXT_COMP_OFFSET		0x74

/* Extended Module Codes — byte 164, bit 5 */
#define SFF8636_EXT_MOD_INFINIBAND_HDR		(1 << 5)

/* Extended Baud Rate Nominal — byte 222 */
#define SFF8636_EXT_BAUD_RATE_OFFSET		0xDE

struct cmd_context;

int sff8636_show_all_ext_nl(struct cmd_context *ctx);

#endif /* QSFP_EXT_H__ */
