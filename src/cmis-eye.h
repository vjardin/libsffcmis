/*
 * cmis-eye.h: Synthetic PAM4 eye diagram / IQ constellation from VDM data
 *
 * PAM4 eye: rendered from amplitude levels (Type IDs 16-19) and SNR (29-34).
 * IQ constellation: rendered from coherent EVM/eSNR (C-CMIS Type IDs 140-142)
 * with modulation auto-detected from the active application descriptor.
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_EYE_H__
#define CMIS_EYE_H__

#include "cmis-internal.h"

/* Render synthetic PAM4 eye diagram(s) and/or IQ constellation from VDM data */
void cmis_show_eye(const struct cmis_memory_map *map);

#endif /* CMIS_EYE_H__ */
