/*
 * cmis-cdb.h: CDB (Command Data Block) messaging and PM retrieval
 *
 * CDB command execution (OIF-CMIS-05.3, Section 9) and Performance
 * Monitoring via CDB commands 0x0200-0x0217 (Section 9.8).
 *
 * Copyright (c) 2026 Vincent Jardin, Free Mobile
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef CMIS_CDB_H__
#define CMIS_CDB_H__

#include "cmis-internal.h"

/*
 * Read the CDB status byte from Page 00h:0x25.
 * Returns the raw status byte, or -1 on I2C error.
 */
int cmis_cdb_read_status(struct cmd_context *ctx);

/*
 * Send a CDB command with an optional LPL payload.
 *
 * cmd_id:   16-bit CDB command ID
 * lpl_data: pointer to LPL payload bytes (NULL if none)
 * lpl_len:  number of LPL payload bytes (0-120)
 * reply:    output buffer for LPL reply data (120 bytes max), or NULL
 * rpl_len:  output — number of reply bytes, or NULL
 *
 * Returns the CDB status byte on success, or -1 on error.
 */
int cmis_cdb_send_command_lpl(struct cmd_context *ctx, __u16 cmd_id,
			      const __u8 *lpl_data, __u8 lpl_len,
			      __u8 *reply, __u8 *rpl_len);

/*
 * Send a CDB command with no LPL payload (convenience wrapper).
 */
int cmis_cdb_send_command(struct cmd_context *ctx, __u16 cmd_id,
			  __u8 *reply, __u8 *rpl_len);

/*
 * Return a human-readable string for a CDB status byte.
 */
const char *cmis_cdb_status_str(int status);

/*
 * Look up a standard CDB command name by ID.
 * Returns "Unknown" or "Vendor-specific" for unrecognized IDs.
 */
const char *cmis_cdb_cmd_name(__u16 cmd_id);

/*
 * Display CDB Performance Monitoring (CMD 0x0200-0x0217).
 * Queries PM features, then retrieves module/host/media/data-path PM.
 * Returns 0 on success, -1 on error.
 */
int cmis_cdb_show_pm(struct cmd_context *ctx);

/*
 * Display CDB Module Features (CMD 0x0040).
 * Shows supported commands bitmap and max completion time.
 */
void cmis_cdb_show_module_features(struct cmd_context *ctx);

/*
 * Display CDB Firmware Management Features (CMD 0x0041).
 * Shows FW update capabilities, image sizes, block sizes.
 */
void cmis_cdb_show_fw_mgmt_features(struct cmd_context *ctx);

/*
 * Display CDB Get Firmware Info (CMD 0x0100).
 * Shows image A/B/factory versions and status.
 */
void cmis_cdb_show_fw_info(struct cmd_context *ctx);

/*
 * Display a generic CDB feature query (CMD 0x0042-0x0045).
 * Sends cmd_id and prints the raw reply hex dump.
 */
void cmis_cdb_show_generic_features(struct cmd_context *ctx, __u16 cmd_id,
				    const char *name);

/*
 * Display all CDB feature discovery information.
 * Calls Module Features, FW Mgmt Features, FW Info, and
 * generic feature queries (PM, BERT, Security, Ext).
 * Returns 0 on success, -1 on error.
 */
int cmis_cdb_show_features(struct cmd_context *ctx);

/*
 * Display BERT and Diagnostics Features (CMD 0x0043).
 * Decodes the 0x0300-0x03FF support bitmap.
 */
void cmis_cdb_show_bert_features(struct cmd_context *ctx);

/*
 * Display Security Features (CMD 0x0044).
 * Decodes 0x0400-0x04FF support bitmap, certificate info.
 */
void cmis_cdb_show_security_features(struct cmd_context *ctx);

/*----------------------------------------------------------------------
 * Firmware Update (CMD 0x0101-0x010A)
 *----------------------------------------------------------------------*/

/*
 * Download firmware from a file to the module.
 * Queries FW management features, sends start/write/complete sequence.
 * Returns 0 on success, -1 on error.
 */
int cmis_cdb_fw_download(struct cmd_context *ctx, const char *filepath);

/*
 * Run firmware image (CMD 0x0109).
 * mode: 0=reset-to-inactive, 1=hitless-to-inactive,
 *       2=reset-to-running, 3=hitless-to-running
 */
int cmis_cdb_fw_run(struct cmd_context *ctx, __u8 mode);

/*
 * Commit the running firmware image (CMD 0x010A).
 */
int cmis_cdb_fw_commit(struct cmd_context *ctx);

/*
 * Copy firmware image (CMD 0x0108).
 * direction: 0xAB = A->B, 0xBA = B->A
 */
int cmis_cdb_fw_copy(struct cmd_context *ctx, __u8 direction);

/*
 * Abort an in-progress firmware download (CMD 0x0102).
 */
int cmis_cdb_fw_abort(struct cmd_context *ctx);

/*----------------------------------------------------------------------
 * Security / IDevID (CMD 0x0400-0x0405)
 *----------------------------------------------------------------------*/

/*
 * Retrieve an IDevID certificate via CDB CMD 0x0400.
 * cert_index: 0=leaf, 1-3=chain
 * cert_buf/buf_size: output buffer
 * cert_len: actual certificate length returned
 * Returns 0 on success, -1 on error.
 */
int cmis_cdb_get_certificate(struct cmd_context *ctx, __u8 cert_index,
			     __u8 *cert_buf, int buf_size, int *cert_len);

/*
 * Display all IDevID certificates (hex dump).
 */
void cmis_cdb_show_certificates(struct cmd_context *ctx);

/*
 * Set a digest for the module to sign (CMD 0x0402).
 */
int cmis_cdb_set_digest(struct cmd_context *ctx, const __u8 *digest,
			__u8 len);

/*
 * Get the signature for the last set digest (CMD 0x0404).
 */
int cmis_cdb_get_signature(struct cmd_context *ctx, __u8 *sig_buf,
			   int buf_size, int *sig_len);

#endif /* CMIS_CDB_H__ */
