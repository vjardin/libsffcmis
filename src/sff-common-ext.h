/*
 * sff-common-ext.h: SFF-8024 Rev 4.13 interface ID lookup tables
 * and extended utility macros.
 *
 * Copyright (C) 2025 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SFF_COMMON_EXT_H__
#define SFF_COMMON_EXT_H__

#include "sff-common.h"

/* Signed 16-bit */
#define OFFSET_TO_S16_PTR(ptr, offset) \
	((__s16)OFFSET_TO_U16_PTR(ptr, offset))

/* Unsigned 32-bit (big-endian) */
#define OFFSET_TO_U32_PTR(ptr, offset) \
	(((__u32)(ptr)[offset] << 24) | ((__u32)(ptr)[(offset)+1] << 16) | \
	 ((__u32)(ptr)[(offset)+2] << 8) | (__u32)(ptr)[(offset)+3])

/* Signed 32-bit (big-endian) */
#define OFFSET_TO_S32_PTR(ptr, offset) \
	((__s32)OFFSET_TO_U32_PTR(ptr, offset))

/* Unsigned 64-bit (big-endian) */
#define OFFSET_TO_U64_PTR(ptr, offset) \
	(((__u64)(ptr)[offset] << 56) | ((__u64)(ptr)[(offset)+1] << 48) | \
	 ((__u64)(ptr)[(offset)+2] << 40) | ((__u64)(ptr)[(offset)+3] << 32) | \
	 ((__u64)(ptr)[(offset)+4] << 24) | ((__u64)(ptr)[(offset)+5] << 16) | \
	 ((__u64)(ptr)[(offset)+6] << 8)  | (__u64)(ptr)[(offset)+7])

/* SFF-8024 interface ID lookup tables (Tables 4-4 through 4-10) */
struct sff8024_intf_id {
	__u8 id;
	const char *name;
};

#define SFF8024_TBL_SZ(a) (sizeof(a) / sizeof((a)[0]))

const char *sff8024_host_id_name(__u8 id);
const char *sff8024_media_id_name(__u8 module_type, __u8 id);
const char *sff8024_ext_spec_compliance_name(__u8 id);

#endif /* SFF_COMMON_EXT_H__ */
