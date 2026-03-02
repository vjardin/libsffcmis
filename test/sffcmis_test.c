#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <unistd.h>
#include <linux/types.h>

#include "internal.h"
#include "sff-common.h"
#include "sffcmis.h"
#include "cmis-internal.h"
#include "cmis-tunable.h"
#include "cmis-coherent.h"
#include "cmis-datapath.h"
#include "cmis-netpath.h"
#include "cmis-diag.h"
#include "json_print.h"
#include "cmis-vdm.h"
#include "cmis-eye.h"
#include "cmis-cdb.h"

static void
usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS] <i2c_bus_number>\n"
		"\n"
		"Options:\n"
		"  -h, --help                 Show this help message\n"
		"  -j, --json                 Output in JSON format\n"
		"  -d, --debug <level>        Set debug level\n"
		"  -I, --include-statistics   Include statistics in output\n"
		"  -m, --map                  Display tunable wavelength map\n"
		"\n"
		"Module power state:\n"
		"  -w, --wake                 Wake module from ModuleLowPwr to ModuleReady\n"
		"  -s, --sleep                Put module back into ModuleLowPwr\n"
		"\n"
		"CDB (Command Data Block) messaging:\n"
		"  -C, --cdb                  Query CDB supported commands\n"
		"  -P, --pm                   Show CDB Performance Monitoring (CMD 0x0200-0x0217)\n"
		"\n"
		"Firmware update:\n"
		"      --fw-update <file>     Download firmware from file\n"
		"      --fw-run [mode]        Run firmware image (0=reset-inactive, 1=hitless-inactive,\n"
		"                               2=reset-running, 3=hitless-running)\n"
		"      --fw-commit            Commit running firmware image\n"
		"      --fw-copy <AB|BA>      Copy firmware image\n"
		"      --fw-abort             Abort in-progress firmware download\n"
		"\n"
		"Security/IDevID:\n"
		"      --cert                 Display IDevID certificates\n"
		"      --cert-file <path>     Save leaf certificate to file (DER)\n"
		"      --auth <digest-hex>    Challenge-response: set digest, get signature\n"
		"\n"
		"Coherent optics monitoring:\n"
		"  -K, --coherent             Show C-CMIS coherent PM (Page 35h) and FEC (Page 34h)\n"
		"  -D, --datapath             Show data path control/status (Pages 10h/11h)\n"
		"  -N, --netpath              Show network path control/status (Pages 16h/17h)\n"
		"      --diag                 Show diagnostics capabilities (Pages 13h-14h)\n"
		"      --bert-start <pattern> Start PRBS/BER test (e.g., PRBS-31Q, PRBS-13Q)\n"
		"      --bert-stop            Stop PRBS/BER test\n"
		"      --bert-read            Read PRBS/BER test results (BER, error/bit counts)\n"
		"      --loopback-start <mode> Start loopback (host-input, host-output, media-input, media-output)\n"
		"      --loopback-stop        Stop all loopback modes\n"
		"  -V, --vdm                  Show VDM real-time monitors (Pages 20h-2Fh)\n"
		"  -E, --eye                  Show eye diagram / IQ constellation (from VDM)\n"
		"\n"
		"Data path write operations:\n"
		"      --tx-disable           Disable Tx output for --lane (or all lanes)\n"
		"      --tx-enable            Enable Tx output for --lane (or all lanes)\n"
		"      --dp-deactivate        Deactivate data path for --lane (or all lanes)\n"
		"      --dp-activate          Activate data path for --lane (or all lanes)\n"
		"      --dp-config            Configure data path lane (requires --lane, --appsel)\n"
		"      --appsel <1-15>        Application selection code for --dp-config\n"
		"      --dpid <0-7>           Data path ID for --dp-config (default: 0)\n"
		"      --explicit             Enable explicit control for --dp-config\n"
		"      --cdr <on|off>         Set CDR enable for --lane (or all lanes)\n"
		"\n"
		"Coherent threshold configuration:\n"
		"      --set-threshold <type> <value>  Set threshold (see types below)\n"
		"      --threshold-enable <type>       Enable configured threshold\n"
		"      --threshold-disable <type>      Disable configured threshold\n"
		"    Threshold types: total-pwr-hi-alarm, total-pwr-lo-alarm,\n"
		"      total-pwr-hi-warn, total-pwr-lo-warn, sig-pwr-hi-alarm,\n"
		"      sig-pwr-lo-alarm, sig-pwr-hi-warn, sig-pwr-lo-warn,\n"
		"      fdd-raise, fdd-clear, fed-raise, fed-clear\n"
		"    Enable/disable types: total-pwr, sig-pwr, fdd, fed\n"
		"\n"
		"Signal integrity controls (Page 10h):\n"
		"      --polarity-tx <flip|normal>  Set Tx input polarity\n"
		"      --polarity-rx <flip|normal>  Set Rx output polarity\n"
		"      --rx-disable           Disable Rx output for --lane\n"
		"      --rx-enable            Enable Rx output for --lane\n"
		"      --auto-squelch-tx <on|off>   Set Tx auto-squelch disable\n"
		"      --auto-squelch-rx <on|off>   Set Rx auto-squelch disable\n"
		"      --squelch-force-tx <on|off>  Force Tx output squelch\n"
		"      --eq-freeze-tx <on|off>      Adaptive Tx input EQ freeze\n"
		"      --eq-enable-tx <on|off>      Adaptive Tx input EQ enable (staged)\n"
		"      --eq-target-tx <0-15>        Fixed Tx input EQ target (staged, requires --lane)\n"
		"      --rx-eq-pre <0-15>    Rx output EQ pre-cursor target (staged, requires --lane)\n"
		"      --rx-eq-post <0-15>   Rx output EQ post-cursor target (staged, requires --lane)\n"
		"      --rx-amplitude <0-15> Rx output amplitude target (staged, requires --lane)\n"
		"      --lane-mask <offset> <mask> <on|off>  Set lane interrupt mask\n"
		"\n"
		"Module global controls:\n"
		"      --sw-reset             Trigger module software reset\n"
		"      --bank-broadcast <on|off>    Set bank broadcast enable\n"
		"      --squelch-method <oma|pav>   Set squelch method\n"
		"      --module-mask <offset> <mask> <on|off>  Set module-level mask\n"
		"      --password-entry <hex>        Enter module password (hex, e.g. 00000000)\n"
		"      --password-change <cur> <new> Change module password (hex)\n"
		"\n"
		"VDM controls:\n"
		"      --vdm-freeze          Freeze VDM samples\n"
		"      --vdm-unfreeze        Unfreeze VDM samples\n"
		"      --vdm-power-saving <on|off>  Set VDM power saving mode\n"
		"      --vdm-mask <instance> <nibble>  Set VDM alarm mask\n"
		"\n"
		"C-CMIS coherent write operations:\n"
		"      --tx-filter <on|off>  Set Tx filter enable for --lane\n"
		"      --tx-filter-type <0-3> Set Tx filter type (requires --lane)\n"
		"      --lf-insertion <on|off> Set LF insertion on LD enable\n"
		"      --media-mask <offset> <mask> <on|off>   Set media flag mask (Page 32h)\n"
		"      --host-threshold <type> <value>          Set host BER threshold (Page 38h)\n"
		"      --host-threshold-enable <type>            Enable host threshold\n"
		"      --host-threshold-disable <type>           Disable host threshold\n"
		"      --host-mask <offset> <mask> <on|off>     Set host flag mask (Page 3Bh)\n"
		"    Host threshold types: host-fdd-raise, host-fdd-clear, host-fed-raise, host-fed-clear\n"
		"    Host enable/disable types: host-fdd, host-fed\n"
		"\n"
		"Network path controls:\n"
		"      --np-deactivate       Deactivate network path for --lane\n"
		"      --np-activate         Activate network path for --lane\n"
		"      --np-config           Configure NP lane (requires --lane, --npid)\n"
		"      --npid <0-15>         Network path ID for --np-config\n"
		"      --np-in-use           Set NP in-use for --np-config\n"
		"      --hp-source-rx <np|replace>  Set HP signal source Rx\n"
		"      --np-source-tx <hp|replace>  Set NP signal source Tx\n"
		"      --np-state-mask <on|off>     Set NP state-changed mask\n"
		"\n"
		"Diagnostics write operations:\n"
		"      --diag-mask <offset> <on|off>  Set diagnostics mask\n"
		"      --scratchpad-read     Read host scratchpad (8 bytes)\n"
		"      --scratchpad-write <hex>  Write host scratchpad\n"
		"      --user-pattern <hex>  Write PRBS user-defined pattern\n"
		"\n"
		"User EEPROM (Page 03h):\n"
		"      --eeprom-read <offset> <len>  Read user EEPROM\n"
		"      --eeprom-write <offset> <hex>  Write user EEPROM\n"
		"\n"
		"Host lane switching:\n"
		"      --lane-redir <target>  Set lane redirection target (requires --lane)\n"
		"      --lane-switch-enable   Enable lane switching\n"
		"      --lane-switch-disable  Disable lane switching\n"
		"      --lane-switch-commit   Commit lane switching config\n"
		"      --lane-switch-result   Read lane switching result\n"
		"\n"
		"Tunable laser options:\n"
		"      --tuning-mask <bits> <on|off>  Set per-lane tuning flag mask (requires --lane)\n"
		"\n"
		"Tunable laser options (require --tune):\n"
		"  -t, --tune                 Configure tunable laser wavelength\n"
		"  -g, --grid <ghz>           Grid spacing in GHz (3.125, 6.25, 12.5, 25, 33, 50, 75, 100, 150)\n"
		"  -c, --channel <n>          Channel number (signed integer)\n"
		"  -l, --lane <n>             Lane number 0-7 (default: all lanes in bank)\n"
		"  -b, --bank <n>             Bank number 0-3 (default: 0)\n"
		"  -f, --fine-offset <mghz>   Fine-tuning offset in units of 0.001 GHz (default: 0)\n"
		"  -p, --power <cdbm>         Target output power in units of 0.01 dBm (default: 0)\n",
		progname);
}

/*
 * Convert a grid spacing string (GHz) to the CMIS_GRID_ENC_* encoding value.
 * Returns -1 on invalid input.
 */
static int
parse_grid_ghz(const char *str)
{
	char *endp;
	double ghz = strtod(str, &endp);

	if (*endp != '\0' || endp == str)
		return -1;

	if (fabs(ghz - 3.125) < 0.001)  return CMIS_GRID_ENC_3P125GHZ;
	if (fabs(ghz - 6.25)  < 0.001)  return CMIS_GRID_ENC_6P25GHZ;
	if (fabs(ghz - 12.5)  < 0.001)  return CMIS_GRID_ENC_12P5GHZ;
	if (fabs(ghz - 25.0)  < 0.001)  return CMIS_GRID_ENC_25GHZ;
	if (fabs(ghz - 33.0)  < 0.001)  return CMIS_GRID_ENC_33GHZ;
	if (fabs(ghz - 50.0)  < 0.001)  return CMIS_GRID_ENC_50GHZ;
	if (fabs(ghz - 75.0)  < 0.001)  return CMIS_GRID_ENC_75GHZ;
	if (fabs(ghz - 100.0) < 0.001)  return CMIS_GRID_ENC_100GHZ;
	if (fabs(ghz - 150.0) < 0.001)  return CMIS_GRID_ENC_150GHZ;

	return -1;
}

/*
 * Read a CMIS upper memory page and return a pointer adjusted so that
 * ptr[0x80..0xFF] maps to the page data (matching spec offsets).
 */
static const __u8 *
read_cmis_page(struct cmd_context *ctx, __u8 bank, __u8 page)
{
	struct module_eeprom req;

	cmis_request_init(&req, bank, page, CMIS_PAGE_SIZE);
	if (get_eeprom_page(ctx, &req) < 0)
		return NULL;
	return req.data - CMIS_PAGE_SIZE;
}

/*
 * Parse a hex string into a byte buffer. Returns number of bytes parsed,
 * or -1 on error. Stops at non-hex chars.
 */
static int
parse_hex_string(const char *hex, __u8 *buf, int max_len)
{
	int len = 0;
	unsigned int byte;

	while (*hex && len < max_len) {
		if (sscanf(hex, "%2x", &byte) != 1)
			break;
		buf[len++] = byte;
		hex += 2;
	}
	return len;
}

/*
 * Parse "on"/"off" string. Returns 1 for on, 0 for off, -1 for invalid.
 */
static int
parse_on_off(const char *str)
{
	if (strcmp(str, "on") == 0)
		return 1;
	if (strcmp(str, "off") == 0)
		return 0;
	return -1;
}

/* Convert frequency in THz to wavelength in nm: λ = c / f */
static double
freq_to_wavelength(double freq_thz)
{
	if (freq_thz <= 0.0)
		return 0.0;
	return 299792.458 / freq_thz;
}

/*
 * Convert a frequency offset in GHz to a wavelength offset in nm
 * at a given center wavelength.  |Δλ| = λ² x |Δf| / c
 */
static double
ghz_offset_to_nm(double ghz, double center_nm)
{
	return center_nm * center_nm * ghz / 299792458.0;
}

/* Local grid info table — mirrors cmis-tunable.c:grids[] */
struct map_grid_info {
	const char *name;
	__u8 msb_mask;
	__u8 lsb_mask;
	__u8 low_ch_offset;
	__u8 high_ch_offset;
	__u32 spacing_mhz;
	__u8 encoding;
};

static const struct map_grid_info map_grids[] = {
	{ "75 GHz",    CMIS_GRID_75GHZ_SUPPORTED,   0,
	  CMIS_GRID_75GHZ_LOW_CH,   CMIS_GRID_75GHZ_HIGH_CH,
	  75000,  CMIS_GRID_ENC_75GHZ },
	{ "150 GHz",   CMIS_GRID_150GHZ_SUPPORTED,  0,
	  CMIS_GRID_150GHZ_LOW_CH,  CMIS_GRID_150GHZ_HIGH_CH,
	  150000, CMIS_GRID_ENC_150GHZ },
	{ "33 GHz",    CMIS_GRID_33GHZ_SUPPORTED,   0,
	  CMIS_GRID_33GHZ_LOW_CH,   CMIS_GRID_33GHZ_HIGH_CH,
	  33000,  CMIS_GRID_ENC_33GHZ },
	{ "100 GHz",   CMIS_GRID_100GHZ_SUPPORTED,  0,
	  CMIS_GRID_100GHZ_LOW_CH,  CMIS_GRID_100GHZ_HIGH_CH,
	  100000, CMIS_GRID_ENC_100GHZ },
	{ "50 GHz",    CMIS_GRID_50GHZ_SUPPORTED,   0,
	  CMIS_GRID_50GHZ_LOW_CH,   CMIS_GRID_50GHZ_HIGH_CH,
	  50000,  CMIS_GRID_ENC_50GHZ },
	{ "25 GHz",    CMIS_GRID_25GHZ_SUPPORTED,   0,
	  CMIS_GRID_25GHZ_LOW_CH,   CMIS_GRID_25GHZ_HIGH_CH,
	  25000,  CMIS_GRID_ENC_25GHZ },
	{ "12.5 GHz",  CMIS_GRID_12P5GHZ_SUPPORTED, 0,
	  CMIS_GRID_12P5GHZ_LOW_CH, CMIS_GRID_12P5GHZ_HIGH_CH,
	  12500,  CMIS_GRID_ENC_12P5GHZ },
	{ "6.25 GHz",  CMIS_GRID_6P25GHZ_SUPPORTED, 0,
	  CMIS_GRID_6P25GHZ_LOW_CH, CMIS_GRID_6P25GHZ_HIGH_CH,
	  6250,   CMIS_GRID_ENC_6P25GHZ },
	{ "3.125 GHz", 0, CMIS_GRID_3P125GHZ_SUPPORTED,
	  CMIS_GRID_3P125GHZ_LOW_CH, CMIS_GRID_3P125GHZ_HIGH_CH,
	  3125,   CMIS_GRID_ENC_3P125GHZ },
};

#define NUM_MAP_GRIDS ARRAY_SIZE(map_grids)

static const char *
grid_enc_to_name(__u8 enc)
{
	unsigned int i;

	for (i = 0; i < NUM_MAP_GRIDS; i++)
		if (map_grids[i].encoding == enc)
			return map_grids[i].name;
	return "Unknown";
}

static const char *
cmis_mod_state_str(__u8 state)
{
	switch (state) {
	case CMIS_MODULE_STATE_MODULE_LOW_PWR:	return "ModuleLowPwr";
	case CMIS_MODULE_STATE_MODULE_PWR_UP:	return "ModulePwrUp";
	case CMIS_MODULE_STATE_MODULE_READY:	return "ModuleReady";
	case CMIS_MODULE_STATE_MODULE_PWR_DN:	return "ModulePwrDn";
	case CMIS_MODULE_STATE_MODULE_FAULT:	return "ModuleFault";
	default:				return "Unknown";
	}
}

/*
 * Read just the control area (bytes 0x00-0x1A) from lower memory.
 * This covers the module state (0x03) and control register (0x1A)
 * in a single 32-byte I2C transfer, with retries on timeout.
 */
static int
cmis_read_control_area(struct cmd_context *ctx, struct module_eeprom *request)
{
	int ret, retries;

	for (retries = 0; retries < 3; retries++) {
		cmis_request_init(request, 0, 0x0, 0);
		request->length = CMIS_MODULE_CONTROL_OFFSET + 1;
		ret = get_eeprom_page(ctx, request);
		if (ret == 0)
			return 0;
		if (retries < 2)
			usleep(100000);
	}
	return ret;
}

/* Read just the module state byte (0x03), with retries. */
static int
cmis_read_module_state(struct cmd_context *ctx, __u8 *mod_state)
{
	struct module_eeprom request;
	int ret, retries;

	for (retries = 0; retries < 3; retries++) {
		cmis_request_init(&request, 0, 0x0, 0);
		request.length = CMIS_MODULE_STATE_OFFSET + 1;
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0) {
			*mod_state = (request.data[CMIS_MODULE_STATE_OFFSET] &
				      CMIS_MODULE_STATE_MASK) >> 1;
			return 0;
		}
		if (retries < 2)
			usleep(100000);
	}
	return ret;
}

/*
 * Wake a CMIS module from ModuleLowPwr to ModuleReady.
 *
 * Clears LowPwrAllowRequestHW (bit 6) and LowPwrRequestSW (bit 4)
 * in the Module Control register (byte 0x1A), then polls the Module
 * State register (byte 0x03) until the module reaches ModuleReady.
 */
static int
cmis_wake(struct cmd_context *ctx)
{
	struct module_eeprom request;
	__u8 mod_state, control, buf;
	int ret, tries;

	/* Read control area (bytes 0x00-0x1A) with retry */
	ret = cmis_read_control_area(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read module state\n");
		return ret;
	}

	/* Check current module state */
	mod_state = (request.data[CMIS_MODULE_STATE_OFFSET] &
		     CMIS_MODULE_STATE_MASK) >> 1;
	printf("Module state: %s (0x%02x)\n",
	       cmis_mod_state_str(mod_state), mod_state);

	if (mod_state == CMIS_MODULE_STATE_MODULE_READY) {
		printf("Module is already in ModuleReady state\n");
		return 0;
	}

	if (mod_state == CMIS_MODULE_STATE_MODULE_FAULT) {
		fprintf(stderr, "Error: module is in fault state\n");
		return -1;
	}

	/* Read and display current control register */
	control = request.data[CMIS_MODULE_CONTROL_OFFSET];
	printf("Control register 0x%02x: LowPwrAllowRequestHW=%s,"
	       " LowPwrRequestSW=%s\n", control,
	       (control & CMIS_LOW_PWR_ALLOW_REQUEST_HW_MASK) ? "On" : "Off",
	       (control & CMIS_LOW_PWR_REQUEST_SW_MASK) ? "On" : "Off");

	/* Clear both low-power request bits */
	buf = control & ~(CMIS_LOW_PWR_ALLOW_REQUEST_HW_MASK |
			  CMIS_LOW_PWR_REQUEST_SW_MASK);

	cmis_request_init(&request, 0, 0x0, CMIS_MODULE_CONTROL_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot write control register\n");
		return ret;
	}
	printf("Cleared LowPwrAllowRequestHW and LowPwrRequestSW\n");

	/* Poll for ModuleReady (100ms intervals, 10s timeout) */
	printf("Waiting for ModuleReady");
	fflush(stdout);
	for (tries = 0; tries < 100; tries++) {
		usleep(100000);

		ret = cmis_read_module_state(ctx, &mod_state);
		if (ret < 0) {
			printf("x");
			fflush(stdout);
			continue;  /* retry on transient I2C errors */
		}

		printf(".");
		fflush(stdout);

		if (mod_state == CMIS_MODULE_STATE_MODULE_READY) {
			printf(" OK\n");
			printf("Module is now in ModuleReady state\n");
			return 0;
		}
		if (mod_state == CMIS_MODULE_STATE_MODULE_FAULT) {
			printf("\n");
			fprintf(stderr,
				"Error: module entered fault state\n");
			return -1;
		}
	}

	printf("\n");
	fprintf(stderr, "Timeout: module did not reach ModuleReady"
		" within 10 seconds (state: %s)\n",
		cmis_mod_state_str(mod_state));
	return -1;
}

/*
 * Put a CMIS module back into ModuleLowPwr by setting
 * LowPwrRequestSW (bit 4) in the Module Control register.
 */
static int
cmis_sleep(struct cmd_context *ctx)
{
	struct module_eeprom request;
	__u8 mod_state, buf;
	int ret;

	/* Read control area (bytes 0x00-0x1A) with retry */
	ret = cmis_read_control_area(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read module state\n");
		return ret;
	}

	mod_state = (request.data[CMIS_MODULE_STATE_OFFSET] &
		     CMIS_MODULE_STATE_MASK) >> 1;
	printf("Module state: %s (0x%02x)\n",
	       cmis_mod_state_str(mod_state), mod_state);

	if (mod_state == CMIS_MODULE_STATE_MODULE_LOW_PWR) {
		printf("Module is already in ModuleLowPwr state\n");
		return 0;
	}

	/* Set LowPwrRequestSW (bit 4) */
	buf = request.data[CMIS_MODULE_CONTROL_OFFSET] |
	      CMIS_LOW_PWR_REQUEST_SW_MASK;

	cmis_request_init(&request, 0, 0x0, CMIS_MODULE_CONTROL_OFFSET);
	request.length = 1;
	request.data = &buf;
	ret = set_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot write control register\n");
		return ret;
	}

	printf("LowPwrRequestSW set — module entering ModuleLowPwr\n");
	return 0;
}

/*----------------------------------------------------------------------
 * CDB (Command Data Block) messaging support
 *
 * Implements the CMIS CDB protocol (OIF-CMIS-05.3, section 9) to send
 * commands to the module and read replies.  Used for feature discovery
 * (CMD 0040h-0045h) and probing vendor-specific command space (8000h+).
 *----------------------------------------------------------------------*/

/* CDB infrastructure (cdb_send_command, cdb_status_str, etc.) is now
 * in the library (cmis-cdb.c).  Feature discovery functions below use
 * the library's cmis_cdb_send_command() / cmis_cdb_status_str() /
 * cmis_cdb_cmd_name() APIs.
 */

/* CDB feature discovery functions are now in the library (cmis-cdb.c). */




/*
 * Probe vendor-specific CDB command range (8000h-FFFFh).
 *
 * There is no standard way to enumerate vendor-specific commands.
 * We probe by sending each command ID and checking whether the module
 * responds with "Unknown command" (fail code 0x01) or something else.
 * Any response other than "Unknown command" indicates the command is
 * recognized (though it may still fail for other reasons).
 *
 * Strategy: scan in 256-command blocks (0x8000-0x80FF, 0x8100-0x81FF,
 * etc.).  If a full block yields no hits, skip ahead.  Stop after 4
 * consecutive empty blocks to avoid scanning the entire 32K space.
 */
static void
cdb_probe_vendor_commands(struct cmd_context *ctx)
{
	int status;
	int count = 0;
	int empty_blocks = 0;
	__u16 block, cmd;

	printf("\nProbing vendor-specific CDB commands (0x8000-0xFFFF)...\n");
	printf("  (Commands returning anything other than 'Unknown command'"
	       " are listed)\n");
	printf("  (Stops after 4 consecutive empty 256-command blocks)\n\n");

	for (block = 0x80; block <= 0xFF; block++) {
		int block_hits = 0;

		printf("  Scanning 0x%02X00-0x%02XFF...", block, block);
		fflush(stdout);

		for (cmd = (block << 8); cmd <= (block << 8 | 0xFF); cmd++) {
			status = cmis_cdb_send_command(ctx, cmd, NULL, NULL);
			if (status < 0) {
				fprintf(stderr,
					"\n  I2C error at CMD 0x%04X,"
					" stopping probe\n", cmd);
				goto done;
			}

			/* "Unknown command" = not supported */
			if ((status & CMIS_CDB_STATUS_FAIL) &&
			    (status & CMIS_CDB_STATUS_RESULT_MASK) ==
			    CMIS_CDB_FAIL_UNKNOWN_CMD)
				continue;

			printf("\n    CMD 0x%04X  -> %s (status 0x%02x)",
			       cmd, cmis_cdb_status_str(status), status);
			block_hits++;
			count++;
		}

		if (block_hits > 0) {
			printf("\n  Block 0x%02X00: %d commands found\n",
			       block, block_hits);
			empty_blocks = 0;
		} else {
			printf(" (none)\n");
			empty_blocks++;
			if (empty_blocks >= 4) {
				printf("  4 consecutive empty blocks,"
				       " stopping early.\n");
				break;
			}
		}
	}

done:
	printf("  Probe complete: %d vendor-specific commands found\n",
	       count);
}

/* CDB PM functions are now in the library (cmis-cdb.c / cmis_cdb_show_pm()). */

/*
 * Show coherent PM and flags via the library API.
 * Loads the CMIS memory map and calls cmis_show_coherent_pm/flags().
 */
static int
show_coherent_pm(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver, banks_bits;
	int num_banks, i, ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];
	if (!(adver & CMIS_PAGES_ADVER_COHERENT)) {
		cmis_quirk_check_coherent(ctx, &map);
		adver = page_01[CMIS_PAGES_ADVER_OFFSET];
		if (!(adver & CMIS_PAGES_ADVER_COHERENT)) {
			printf("C-CMIS coherent pages (30h-4Fh) not advertised by this module.\n");
			return 0;
		}
	}

	/* Determine number of banks */
	banks_bits = adver & CMIS_BANKS_SUPPORTED_MASK;
	if (banks_bits == CMIS_BANK_0_3_SUPPORTED)
		num_banks = 4;
	else if (banks_bits == CMIS_BANK_0_1_SUPPORTED)
		num_banks = 2;
	else
		num_banks = 1;

	/* Derive media lane count */
	map.media_lane_count = 0;
	{
		int base = 0x56; /* CMIS_APP_DESC_START_OFFSET */
		__u8 media_lanes = map.lower_memory[base + 2] & 0x0F;
		if (media_lanes > 0)
			map.media_lane_count = media_lanes;
	}

	/* Load C-CMIS pages into memory map */
	cmis_request_init(&request, 0, 0x35, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x35] = request.data - CMIS_PAGE_SIZE;

	cmis_request_init(&request, 0, 0x34, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x34] = request.data - CMIS_PAGE_SIZE;

	/* Page 40h may already be stashed by the quirk probe */
	if (!map.upper_memory[0][0x40]) {
		cmis_request_init(&request, 0, 0x40, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][0x40] =
				request.data - CMIS_PAGE_SIZE;
	}

	cmis_request_init(&request, 0, 0x42, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x42] = request.data - CMIS_PAGE_SIZE;

	/* Page 44h: alarm advertisement (non-banked) */
	cmis_request_init(&request, 0, 0x44, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x44] = request.data - CMIS_PAGE_SIZE;

	/* Page 41h: Rx signal power advertisement (non-banked) */
	cmis_request_init(&request, 0, CCMIS_RX_PWR_ADVER_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][CCMIS_RX_PWR_ADVER_PAGE] =
			request.data - CMIS_PAGE_SIZE;

	/* Page 43h: provisioning advertisement (non-banked) */
	cmis_request_init(&request, 0, CCMIS_PROV_ADVER_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][CCMIS_PROV_ADVER_PAGE] =
			request.data - CMIS_PAGE_SIZE;

	/* Pages 33h, 3Bh, 3Ah, 30h, 31h: coherent flags/thresholds/prov */
	for (i = 0; i < num_banks; i++) {
		cmis_request_init(&request, i, 0x33, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x33] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, 0x3B, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x3B] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, CCMIS_HOST_FEC_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][CCMIS_HOST_FEC_PAGE] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, CCMIS_THRESH_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][CCMIS_THRESH_PAGE] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, CCMIS_PROV_PAGE,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][CCMIS_PROV_PAGE] =
				request.data - CMIS_PAGE_SIZE;
	}

	cmis_show_coherent_pm(&map);
	cmis_show_coherent_flags(&map);
	return 0;
}

/*
 * Show VDM real-time monitors via the library API.
 * Loads the CMIS memory map and calls cmis_show_vdm().
 */
static int
show_vdm(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver;
	int vdm_groups, g, ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];
	if (!(adver & CMIS_PAGES_ADVER_VDM)) {
		printf("VDM pages (20h-2Fh) not advertised by this module.\n");
		return 0;
	}

	/* Page 2Fh: advertisement/control */
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read VDM Page 2Fh\n");
		return ret;
	}
	map.upper_memory[0][CMIS_VDM_ADVER_PAGE] =
		request.data - CMIS_PAGE_SIZE;

	vdm_groups = (map.upper_memory[0][CMIS_VDM_ADVER_PAGE]
		      [CMIS_VDM_SUPPORT_OFFSET] &
		      CMIS_VDM_SUPPORT_MASK) + 1;
	if (vdm_groups > CMIS_VDM_MAX_GROUPS)
		vdm_groups = CMIS_VDM_MAX_GROUPS;

	/* Freeze VDM samples for consistent snapshot */
	ret = cmis_vdm_freeze(ctx);
	if (ret < 0)
		fprintf(stderr, "Warning: VDM freeze failed (%d), reading unfrozen\n", ret);

	for (g = 0; g < vdm_groups; g++) {
		/* Descriptor page */
		cmis_request_init(&request, 0,
				  CMIS_VDM_DESC_PAGE_BASE + g,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g] =
				request.data - CMIS_PAGE_SIZE;

		/* Sample page */
		cmis_request_init(&request, 0,
				  CMIS_VDM_SAMPLE_PAGE_BASE + g,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g] =
				request.data - CMIS_PAGE_SIZE;

		/* Threshold page */
		cmis_request_init(&request, 0,
				  CMIS_VDM_THRESH_PAGE_BASE + g,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][CMIS_VDM_THRESH_PAGE_BASE + g] =
				request.data - CMIS_PAGE_SIZE;
	}

	/* Unfreeze VDM samples */
	ret = cmis_vdm_unfreeze(ctx);
	if (ret < 0)
		fprintf(stderr, "Warning: VDM unfreeze failed (%d)\n", ret);

	/* Flag page 2Ch */
	cmis_request_init(&request, 0, CMIS_VDM_FLAGS_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][CMIS_VDM_FLAGS_PAGE] =
			request.data - CMIS_PAGE_SIZE;

	/* Mask page 2Dh */
	cmis_request_init(&request, 0, CMIS_VDM_MASKS_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][CMIS_VDM_MASKS_PAGE] =
			request.data - CMIS_PAGE_SIZE;

	cmis_show_vdm(&map);
	return 0;
}

/*
 * Show PAM4 eye diagram from VDM data.
 * Only loads descriptor + sample pages (no thresholds/flags/masks needed).
 */
static int
show_eye(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver;
	int vdm_groups, g, ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];
	if (!(adver & CMIS_PAGES_ADVER_VDM)) {
		printf("VDM pages (20h-2Fh) not advertised by this module.\n");
		return 0;
	}

	/* Page 2Fh: advertisement/control */
	cmis_request_init(&request, 0, CMIS_VDM_ADVER_PAGE,
			  CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0) {
		fprintf(stderr, "Error: cannot read VDM Page 2Fh\n");
		return ret;
	}
	map.upper_memory[0][CMIS_VDM_ADVER_PAGE] =
		request.data - CMIS_PAGE_SIZE;

	vdm_groups = (map.upper_memory[0][CMIS_VDM_ADVER_PAGE]
		      [CMIS_VDM_SUPPORT_OFFSET] &
		      CMIS_VDM_SUPPORT_MASK) + 1;
	if (vdm_groups > CMIS_VDM_MAX_GROUPS)
		vdm_groups = CMIS_VDM_MAX_GROUPS;

	/* Freeze VDM samples for consistent snapshot */
	ret = cmis_vdm_freeze(ctx);
	if (ret < 0)
		fprintf(stderr, "Warning: VDM freeze failed (%d), reading unfrozen\n", ret);

	for (g = 0; g < vdm_groups; g++) {
		/* Descriptor page */
		cmis_request_init(&request, 0,
				  CMIS_VDM_DESC_PAGE_BASE + g,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][CMIS_VDM_DESC_PAGE_BASE + g] =
				request.data - CMIS_PAGE_SIZE;

		/* Sample page */
		cmis_request_init(&request, 0,
				  CMIS_VDM_SAMPLE_PAGE_BASE + g,
				  CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[0][CMIS_VDM_SAMPLE_PAGE_BASE + g] =
				request.data - CMIS_PAGE_SIZE;
	}

	/* Unfreeze VDM samples */
	ret = cmis_vdm_unfreeze(ctx);
	if (ret < 0)
		fprintf(stderr, "Warning: VDM unfreeze failed (%d)\n", ret);

	/* Page 11h (bank 0): for AppSel/modulation detection */
	cmis_request_init(&request, 0, 0x11, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x11] = request.data - CMIS_PAGE_SIZE;

	cmis_show_eye(&map);
	return 0;
}

/*
 * Show diagnostics capabilities via the library API.
 * Loads the CMIS memory map and calls cmis_show_diag().
 */
static int
show_diag(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver;
	int ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];
	if (!(adver & CMIS_PAGES_ADVER_DIAG)) {
		printf("Diagnostics pages (13h-14h) not advertised by this module.\n");
		return 0;
	}

	/* Page 13h */
	cmis_request_init(&request, 0, 0x13, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x13] = request.data - CMIS_PAGE_SIZE;

	/* Page 14h */
	cmis_request_init(&request, 0, 0x14, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret == 0)
		map.upper_memory[0][0x14] = request.data - CMIS_PAGE_SIZE;

	cmis_show_diag(&map);
	return 0;
}

/*
 * Show network path control and status via the library API.
 * Loads the CMIS memory map and calls cmis_show_netpath().
 */
static int
show_netpath(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver, banks_bits;
	int num_banks, i, ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];
	if (!(adver & CMIS_PAGES_ADVER_NETWORK_PATH)) {
		printf("Network Path pages (16h-17h) not advertised by this module.\n");
		return 0;
	}

	/* Determine number of banks */
	banks_bits = adver & CMIS_BANKS_SUPPORTED_MASK;
	if (banks_bits == CMIS_BANK_0_3_SUPPORTED)
		num_banks = 4;
	else if (banks_bits == CMIS_BANK_0_1_SUPPORTED)
		num_banks = 2;
	else
		num_banks = 1;

	/* Derive media lane count */
	map.media_lane_count = 0;
	{
		int base = CMIS_APP_DESC_START_OFFSET;
		__u8 media_lanes = map.lower_memory[base + 2] & 0x0F;
		if (media_lanes > 0)
			map.media_lane_count = media_lanes;
	}

	/* Load Pages 16h and 17h for all banks */
	for (i = 0; i < num_banks; i++) {
		cmis_request_init(&request, i, 0x16, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x16] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, 0x17, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x17] =
				request.data - CMIS_PAGE_SIZE;
	}

	cmis_show_netpath(&map);
	return 0;
}

/*
 * Show data path control and status via the library API.
 * Loads the CMIS memory map and calls cmis_show_datapath().
 */
static int
show_datapath(struct cmd_context *ctx)
{
	struct cmis_memory_map map = {};
	struct module_eeprom request;
	const __u8 *page_01;
	__u8 adver, banks_bits;
	int num_banks, i, ret;

	/* Lower memory */
	cmis_request_init(&request, 0, 0x0, 0);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.lower_memory = request.data;

	/* Page 00h upper */
	cmis_request_init(&request, 0, 0x0, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x0] = request.data - CMIS_PAGE_SIZE;

	/* Check flat memory model */
	if (map.lower_memory[CMIS_MEMORY_MODEL_OFFSET] &
	    CMIS_MEMORY_MODEL_MASK) {
		printf("Flat memory model — no upper pages\n");
		return 0;
	}

	/* Page 01h */
	cmis_request_init(&request, 0, 0x1, CMIS_PAGE_SIZE);
	ret = get_eeprom_page(ctx, &request);
	if (ret < 0)
		return ret;
	map.upper_memory[0][0x1] = request.data - CMIS_PAGE_SIZE;
	page_01 = map.upper_memory[0][0x1];

	adver = page_01[CMIS_PAGES_ADVER_OFFSET];

	/* Determine number of banks */
	banks_bits = adver & CMIS_BANKS_SUPPORTED_MASK;
	if (banks_bits == CMIS_BANK_0_3_SUPPORTED)
		num_banks = 4;
	else if (banks_bits == CMIS_BANK_0_1_SUPPORTED)
		num_banks = 2;
	else
		num_banks = 1;

	/* Derive media lane count */
	map.media_lane_count = 0;
	{
		int base = CMIS_APP_DESC_START_OFFSET;
		__u8 media_lanes = map.lower_memory[base + 2] & 0x0F;
		if (media_lanes > 0)
			map.media_lane_count = media_lanes;
	}

	/* Load Pages 10h and 11h for all banks */
	for (i = 0; i < num_banks; i++) {
		cmis_request_init(&request, i, 0x10, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x10] =
				request.data - CMIS_PAGE_SIZE;

		cmis_request_init(&request, i, 0x11, CMIS_PAGE_SIZE);
		ret = get_eeprom_page(ctx, &request);
		if (ret == 0)
			map.upper_memory[i][0x11] =
				request.data - CMIS_PAGE_SIZE;
	}

	cmis_show_datapath(&map);
	return 0;
}

/*
 * Main CDB feature discovery entry point.
 * Called when --cdb option is used.
 */
static int
cmis_show_cdb(struct cmd_context *ctx)
{
	const __u8 *page_01;
	__u8 cdb_adver, instances, epl_pages;
	__u8 rw_len_ext, trigger;
	int status;

	/* Read Page 01h to check CDB advertisement */
	page_01 = read_cmis_page(ctx, 0, 0x1);
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

	if (!is_json_context()) {
		epl_pages = cdb_adver & CMIS_CDB_ADVER_EPL_MASK;
		rw_len_ext = page_01[CMIS_CDB_ADVER_RW_LEN_OFFSET];
		trigger = page_01[CMIS_CDB_ADVER_TRIGGER_OFFSET];

		printf("CDB (Command Data Block) Messaging\n");
		printf("===================================\n");
		printf("  Instances:        %d\n", instances);
		printf("  Background mode:  %s\n",
		       (cdb_adver & CMIS_CDB_ADVER_MODE_MASK) ? "Yes" : "No");
		printf("  EPL pages:        %d", epl_pages);
		if (epl_pages > 0) {
			static const int epl_count[] = {
				0, 1, 2, 3, 4, 8, 12, 16
			};
			int n = epl_pages < 8 ? epl_count[epl_pages]
					      : epl_pages;
			printf(" (%d bytes max EPL)", n * 128);
		}
		printf("\n");
		printf("  Max EPL RW len:   %d bytes\n",
		       (rw_len_ext + 1) * 8);
		printf("  Max LPL RW len:   %d bytes\n",
		       (rw_len_ext > 15 ? 15 : rw_len_ext) * 8 + 8);
		printf("  Auto-paging:      %s\n",
		       (cdb_adver & CMIS_CDB_ADVER_AUTOPAGING_MASK)
			       ? "Yes" : "No");
		printf("  Trigger method:   %s\n",
		       (trigger & CMIS_CDB_ADVER_TRIGGER_MASK)
			       ? "Single write" : "Two writes");

		/* MaxBusyTime (bytes 165-166) */
		{
			__u8 byte_165 =
				page_01[CMIS_CDB_ADVER_TRIGGER_OFFSET];
			__u8 byte_166 =
				page_01[CMIS_CDB_ADVER_BUSY_OFFSET];
			int timeout_ms;

			if (byte_166 & CMIS_CDB_BUSY_SPEC_METHOD_MASK) {
				int x = byte_165 &
					CMIS_CDB_ADVER_EXT_BUSY_MASK;

				if (x < 1)
					x = 1;
				timeout_ms = x * 160;
				printf("  Max busy time:    %d ms"
				       " (extended method)\n",
				       timeout_ms);
			} else {
				int x = byte_166 &
					CMIS_CDB_BUSY_TIME_MASK;

				if (x > 80)
					x = 80;
				timeout_ms = 80 - x;
				printf("  Max busy time:    %d ms"
				       " (short method)\n",
				       timeout_ms);
			}
		}

		/* Quick CDB health check — read current status */
		status = cmis_cdb_read_status(ctx);
		printf("  Current status:   0x%02x (%s)\n",
		       status >= 0 ? status : 0,
		       status >= 0 ? cmis_cdb_status_str(status)
				   : "I2C error");

		/* Supported Pages Advertisement (Page 01h byte 142) */
		{
			__u8 adver = page_01[CMIS_PAGES_ADVER_OFFSET];
			__u8 banks = adver & CMIS_BANKS_SUPPORTED_MASK;

			printf("\nSupported Pages Advertisement (01h:142)\n");
			printf("---------------------------------------\n");
			printf("  Network Path (16h-17h):  %s\n",
			       (adver & CMIS_PAGES_ADVER_NETWORK_PATH)
				       ? "Yes" : "No");
			printf("  VDM (20h-2Fh):          %s\n",
			       (adver & CMIS_PAGES_ADVER_VDM)
				       ? "Yes" : "No");
			printf("  Diagnostics (13h-14h):   %s\n",
			       (adver & CMIS_PAGES_ADVER_DIAG)
				       ? "Yes" : "No");
			printf("  Coherent/C-CMIS (30h-4Fh): %s\n",
			       (adver & CMIS_PAGES_ADVER_COHERENT)
				       ? "Yes" : "No");
			printf("  CMIS-FF (05h):           %s\n",
			       (adver & CMIS_PAGES_ADVER_CMIS_FF)
				       ? "Yes" : "No");
			printf("  User Page 03h:           %s\n",
			       (adver & CMIS_PAGES_ADVER_PAGE_03H)
				       ? "Yes" : "No");
			printf("  Banks supported:         %s\n",
			       banks == 0x00 ? "0 only" :
			       banks == 0x01 ? "0-1" :
			       banks == 0x02 ? "0-3" : "Reserved");

			/* C-CMIS page hex dump if coherent */
			if (adver & CMIS_PAGES_ADVER_COHERENT) {
				int pg;

				printf("\nC-CMIS Page Dump"
				       " (Pages 30h-4Fh, Bank 0)\n");
				printf("------------------------------"
				       "-----------\n");
				for (pg = 0x30; pg <= 0x4F; pg++) {
					const __u8 *data;
					int j, all_same;

					data = read_cmis_page(ctx, 0, pg);
					if (!data) {
						printf("  Page %02Xh:"
						       " read error,"
						       " stopping\n", pg);
						break;
					}

					all_same = 1;
					for (j = 1; j < 128; j++) {
						if (data[0x80 + j] !=
						    data[0x80]) {
							all_same = 0;
							break;
						}
					}
					if (all_same &&
					    (data[0x80] == 0x00 ||
					     data[0x80] == 0xFF)) {
						printf("  Page %02Xh:"
						       " all 0x%02X"
						       " (skipped)\n",
						       pg, data[0x80]);
						continue;
					}

					printf("  Page %02Xh:\n", pg);
					for (j = 0; j < 128; j++) {
						if (j % 16 == 0)
							printf("    %04x:",
							       0x80 + j);
						printf(" %02x",
						       data[0x80 + j]);
						if (j % 16 == 15)
							printf("\n");
					}
				}
			}
		}
	}

	/* Query standard feature commands (now in the library) */
	cmis_cdb_show_features(ctx);

	/* Probe vendor-specific range (text mode only — slow I2C scan) */
	if (!is_json_context())
		cdb_probe_vendor_commands(ctx);

	return 0;
}

#define RULER_WIDTH 60

static void
show_wavelength_map(struct cmd_context *ctx)
{
	const __u8 *page_01, *page_04, *page_12;
	__u16 nom_wl_raw;
	double nom_wl_nm;
	__u8 grid_msb, grid_lsb;
	bool tunable, fine_tuning, prog_pwr;
	double global_min_freq = 1e18, global_max_freq = -1e18;
	unsigned int i;
	int first_grid_idx = -1;

	/* --- Read pages --- */
	page_01 = read_cmis_page(ctx, 0, 0x1);
	if (!page_01) {
		fprintf(stderr, "Error: cannot read Page 01h\n");
		return;
	}

	/* Nominal wavelength: U16 in units of 0.05 nm */
	nom_wl_raw = OFFSET_TO_U16_PTR(page_01, CMIS_NOM_WAVELENGTH_MSB);
	nom_wl_nm = nom_wl_raw * 0.05;

	/* Check tunable advertisement */
	tunable = page_01[CMIS_TUNABLE_ADVER_OFFSET] &
		  CMIS_TUNABLE_ADVER_MASK;
	if (!tunable) {
		printf("Tunable Laser Wavelength Map\n");
		printf("============================\n");
		printf("Nominal wavelength: %.2f nm\n", nom_wl_nm);
		printf("\nThis module does not advertise tunable laser support.\n");
		return;
	}

	page_04 = read_cmis_page(ctx, 0, 0x4);
	if (!page_04) {
		fprintf(stderr, "Error: cannot read Page 04h\n");
		return;
	}

	page_12 = read_cmis_page(ctx, 0, 0x12);
	/* page_12 may be NULL if not available */

	/* --- Title --- */
	printf("Tunable Laser Wavelength Map\n");
	printf("============================\n");
	printf("Nominal wavelength: %.2f nm\n\n", nom_wl_nm);

	grid_msb = page_04[CMIS_TUNABLE_GRID_SUPPORT_MSB];
	grid_lsb = page_04[CMIS_TUNABLE_GRID_SUPPORT_LSB];

	/* Collect per-grid info and compute global freq range.
	 * Some modules have firmware bugs where the GridSupported bits
	 * don't match the actually populated channel ranges.  We scan
	 * all grids and use any with non-zero channel ranges.
	 */
	struct {
		bool advertised;  /* GridSupported bit set */
		bool populated;   /* non-zero channel range */
		bool usable;      /* populated (includes workaround) */
		__s16 low_ch, high_ch;
		double low_freq, high_freq;
	} gi[NUM_MAP_GRIDS];

	bool has_mismatch = false;

	for (i = 0; i < NUM_MAP_GRIDS; i++) {
		if (map_grids[i].msb_mask)
			gi[i].advertised = grid_msb & map_grids[i].msb_mask;
		else
			gi[i].advertised = grid_lsb & map_grids[i].lsb_mask;

		/* Always read channel ranges regardless of support bit */
		gi[i].low_ch = (__s16)OFFSET_TO_U16_PTR(
			page_04, map_grids[i].low_ch_offset);
		gi[i].high_ch = (__s16)OFFSET_TO_U16_PTR(
			page_04, map_grids[i].high_ch_offset);
		gi[i].populated = !(gi[i].low_ch == 0 &&
				    gi[i].high_ch == 0);

		/* Usable if populated (workaround for firmware bugs) */
		gi[i].usable = gi[i].populated;

		if (gi[i].advertised != gi[i].populated)
			has_mismatch = true;

		if (!gi[i].usable)
			continue;

		gi[i].low_freq = 193.1 +
			(double)gi[i].low_ch * map_grids[i].spacing_mhz /
			1000000.0;
		gi[i].high_freq = 193.1 +
			(double)gi[i].high_ch * map_grids[i].spacing_mhz /
			1000000.0;

		if (gi[i].low_freq < global_min_freq)
			global_min_freq = gi[i].low_freq;
		if (gi[i].high_freq > global_max_freq)
			global_max_freq = gi[i].high_freq;

		if (first_grid_idx < 0)
			first_grid_idx = (int)i;
	}

	/* Show diagnostic when advertised bits don't match populated ranges */
	if (has_mismatch) {
		printf("Warning: GridSupported bits (MSB=0x%02x LSB=0x%02x)"
		       " mismatch with populated channel ranges:\n",
		       grid_msb, grid_lsb);
		for (i = 0; i < NUM_MAP_GRIDS; i++) {
			if (!gi[i].advertised && !gi[i].populated)
				continue;
			printf("  %-12s advertised=%-3s  populated=%-3s",
			       map_grids[i].name,
			       gi[i].advertised ? "yes" : "no",
			       gi[i].populated ? "yes" : "no");
			if (gi[i].populated)
				printf("  ch=[%d..%d]",
				       gi[i].low_ch, gi[i].high_ch);
			if (gi[i].populated && !gi[i].advertised)
				printf("  <- using (firmware bug?)");
			else if (gi[i].advertised && !gi[i].populated)
				printf("  <- skipped (range empty)");
			printf("\n");
		}
		printf("\n");
	}

	if (first_grid_idx < 0) {
		printf("No usable grids found on Page 04h.\n");
		printf("\nPage 04h raw dump (bytes 128-165):\n");
		for (i = 0; i < 38; i++) {
			if (i % 16 == 0)
				printf("  %04x:", 0x80 + i);
			printf(" %02x", page_04[0x80 + i]);
			if (i % 16 == 15 || i == 37)
				printf("\n");
		}
		return;
	}

	/* Add small margin so endpoints aren't right at the edge */
	double range = global_max_freq - global_min_freq;

	if (range < 0.001)
		range = 1.0;

	double margin = range * 0.02;

	global_min_freq -= margin;
	global_max_freq += margin;
	range = global_max_freq - global_min_freq;

	/* Wavelength at ruler endpoints (nm decreases left-to-right) */
	double ruler_left_nm = freq_to_wavelength(global_min_freq);
	double ruler_right_nm = freq_to_wavelength(global_max_freq);
	double center_nm = freq_to_wavelength(193.1);

	printf("Supported grids and tunable wavelength ranges:\n\n");

	/* --- Ruler line --- */
	double center_freq = 193.1;
	int center_pos = (int)((center_freq - global_min_freq) / range *
			       (RULER_WIDTH - 1));

	if (center_pos < 0)
		center_pos = 0;
	if (center_pos >= RULER_WIDTH)
		center_pos = RULER_WIDTH - 1;

	/* Wavelength label line */
	{
		char line[256];
		char left_lbl[32], center_lbl[32], right_lbl[32];
		int len, pad;

		snprintf(left_lbl, sizeof(left_lbl), "%.2f nm",
			 ruler_left_nm);
		snprintf(center_lbl, sizeof(center_lbl), "%.2f nm",
			 center_nm);
		snprintf(right_lbl, sizeof(right_lbl), "%.2f nm",
			 ruler_right_nm);

		memset(line, ' ', sizeof(line));
		pad = 14;
		memcpy(line + pad, left_lbl, strlen(left_lbl));
		len = (int)strlen(center_lbl);
		if (pad + center_pos - len / 2 > 0)
			memcpy(line + pad + center_pos - len / 2,
			       center_lbl, len);
		len = (int)strlen(right_lbl);
		memcpy(line + pad + RULER_WIDTH - len, right_lbl, len);
		line[pad + RULER_WIDTH + 1] = '\0';
		printf("  Wavelength  %s\n", line + pad);
	}

	/* Ruler: |-----+-----| */
	{
		char ruler[RULER_WIDTH + 1];
		int j;

		for (j = 0; j < RULER_WIDTH; j++)
			ruler[j] = '-';
		ruler[0] = '|';
		ruler[RULER_WIDTH - 1] = '|';
		if (center_pos > 0 && center_pos < RULER_WIDTH - 1)
			ruler[center_pos] = '+';
		ruler[RULER_WIDTH] = '\0';
		printf("              %s\n", ruler);
	}

	/* --- Per-grid bars --- */
	for (i = 0; i < NUM_MAP_GRIDS; i++) {
		char bar[RULER_WIDTH + 1];
		int lo_pos, hi_pos, j;
		char lo_lbl[48], hi_lbl[48], ch_lbl[64];

		if (!gi[i].usable)
			continue;

		lo_pos = (int)((gi[i].low_freq - global_min_freq) / range *
			       (RULER_WIDTH - 1));
		hi_pos = (int)((gi[i].high_freq - global_min_freq) / range *
			       (RULER_WIDTH - 1));

		if (lo_pos < 0) lo_pos = 0;
		if (hi_pos >= RULER_WIDTH) hi_pos = RULER_WIDTH - 1;

		memset(bar, ' ', RULER_WIDTH);
		bar[lo_pos] = '[';
		bar[hi_pos] = ']';
		for (j = lo_pos + 1; j < hi_pos; j++)
			bar[j] = '=';
		if (center_pos > lo_pos && center_pos < hi_pos)
			bar[center_pos] = '+';
		bar[RULER_WIDTH] = '\0';

		printf("  %-12s%s%s\n", map_grids[i].name, bar,
		       !gi[i].advertised ? " *" : "");

		/* Channel labels */
		snprintf(lo_lbl, sizeof(lo_lbl), "ch %d", gi[i].low_ch);
		snprintf(hi_lbl, sizeof(hi_lbl), "ch %+d", gi[i].high_ch);
		snprintf(ch_lbl, sizeof(ch_lbl), "ch 0");

		{
			char lbl_line[256];
			int lo_len, clen, hlen, hi_start;
			int end_of_lo;

			lo_len = (int)strlen(lo_lbl);
			clen = (int)strlen(ch_lbl);
			hlen = (int)strlen(hi_lbl);
			hi_start = hi_pos - hlen + 1;

			memset(lbl_line, ' ', sizeof(lbl_line));
			memcpy(lbl_line + lo_pos, lo_lbl, lo_len);
			end_of_lo = lo_pos + lo_len;

			if (center_pos - clen / 2 > end_of_lo + 1 &&
			    center_pos > 0 && center_pos < RULER_WIDTH - 1 &&
			    (hi_start < 0 ||
			     center_pos + clen / 2 < hi_start - 1)) {
				memcpy(lbl_line + center_pos - clen / 2,
				       ch_lbl, clen);
				end_of_lo = center_pos + (clen + 1) / 2;
			}

			if (hi_start > end_of_lo + 1 && hi_start > 0)
				memcpy(lbl_line + hi_start,
				       hi_lbl, hlen);
			lbl_line[RULER_WIDTH + 16] = '\0';
			printf("              %s\n", lbl_line);
		}

		/* Wavelength labels */
		{
			char wl_line[256];
			char wlo[32], whi[32];
			int wlo_len, whi_len, whi_start;

			snprintf(wlo, sizeof(wlo), "%.2f nm",
				 freq_to_wavelength(gi[i].low_freq));
			snprintf(whi, sizeof(whi), "%.2f nm",
				 freq_to_wavelength(gi[i].high_freq));
			wlo_len = (int)strlen(wlo);
			whi_len = (int)strlen(whi);
			whi_start = hi_pos - whi_len + 1;
			memset(wl_line, ' ', sizeof(wl_line));
			memcpy(wl_line + lo_pos, wlo, wlo_len);
			/* Only place high label if it won't overlap low */
			if (whi_start > lo_pos + wlo_len &&
			    whi_start > 0)
				memcpy(wl_line + whi_start,
				       whi, whi_len);
			wl_line[RULER_WIDTH + 16] = '\0';
			printf("              %s\n\n", wl_line);
		}
	}

	/* --- Fine tuning --- */
	fine_tuning = grid_lsb & CMIS_FINE_TUNING_SUPPORTED;
	if (fine_tuning) {
		double res_ghz, lo_ghz, hi_ghz;

		res_ghz = (double)OFFSET_TO_U16_PTR(page_04,
			   CMIS_FINE_TUNING_RESOLUTION) * 0.001;
		lo_ghz = (double)(__s16)OFFSET_TO_U16_PTR(page_04,
			  CMIS_FINE_TUNING_LOW_OFFSET) * 0.001;
		hi_ghz = (double)(__s16)OFFSET_TO_U16_PTR(page_04,
			  CMIS_FINE_TUNING_HIGH_OFFSET) * 0.001;

		printf("Fine tuning : Yes  resolution %.4f nm"
		       "  range: %.3f .. +%.3f nm\n",
		       ghz_offset_to_nm(res_ghz, nom_wl_nm),
		       -ghz_offset_to_nm(-lo_ghz, nom_wl_nm),
		       ghz_offset_to_nm(hi_ghz, nom_wl_nm));
	} else {
		printf("Fine tuning : No\n");
	}

	/* --- Programmable output power --- */
	prog_pwr = page_04[CMIS_PROG_OUT_PWR_SUPPORT] &
		   CMIS_PROG_OUT_PWR_SUPPORT_MASK;
	if (prog_pwr) {
		__s16 pwr_min = (__s16)OFFSET_TO_U16_PTR(page_04,
				 CMIS_PROG_OUT_PWR_MIN);
		__s16 pwr_max = (__s16)OFFSET_TO_U16_PTR(page_04,
				 CMIS_PROG_OUT_PWR_MAX);

		printf("Output power: %.2f .. %.2f dBm\n",
		       (double)pwr_min * 0.01, (double)pwr_max * 0.01);
	}

	/* --- Current lane status (bank 0) --- */
	printf("\nCurrent lane status (bank 0):\n");
	if (!page_12) {
		printf("  (Page 12h not available)\n");
	} else {
		for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
			__u8 grid_byte, genc, status;
			__s16 ch_num;
			__u32 curr_freq;
			double freq_thz, wl_nm;
			bool unlocked;

			grid_byte = page_12[CMIS_TUNABLE_GRID_SPACING_TX(i)];
			genc = (grid_byte & CMIS_TUNABLE_GRID_SPACING_MASK) >>
			       CMIS_TUNABLE_GRID_SPACING_SHIFT;
			ch_num = (__s16)OFFSET_TO_U16_PTR(page_12,
				 CMIS_TUNABLE_CHANNEL_NUM_TX(i));

			curr_freq =
				((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i)] << 24) |
				((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 1] << 16) |
				((__u32)page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 2] << 8) |
				page_12[CMIS_TUNABLE_CURR_FREQ_TX(i) + 3];

			status = page_12[CMIS_TUNABLE_STATUS_TX(i)];
			unlocked = status & CMIS_TUNABLE_STATUS_UNLOCKED;

			freq_thz = (double)curr_freq / 1000000.0;
			wl_nm = freq_to_wavelength(freq_thz);

			if (curr_freq == 0) {
				printf("  Lane %d  --------    %-12s %-8s %s\n",
				       (int)i + 1, "----", "----",
				       unlocked ? "o UNLOCKED" : "* LOCKED");
			} else {
				printf("  Lane %d  %.2f nm  %-12s ch=%-5d %s\n",
				       (int)i + 1, wl_nm,
				       grid_enc_to_name(genc), ch_num,
				       unlocked ? "o UNLOCKED" : "* LOCKED");
			}
		}
	}

	/* --- Tune examples --- */
	if (first_grid_idx >= 0) {
		int fi = first_grid_idx;
		__s16 lo = gi[fi].low_ch;
		__s16 hi = gi[fi].high_ch;

		printf("\nTune examples:\n");
		printf("  --tune --grid %-3s --channel %-6d ->  %.2f nm\n",
		       map_grids[fi].name, 0,
		       freq_to_wavelength(193.1));
		printf("  --tune --grid %-3s --channel %-6d ->  %.2f nm\n",
		       map_grids[fi].name, lo,
		       freq_to_wavelength(gi[fi].low_freq));
		printf("  --tune --grid %-3s --channel %-+6d ->  %.2f nm\n",
		       map_grids[fi].name, hi,
		       freq_to_wavelength(gi[fi].high_freq));
	}
}

static const struct option long_options[] = {
	{ "help",               no_argument,       NULL, 'h' },
	{ "json",               no_argument,       NULL, 'j' },
	{ "debug",              required_argument, NULL, 'd' },
	{ "include-statistics", no_argument,       NULL, 'I' },
	{ "map",                no_argument,       NULL, 'm' },
	{ "wake",               no_argument,       NULL, 'w' },
	{ "sleep",              no_argument,       NULL, 's' },
	{ "cdb",                no_argument,       NULL, 'C' },
	{ "pm",                 no_argument,       NULL, 'P' },
	{ "coherent",           no_argument,       NULL, 'K' },
	{ "datapath",           no_argument,       NULL, 'D' },
	{ "netpath",            no_argument,       NULL, 'N' },
	{ "diag",               no_argument,       NULL, 256 },
	{ "vdm",                no_argument,       NULL, 'V' },
	{ "tune",               no_argument,       NULL, 't' },
	{ "grid",               required_argument, NULL, 'g' },
	{ "channel",            required_argument, NULL, 'c' },
	{ "lane",               required_argument, NULL, 'l' },
	{ "bank",               required_argument, NULL, 'b' },
	{ "fine-offset",        required_argument, NULL, 'f' },
	{ "power",              required_argument, NULL, 'p' },
	{ "fw-update",          required_argument, NULL, 257 },
	{ "fw-run",             optional_argument, NULL, 258 },
	{ "fw-commit",          no_argument,       NULL, 259 },
	{ "fw-copy",            required_argument, NULL, 260 },
	{ "fw-abort",           no_argument,       NULL, 261 },
	{ "cert",               no_argument,       NULL, 262 },
	{ "cert-file",          required_argument, NULL, 263 },
	{ "auth",               required_argument, NULL, 264 },
	{ "bert-start",         required_argument, NULL, 265 },
	{ "bert-stop",          no_argument,       NULL, 266 },
	{ "bert-read",          no_argument,       NULL, 267 },
	{ "loopback-start",     required_argument, NULL, 268 },
	{ "loopback-stop",      no_argument,       NULL, 269 },
	{ "tx-disable",         no_argument,       NULL, 270 },
	{ "tx-enable",          no_argument,       NULL, 271 },
	{ "dp-deactivate",      no_argument,       NULL, 272 },
	{ "dp-activate",        no_argument,       NULL, 273 },
	{ "dp-config",          no_argument,       NULL, 274 },
	{ "appsel",             required_argument, NULL, 275 },
	{ "dpid",               required_argument, NULL, 276 },
	{ "explicit",           no_argument,       NULL, 277 },
	{ "cdr",                required_argument, NULL, 278 },
	{ "set-threshold",      required_argument, NULL, 279 },
	{ "threshold-enable",   required_argument, NULL, 280 },
	{ "threshold-disable",  required_argument, NULL, 281 },
	/* Page 10h signal integrity controls */
	{ "polarity-tx",        required_argument, NULL, 282 },
	{ "polarity-rx",        required_argument, NULL, 283 },
	{ "rx-disable",         no_argument,       NULL, 284 },
	{ "rx-enable",          no_argument,       NULL, 285 },
	{ "auto-squelch-tx",    required_argument, NULL, 286 },
	{ "auto-squelch-rx",    required_argument, NULL, 287 },
	{ "squelch-force-tx",   required_argument, NULL, 288 },
	{ "eq-freeze-tx",       required_argument, NULL, 289 },
	{ "eq-enable-tx",       required_argument, NULL, 290 },
	{ "eq-target-tx",       required_argument, NULL, 291 },
	{ "rx-eq-pre",          required_argument, NULL, 292 },
	{ "rx-eq-post",         required_argument, NULL, 293 },
	{ "rx-amplitude",       required_argument, NULL, 294 },
	{ "lane-mask",          required_argument, NULL, 295 },
	/* Module global controls */
	{ "sw-reset",           no_argument,       NULL, 296 },
	{ "bank-broadcast",     required_argument, NULL, 297 },
	{ "squelch-method",     required_argument, NULL, 298 },
	{ "module-mask",        required_argument, NULL, 299 },
	{ "password-entry",     required_argument, NULL, 300 },
	{ "password-change",    required_argument, NULL, 301 },
	/* VDM controls */
	{ "vdm-freeze",         no_argument,       NULL, 302 },
	{ "vdm-unfreeze",       no_argument,       NULL, 303 },
	{ "vdm-power-saving",   required_argument, NULL, 304 },
	{ "vdm-mask",           required_argument, NULL, 305 },
	/* C-CMIS coherent writes */
	{ "tx-filter",          required_argument, NULL, 306 },
	{ "tx-filter-type",     required_argument, NULL, 307 },
	{ "lf-insertion",       required_argument, NULL, 308 },
	{ "media-mask",         required_argument, NULL, 309 },
	{ "host-threshold",     required_argument, NULL, 310 },
	{ "host-threshold-enable",  required_argument, NULL, 311 },
	{ "host-threshold-disable", required_argument, NULL, 312 },
	{ "host-mask",          required_argument, NULL, 313 },
	/* Network path controls */
	{ "np-deactivate",      no_argument,       NULL, 314 },
	{ "np-activate",        no_argument,       NULL, 315 },
	{ "np-config",          no_argument,       NULL, 316 },
	{ "npid",               required_argument, NULL, 317 },
	{ "np-in-use",          no_argument,       NULL, 318 },
	{ "hp-source-rx",       required_argument, NULL, 319 },
	{ "np-source-tx",       required_argument, NULL, 320 },
	{ "np-state-mask",      required_argument, NULL, 321 },
	/* Diagnostics writes */
	{ "diag-mask",          required_argument, NULL, 322 },
	{ "scratchpad-read",    no_argument,       NULL, 323 },
	{ "scratchpad-write",   required_argument, NULL, 324 },
	{ "user-pattern",       required_argument, NULL, 325 },
	/* User EEPROM */
	{ "eeprom-read",        required_argument, NULL, 326 },
	{ "eeprom-write",       required_argument, NULL, 327 },
	/* Host lane switching */
	{ "lane-redir",         required_argument, NULL, 328 },
	{ "lane-switch-enable", no_argument,       NULL, 329 },
	{ "lane-switch-disable",no_argument,       NULL, 330 },
	{ "lane-switch-commit", no_argument,       NULL, 331 },
	{ "lane-switch-result", no_argument,       NULL, 332 },
	/* Tunable masks */
	{ "tuning-mask",        required_argument, NULL, 333 },
	/* Eye diagram */
	{ "eye",                no_argument,       NULL, 'E' },
	{ NULL, 0, NULL, 0 }
};

int
main(int argc, char *argv[])
{
	struct cmd_context ctx = { 0 };
	char desc[BUFSIZ];
	int opt;

	bool map_mode = false;
	bool wake_mode = false;
	bool sleep_mode = false;
	bool cdb_mode = false;
	bool pm_mode = false;
	bool coherent_mode = false;
	bool datapath_mode = false;
	bool netpath_mode = false;
	bool diag_mode = false;
	bool vdm_mode = false;
	bool eye_mode = false;

	/* Tune-related locals */
	bool tune_mode = false;
	int grid_enc = -1;
	bool channel_set = false;
	int channel = 0;
	int lane = -1;		/* -1 = all lanes */
	int bank = 0;
	int fine_offset = 0;
	int target_power = 0;

	/* FW update locals */
	const char *fw_update_file = NULL;
	bool fw_run_mode = false;
	int fw_run_val = 0;
	bool fw_commit_mode = false;
	const char *fw_copy_dir = NULL;
	bool fw_abort_mode = false;

	/* Security locals */
	bool cert_mode = false;
	const char *cert_file = NULL;
	const char *auth_digest_hex = NULL;

	/* DP write locals */
	bool tx_disable_mode = false;
	bool tx_enable_mode = false;
	bool dp_deactivate_mode = false;
	bool dp_activate_mode = false;
	bool dp_config_mode = false;
	int appsel_val = 0;
	int dpid_val = 0;
	bool explicit_ctl = false;
	const char *cdr_mode_str = NULL;

	/* Threshold locals */
	const char *set_threshold_type = NULL;
	const char *threshold_enable_type = NULL;
	const char *threshold_disable_type = NULL;

	/* Page 10h signal integrity locals */
	const char *polarity_tx_str = NULL;
	const char *polarity_rx_str = NULL;
	bool rx_disable_mode = false;
	bool rx_enable_mode = false;
	const char *auto_squelch_tx_str = NULL;
	const char *auto_squelch_rx_str = NULL;
	const char *squelch_force_tx_str = NULL;
	const char *eq_freeze_tx_str = NULL;
	const char *eq_enable_tx_str = NULL;
	int eq_target_tx_val = -1;
	int rx_eq_pre_val = -1;
	int rx_eq_post_val = -1;
	int rx_amplitude_val = -1;
	const char *lane_mask_str = NULL;

	/* Module global locals */
	bool sw_reset_mode = false;
	const char *bank_broadcast_str = NULL;
	const char *squelch_method_str = NULL;
	const char *module_mask_str = NULL;
	const char *password_entry_str = NULL;
	const char *password_change_str = NULL;

	/* VDM control locals */
	bool vdm_freeze_mode = false;
	bool vdm_unfreeze_mode = false;
	const char *vdm_power_saving_str = NULL;
	const char *vdm_mask_str = NULL;

	/* C-CMIS coherent write locals */
	const char *tx_filter_str = NULL;
	int tx_filter_type_val = -1;
	const char *lf_insertion_str = NULL;
	const char *media_mask_str = NULL;
	const char *host_threshold_str = NULL;
	const char *host_threshold_enable_str = NULL;
	const char *host_threshold_disable_str = NULL;
	const char *host_mask_str = NULL;

	/* NP control locals */
	bool np_deactivate_mode = false;
	bool np_activate_mode = false;
	bool np_config_mode = false;
	int npid_val = -1;
	bool np_in_use = false;
	const char *hp_source_rx_str = NULL;
	const char *np_source_tx_str = NULL;
	const char *np_state_mask_str = NULL;

	/* Diagnostics write locals */
	const char *diag_mask_str = NULL;
	bool scratchpad_read_mode = false;
	const char *scratchpad_write_hex = NULL;
	const char *user_pattern_hex = NULL;

	/* User EEPROM locals */
	const char *eeprom_read_str = NULL;
	const char *eeprom_write_str = NULL;

	/* Host lane switching locals */
	int lane_redir_val = -1;
	bool lane_switch_enable_mode = false;
	bool lane_switch_disable_mode = false;
	bool lane_switch_commit_mode = false;
	bool lane_switch_result_mode = false;

	/* Tunable mask locals */
	const char *tuning_mask_str = NULL;

	/* BERT locals */
	const char *bert_pattern_name = NULL;
	bool bert_stop_mode = false;
	bool bert_read_mode = false;

	/* Loopback locals */
	const char *loopback_mode_name = NULL;
	bool loopback_stop_mode = false;

	while ((opt = getopt_long(argc, argv, "hjd:ICPKDNEVtmwsg:c:l:b:f:p:",
				  long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 'j':
			ctx.json = true;
			break;
		case 'd':
			ctx.debug = strtoul(optarg, NULL, 0);
			break;
		case 'I':
			ctx.show_stats = true;
			break;
		case 'C':
			cdb_mode = true;
			break;
		case 'P':
			pm_mode = true;
			break;
		case 'K':
			coherent_mode = true;
			break;
		case 'D':
			datapath_mode = true;
			break;
		case 'N':
			netpath_mode = true;
			break;
		case 256:
			diag_mode = true;
			break;
		case 'V':
			vdm_mode = true;
			break;
		case 'E':
			eye_mode = true;
			break;
		case 'm':
			map_mode = true;
			break;
		case 'w':
			wake_mode = true;
			break;
		case 's':
			sleep_mode = true;
			break;
		case 't':
			tune_mode = true;
			break;
		case 'g':
			grid_enc = parse_grid_ghz(optarg);
			if (grid_enc < 0) {
				fprintf(stderr, "Invalid grid spacing: %s\n", optarg);
				fprintf(stderr, "Valid values: 3.125, 6.25, 12.5, 25, 33, 50, 75, 100, 150\n");
				return EXIT_FAILURE;
			}
			break;
		case 'c':
			channel = (int)strtol(optarg, NULL, 0);
			channel_set = true;
			break;
		case 'l':
			lane = (int)strtol(optarg, NULL, 0);
			if (lane < 0 || lane > 7) {
				fprintf(stderr, "Lane must be 0-7, got: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'b':
			bank = (int)strtol(optarg, NULL, 0);
			if (bank < 0 || bank > 3) {
				fprintf(stderr, "Bank must be 0-3, got: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'f':
			fine_offset = (int)strtol(optarg, NULL, 0);
			break;
		case 'p':
			target_power = (int)strtol(optarg, NULL, 0);
			break;
		case 257:
			fw_update_file = optarg;
			break;
		case 258:
			fw_run_mode = true;
			if (optarg)
				fw_run_val = (int)strtol(optarg, NULL, 0);
			break;
		case 259:
			fw_commit_mode = true;
			break;
		case 260:
			fw_copy_dir = optarg;
			break;
		case 261:
			fw_abort_mode = true;
			break;
		case 262:
			cert_mode = true;
			break;
		case 263:
			cert_file = optarg;
			break;
		case 264:
			auth_digest_hex = optarg;
			break;
		case 265:
			bert_pattern_name = optarg;
			break;
		case 266:
			bert_stop_mode = true;
			break;
		case 267:
			bert_read_mode = true;
			break;
		case 268:
			loopback_mode_name = optarg;
			break;
		case 269:
			loopback_stop_mode = true;
			break;
		case 270:
			tx_disable_mode = true;
			break;
		case 271:
			tx_enable_mode = true;
			break;
		case 272:
			dp_deactivate_mode = true;
			break;
		case 273:
			dp_activate_mode = true;
			break;
		case 274:
			dp_config_mode = true;
			break;
		case 275:
			appsel_val = (int)strtol(optarg, NULL, 0);
			if (appsel_val < 1 || appsel_val > 15) {
				fprintf(stderr,
					"AppSel must be 1-15, got: %s\n",
					optarg);
				return EXIT_FAILURE;
			}
			break;
		case 276:
			dpid_val = (int)strtol(optarg, NULL, 0);
			if (dpid_val < 0 || dpid_val > 7) {
				fprintf(stderr,
					"DPID must be 0-7, got: %s\n",
					optarg);
				return EXIT_FAILURE;
			}
			break;
		case 277:
			explicit_ctl = true;
			break;
		case 278:
			cdr_mode_str = optarg;
			break;
		case 279:
			set_threshold_type = optarg;
			break;
		case 280:
			threshold_enable_type = optarg;
			break;
		case 281:
			threshold_disable_type = optarg;
			break;
		/* Page 10h signal integrity */
		case 282:
			polarity_tx_str = optarg;
			break;
		case 283:
			polarity_rx_str = optarg;
			break;
		case 284:
			rx_disable_mode = true;
			break;
		case 285:
			rx_enable_mode = true;
			break;
		case 286:
			auto_squelch_tx_str = optarg;
			break;
		case 287:
			auto_squelch_rx_str = optarg;
			break;
		case 288:
			squelch_force_tx_str = optarg;
			break;
		case 289:
			eq_freeze_tx_str = optarg;
			break;
		case 290:
			eq_enable_tx_str = optarg;
			break;
		case 291:
			eq_target_tx_val = (int)strtol(optarg, NULL, 0);
			if (eq_target_tx_val < 0 || eq_target_tx_val > 15) {
				fprintf(stderr,
					"EQ target must be 0-15\n");
				return EXIT_FAILURE;
			}
			break;
		case 292:
			rx_eq_pre_val = (int)strtol(optarg, NULL, 0);
			if (rx_eq_pre_val < 0 || rx_eq_pre_val > 15) {
				fprintf(stderr,
					"Rx EQ pre must be 0-15\n");
				return EXIT_FAILURE;
			}
			break;
		case 293:
			rx_eq_post_val = (int)strtol(optarg, NULL, 0);
			if (rx_eq_post_val < 0 || rx_eq_post_val > 15) {
				fprintf(stderr,
					"Rx EQ post must be 0-15\n");
				return EXIT_FAILURE;
			}
			break;
		case 294:
			rx_amplitude_val = (int)strtol(optarg, NULL, 0);
			if (rx_amplitude_val < 0 || rx_amplitude_val > 15) {
				fprintf(stderr,
					"Rx amplitude must be 0-15\n");
				return EXIT_FAILURE;
			}
			break;
		case 295:
			lane_mask_str = optarg;
			break;
		/* Module global */
		case 296:
			sw_reset_mode = true;
			break;
		case 297:
			bank_broadcast_str = optarg;
			break;
		case 298:
			squelch_method_str = optarg;
			break;
		case 299:
			module_mask_str = optarg;
			break;
		case 300:
			password_entry_str = optarg;
			break;
		case 301:
			password_change_str = optarg;
			break;
		/* VDM */
		case 302:
			vdm_freeze_mode = true;
			break;
		case 303:
			vdm_unfreeze_mode = true;
			break;
		case 304:
			vdm_power_saving_str = optarg;
			break;
		case 305:
			vdm_mask_str = optarg;
			break;
		/* C-CMIS coherent writes */
		case 306:
			tx_filter_str = optarg;
			break;
		case 307:
			tx_filter_type_val = (int)strtol(optarg, NULL, 0);
			if (tx_filter_type_val < 0 || tx_filter_type_val > 3) {
				fprintf(stderr,
					"Tx filter type must be 0-3\n");
				return EXIT_FAILURE;
			}
			break;
		case 308:
			lf_insertion_str = optarg;
			break;
		case 309:
			media_mask_str = optarg;
			break;
		case 310:
			host_threshold_str = optarg;
			break;
		case 311:
			host_threshold_enable_str = optarg;
			break;
		case 312:
			host_threshold_disable_str = optarg;
			break;
		case 313:
			host_mask_str = optarg;
			break;
		/* NP controls */
		case 314:
			np_deactivate_mode = true;
			break;
		case 315:
			np_activate_mode = true;
			break;
		case 316:
			np_config_mode = true;
			break;
		case 317:
			npid_val = (int)strtol(optarg, NULL, 0);
			if (npid_val < 0 || npid_val > 15) {
				fprintf(stderr,
					"NPID must be 0-15\n");
				return EXIT_FAILURE;
			}
			break;
		case 318:
			np_in_use = true;
			break;
		case 319:
			hp_source_rx_str = optarg;
			break;
		case 320:
			np_source_tx_str = optarg;
			break;
		case 321:
			np_state_mask_str = optarg;
			break;
		/* Diagnostics writes */
		case 322:
			diag_mask_str = optarg;
			break;
		case 323:
			scratchpad_read_mode = true;
			break;
		case 324:
			scratchpad_write_hex = optarg;
			break;
		case 325:
			user_pattern_hex = optarg;
			break;
		/* User EEPROM */
		case 326:
			eeprom_read_str = optarg;
			break;
		case 327:
			eeprom_write_str = optarg;
			break;
		/* Host lane switching */
		case 328:
			lane_redir_val = (int)strtol(optarg, NULL, 0);
			if (lane_redir_val < 0 || lane_redir_val > 7) {
				fprintf(stderr,
					"Redir lane must be 0-7\n");
				return EXIT_FAILURE;
			}
			break;
		case 329:
			lane_switch_enable_mode = true;
			break;
		case 330:
			lane_switch_disable_mode = true;
			break;
		case 331:
			lane_switch_commit_mode = true;
			break;
		case 332:
			lane_switch_result_mode = true;
			break;
		case 333:
			tuning_mask_str = optarg;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* Some options consume extra positional args before the bus number.
	 * Count how many extra positional args are expected.
	 */
	{
		int extra_args = 0;

		if (set_threshold_type)	extra_args = 1;  /* <value> */
		if (lane_mask_str)	extra_args = 2;  /* <mask> <on|off> */
		if (module_mask_str)	extra_args = 2;  /* <mask> <on|off> */
		if (media_mask_str)	extra_args = 2;  /* <mask> <on|off> */
		if (host_mask_str)	extra_args = 2;  /* <mask> <on|off> */
		if (host_threshold_str)	extra_args = 1;  /* <value> */
		if (password_change_str) extra_args = 1; /* <new_pw> */
		if (vdm_mask_str)	extra_args = 1;  /* <nibble> */
		if (diag_mask_str)	extra_args = 1;  /* <on|off> */
		if (eeprom_read_str)	extra_args = 1;  /* <len> */
		if (eeprom_write_str)	extra_args = 1;  /* <hex-data> */
		if (tuning_mask_str)	extra_args = 1;  /* <on|off> */

		if (argc - optind != 1 + extra_args) {
			fprintf(stderr,
				"Error: expected %d argument(s) after options: "
				"<i2c_bus_number>\n", 1 + extra_args);
			usage(argv[0]);
			return EXIT_FAILURE;
		}
		ctx.bus_num = atoi(argv[argc - 1]);
	}

	/* Validate --tune requires --grid and --channel */
	if (tune_mode) {
		if (grid_enc < 0) {
			fprintf(stderr, "Error: --tune requires --grid\n");
			usage(argv[0]);
			return EXIT_FAILURE;
		}
		if (!channel_set) {
			fprintf(stderr, "Error: --tune requires --channel\n");
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (i2c_init(&ctx))
		return EXIT_FAILURE;

	new_json_obj_plain(ctx.json);
	if (ctx.json)
		atexit(delete_json_obj);

	if (!is_json_context())
		printf("i2c_get_device_desc: %s\n",
		       i2c_get_device_desc(ctx.device, desc, sizeof(desc)));

	if (wake_mode && sleep_mode) {
		fprintf(stderr, "Error: --wake and --sleep are mutually exclusive\n");
		return EXIT_FAILURE;
	}

	if (wake_mode) {
		if (cmis_wake(&ctx))
			return EXIT_FAILURE;
	}

	if (sleep_mode) {
		if (cmis_sleep(&ctx))
			return EXIT_FAILURE;
		/* After sleep, display module state and exit */
		int ret = eeprom_parse(&ctx);
		return ret ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	if (cdb_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (cmis_show_cdb(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (pm_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (cmis_cdb_show_pm(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (coherent_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_coherent_pm(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (datapath_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_datapath(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (netpath_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_netpath(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (diag_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_diag(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (bert_pattern_name) {
		struct cmis_bert_config cfg = { 0 };
		int pat_idx;

		pat_idx = cmis_diag_pattern_lookup(bert_pattern_name);
		if (pat_idx < 0) {
			fprintf(stderr, "Unknown PRBS pattern: %s\n",
				bert_pattern_name);
			fprintf(stderr, "Valid patterns: PRBS-31Q, PRBS-31, "
				"PRBS-23Q, PRBS-23, PRBS-15Q, PRBS-15, "
				"PRBS-13Q, PRBS-13, PRBS-9Q, PRBS-9, "
				"PRBS-7Q, PRBS-7, SSPRQ, Custom, User\n");
			return EXIT_FAILURE;
		}
		cfg.pattern = pat_idx;
		cfg.lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		cfg.host_gen = true;
		cfg.media_chk = true;
		if (cmis_diag_bert_start(&ctx, bank, &cfg))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (bert_stop_mode) {
		if (cmis_diag_bert_stop(&ctx, bank))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (bert_read_mode) {
		if (ctx.json)
			new_json_obj(ctx.json);
		if (cmis_diag_bert_read(&ctx, bank)) {
			if (ctx.json)
				delete_json_obj();
			return EXIT_FAILURE;
		}
		if (ctx.json)
			delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (loopback_mode_name) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;

		if (cmis_diag_loopback_start(&ctx, bank,
					     loopback_mode_name, lanes))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (loopback_stop_mode) {
		if (cmis_diag_loopback_stop(&ctx, bank))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (tx_disable_mode) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;

		if (cmis_dp_tx_disable(&ctx, bank, lanes))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (tx_enable_mode) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;

		if (cmis_dp_tx_enable(&ctx, bank, lanes))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (dp_deactivate_mode) {
		if (lane >= 0) {
			if (cmis_dp_deinit_lane(&ctx, bank, lane))
				return EXIT_FAILURE;
			printf("Lane %d: DPDeactivated\n", lane);
		} else {
			int i;

			for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
				if (cmis_dp_deinit_lane(&ctx, bank, i))
					return EXIT_FAILURE;
				printf("Lane %d: DPDeactivated\n", i);
			}
		}
		return EXIT_SUCCESS;
	}

	if (dp_activate_mode) {
		if (lane >= 0) {
			if (cmis_dp_init_lane(&ctx, bank, lane))
				return EXIT_FAILURE;
			printf("Lane %d: DPActivated\n", lane);
		} else {
			int i;

			for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
				if (cmis_dp_init_lane(&ctx, bank, i))
					return EXIT_FAILURE;
				printf("Lane %d: DPActivated\n", i);
			}
		}
		return EXIT_SUCCESS;
	}

	if (dp_config_mode) {
		struct cmis_dp_config cfg = {
			.appsel = appsel_val,
			.dpid = dpid_val,
			.explicit_ctl = explicit_ctl,
		};

		if (lane < 0) {
			fprintf(stderr,
				"Error: --dp-config requires --lane\n");
			return EXIT_FAILURE;
		}
		if (appsel_val < 1) {
			fprintf(stderr,
				"Error: --dp-config requires --appsel\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_configure_lane(&ctx, bank, lane, &cfg))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (cdr_mode_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		bool cdr_on;

		if (strcmp(cdr_mode_str, "on") == 0)
			cdr_on = true;
		else if (strcmp(cdr_mode_str, "off") == 0)
			cdr_on = false;
		else {
			fprintf(stderr,
				"Error: --cdr requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_cdr(&ctx, bank, lanes, cdr_on, cdr_on))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (set_threshold_type) {
		double value;
		char *endp;

		value = strtod(argv[optind], &endp);
		if (*endp != '\0') {
			fprintf(stderr,
				"Error: invalid threshold value: %s\n",
				argv[optind]);
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_threshold(&ctx, bank,
						set_threshold_type, value))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (threshold_enable_type) {
		if (cmis_coherent_threshold_enable(&ctx, bank,
						   threshold_enable_type,
						   true))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (threshold_disable_type) {
		if (cmis_coherent_threshold_enable(&ctx, bank,
						   threshold_disable_type,
						   false))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- Page 10h signal integrity controls --- */

	if (polarity_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		bool flip;

		if (strcmp(polarity_tx_str, "flip") == 0)
			flip = true;
		else if (strcmp(polarity_tx_str, "normal") == 0)
			flip = false;
		else {
			fprintf(stderr,
				"Error: --polarity-tx requires 'flip' or 'normal'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_polarity_tx(&ctx, bank, lanes, flip))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (polarity_rx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		bool flip;

		if (strcmp(polarity_rx_str, "flip") == 0)
			flip = true;
		else if (strcmp(polarity_rx_str, "normal") == 0)
			flip = false;
		else {
			fprintf(stderr,
				"Error: --polarity-rx requires 'flip' or 'normal'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_polarity_rx(&ctx, bank, lanes, flip))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (rx_disable_mode) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;

		if (cmis_dp_rx_disable(&ctx, bank, lanes))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (rx_enable_mode) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;

		if (cmis_dp_rx_enable(&ctx, bank, lanes))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (auto_squelch_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(auto_squelch_tx_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --auto-squelch-tx requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		/* "on" means auto-squelch is active = disable bit cleared */
		if (cmis_dp_set_auto_squelch_tx(&ctx, bank, lanes, !onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (auto_squelch_rx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(auto_squelch_rx_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --auto-squelch-rx requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_auto_squelch_rx(&ctx, bank, lanes, !onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (squelch_force_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(squelch_force_tx_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --squelch-force-tx requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_squelch_force_tx(&ctx, bank, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (eq_freeze_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(eq_freeze_tx_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --eq-freeze-tx requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_adapt_eq_freeze_tx(&ctx, bank, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (eq_enable_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(eq_enable_tx_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --eq-enable-tx requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_adapt_eq_enable_tx(&ctx, bank, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (eq_target_tx_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --eq-target-tx requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_fixed_eq_tx(&ctx, bank, lane, eq_target_tx_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (rx_eq_pre_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --rx-eq-pre requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_rx_eq_pre(&ctx, bank, lane, rx_eq_pre_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (rx_eq_post_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --rx-eq-post requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_rx_eq_post(&ctx, bank, lane, rx_eq_post_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (rx_amplitude_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --rx-amplitude requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_rx_amplitude(&ctx, bank, lane, rx_amplitude_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (lane_mask_str) {
		__u8 offset = (__u8)strtoul(lane_mask_str, NULL, 0);
		__u8 mask = (__u8)strtoul(argv[optind], NULL, 0);
		int onoff = parse_on_off(argv[optind + 1]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --lane-mask <offset> <mask> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (cmis_dp_set_lane_mask(&ctx, bank, offset, mask, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- Module global controls --- */

	if (sw_reset_mode) {
		if (cmis_module_sw_reset(&ctx))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (bank_broadcast_str) {
		int onoff = parse_on_off(bank_broadcast_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --bank-broadcast requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_module_set_bank_broadcast(&ctx, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (squelch_method_str) {
		bool pav;

		if (strcmp(squelch_method_str, "oma") == 0)
			pav = false;
		else if (strcmp(squelch_method_str, "pav") == 0)
			pav = true;
		else {
			fprintf(stderr,
				"Error: --squelch-method requires 'oma' or 'pav'\n");
			return EXIT_FAILURE;
		}
		if (cmis_module_set_squelch_method(&ctx, pav))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (module_mask_str) {
		__u8 offset = (__u8)strtoul(module_mask_str, NULL, 0);
		__u8 mask = (__u8)strtoul(argv[optind], NULL, 0);
		int onoff = parse_on_off(argv[optind + 1]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --module-mask <offset> <mask> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (cmis_module_set_mask(&ctx, offset, mask, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (password_entry_str) {
		__u32 pw = (__u32)strtoul(password_entry_str, NULL, 16);

		if (cmis_module_password_entry(&ctx, pw))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (password_change_str) {
		__u32 cur_pw = (__u32)strtoul(password_change_str, NULL, 16);
		__u32 new_pw = (__u32)strtoul(argv[optind], NULL, 16);

		if (cmis_module_password_change(&ctx, cur_pw, new_pw))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- VDM controls --- */

	if (vdm_freeze_mode) {
		if (cmis_vdm_freeze(&ctx))
			return EXIT_FAILURE;
		printf("VDM frozen\n");
		return EXIT_SUCCESS;
	}

	if (vdm_unfreeze_mode) {
		if (cmis_vdm_unfreeze(&ctx))
			return EXIT_FAILURE;
		printf("VDM unfrozen\n");
		return EXIT_SUCCESS;
	}

	if (vdm_power_saving_str) {
		int onoff = parse_on_off(vdm_power_saving_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --vdm-power-saving requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_vdm_set_power_saving(&ctx, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (vdm_mask_str) {
		int instance = (int)strtol(vdm_mask_str, NULL, 0);
		__u8 nibble = (__u8)strtoul(argv[optind], NULL, 0);

		if (cmis_vdm_set_mask(&ctx, instance, nibble))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- C-CMIS coherent writes --- */

	if (tx_filter_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(tx_filter_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --tx-filter requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_tx_filter_enable(&ctx, bank, lanes,
						       onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (tx_filter_type_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --tx-filter-type requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_tx_filter_type(&ctx, bank, lane,
						     tx_filter_type_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (lf_insertion_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(lf_insertion_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --lf-insertion requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_lf_insertion(&ctx, bank, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (media_mask_str) {
		__u8 offset = (__u8)strtoul(media_mask_str, NULL, 0);
		__u8 mask = (__u8)strtoul(argv[optind], NULL, 0);
		int onoff = parse_on_off(argv[optind + 1]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --media-mask <offset> <mask> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_media_mask(&ctx, bank, offset, mask,
						 onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (host_threshold_str) {
		double value;
		char *endp;

		value = strtod(argv[optind], &endp);
		if (*endp != '\0') {
			fprintf(stderr,
				"Error: invalid threshold value: %s\n",
				argv[optind]);
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_host_threshold(&ctx, bank,
						     host_threshold_str,
						     value))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (host_threshold_enable_str) {
		if (cmis_coherent_host_threshold_enable(&ctx, bank,
							host_threshold_enable_str,
							true))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (host_threshold_disable_str) {
		if (cmis_coherent_host_threshold_enable(&ctx, bank,
							host_threshold_disable_str,
							false))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (host_mask_str) {
		__u8 offset = (__u8)strtoul(host_mask_str, NULL, 0);
		__u8 mask = (__u8)strtoul(argv[optind], NULL, 0);
		int onoff = parse_on_off(argv[optind + 1]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --host-mask <offset> <mask> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (cmis_coherent_set_host_mask(&ctx, bank, offset, mask,
						onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- Network path controls --- */

	if (np_deactivate_mode) {
		if (lane >= 0) {
			if (cmis_np_deinit_lane(&ctx, bank, lane))
				return EXIT_FAILURE;
			printf("NP lane %d: NPDeactivated\n", lane);
		} else {
			int i;

			for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
				if (cmis_np_deinit_lane(&ctx, bank, i))
					return EXIT_FAILURE;
				printf("NP lane %d: NPDeactivated\n", i);
			}
		}
		return EXIT_SUCCESS;
	}

	if (np_activate_mode) {
		if (lane >= 0) {
			if (cmis_np_init_lane(&ctx, bank, lane))
				return EXIT_FAILURE;
			printf("NP lane %d: NPActivated\n", lane);
		} else {
			int i;

			for (i = 0; i < CMIS_CHANNELS_PER_BANK; i++) {
				if (cmis_np_init_lane(&ctx, bank, i))
					return EXIT_FAILURE;
				printf("NP lane %d: NPActivated\n", i);
			}
		}
		return EXIT_SUCCESS;
	}

	if (np_config_mode) {
		struct cmis_np_config cfg = {
			.npid = (npid_val >= 0) ? npid_val : 0,
			.in_use = np_in_use,
		};

		if (lane < 0) {
			fprintf(stderr,
				"Error: --np-config requires --lane\n");
			return EXIT_FAILURE;
		}
		if (npid_val < 0) {
			fprintf(stderr,
				"Error: --np-config requires --npid\n");
			return EXIT_FAILURE;
		}
		if (cmis_np_configure_lane(&ctx, bank, lane, &cfg))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (hp_source_rx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		bool replace;

		if (strcmp(hp_source_rx_str, "np") == 0)
			replace = false;
		else if (strcmp(hp_source_rx_str, "replace") == 0)
			replace = true;
		else {
			fprintf(stderr,
				"Error: --hp-source-rx requires 'np' or 'replace'\n");
			return EXIT_FAILURE;
		}
		if (cmis_np_set_hp_source_rx(&ctx, bank, lanes, replace))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (np_source_tx_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		bool replace;

		if (strcmp(np_source_tx_str, "hp") == 0)
			replace = false;
		else if (strcmp(np_source_tx_str, "replace") == 0)
			replace = true;
		else {
			fprintf(stderr,
				"Error: --np-source-tx requires 'hp' or 'replace'\n");
			return EXIT_FAILURE;
		}
		if (cmis_np_set_np_source_tx(&ctx, bank, lanes, replace))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (np_state_mask_str) {
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(np_state_mask_str);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --np-state-mask requires 'on' or 'off'\n");
			return EXIT_FAILURE;
		}
		if (cmis_np_set_state_mask(&ctx, bank, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* --- Diagnostics writes --- */

	if (diag_mask_str) {
		__u8 offset = (__u8)strtoul(diag_mask_str, NULL, 0);
		__u8 lanes = (lane >= 0) ? (1 << lane) : 0xFF;
		int onoff = parse_on_off(argv[optind]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --diag-mask <offset> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (cmis_diag_set_mask(&ctx, bank, offset, lanes, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (scratchpad_read_mode) {
		__u8 buf[CMIS_DIAG_SCRATCHPAD_SIZE];
		int i;

		if (cmis_diag_scratchpad_read(&ctx, bank, buf))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("scratchpad");
			open_json_array("data", NULL);
			for (i = 0; i < CMIS_DIAG_SCRATCHPAD_SIZE; i++) {
				char hex[8];

				snprintf(hex, sizeof(hex), "0x%02x", buf[i]);
				print_string(PRINT_JSON, NULL, "%s", hex);
			}
			close_json_array(NULL);
			close_json_object();
		} else {
			printf("Scratchpad:");
			for (i = 0; i < CMIS_DIAG_SCRATCHPAD_SIZE; i++)
				printf(" %02x", buf[i]);
			printf("\n");
		}
		return EXIT_SUCCESS;
	}

	if (scratchpad_write_hex) {
		__u8 buf[CMIS_DIAG_SCRATCHPAD_SIZE];
		int len;

		len = parse_hex_string(scratchpad_write_hex, buf,
				       CMIS_DIAG_SCRATCHPAD_SIZE);
		if (len <= 0) {
			fprintf(stderr,
				"Error: invalid hex string\n");
			return EXIT_FAILURE;
		}
		if (cmis_diag_scratchpad_write(&ctx, bank, buf, len))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("scratchpad_write");
			print_uint(PRINT_JSON, "bytes_written", "%u", len);
			close_json_object();
		} else {
			printf("Scratchpad written (%d bytes)\n", len);
		}
		return EXIT_SUCCESS;
	}

	if (user_pattern_hex) {
		__u8 buf[CMIS_DIAG_USER_PATTERN_SIZE];
		int len;

		len = parse_hex_string(user_pattern_hex, buf,
				       CMIS_DIAG_USER_PATTERN_SIZE);
		if (len <= 0) {
			fprintf(stderr,
				"Error: invalid hex string\n");
			return EXIT_FAILURE;
		}
		if (cmis_diag_user_pattern_write(&ctx, bank, buf, len))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("user_pattern_write");
			print_uint(PRINT_JSON, "bytes_written", "%u", len);
			close_json_object();
		} else {
			printf("User pattern written (%d bytes)\n", len);
		}
		return EXIT_SUCCESS;
	}

	/* --- User EEPROM --- */

	if (eeprom_read_str) {
		int offset_val = (int)strtol(eeprom_read_str, NULL, 0);
		int len = (int)strtol(argv[optind], NULL, 0);
		__u8 buf[CMIS_USER_EEPROM_SIZE];
		int i;

		if (len < 1 || len > CMIS_USER_EEPROM_SIZE ||
		    offset_val < 0 || offset_val + len > CMIS_USER_EEPROM_SIZE) {
			fprintf(stderr,
				"Error: invalid offset/len for EEPROM read\n");
			return EXIT_FAILURE;
		}
		if (cmis_user_eeprom_read(&ctx, offset_val, buf, len))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("eeprom_read");
			print_uint(PRINT_JSON, "offset", "%u", offset_val);
			print_uint(PRINT_JSON, "length", "%u", len);
			open_json_array("data", NULL);
			for (i = 0; i < len; i++) {
				char hex[8];

				snprintf(hex, sizeof(hex), "0x%02x", buf[i]);
				print_string(PRINT_JSON, NULL, "%s", hex);
			}
			close_json_array(NULL);
			close_json_object();
		} else {
			printf("EEPROM[0x%02x..0x%02x]:",
			       offset_val, offset_val + len - 1);
			for (i = 0; i < len; i++) {
				if (i % 16 == 0 && len > 16)
					printf("\n  %02x:", offset_val + i);
				printf(" %02x", buf[i]);
			}
			printf("\n");
		}
		return EXIT_SUCCESS;
	}

	if (eeprom_write_str) {
		int offset_val = (int)strtol(eeprom_write_str, NULL, 0);
		__u8 buf[CMIS_USER_EEPROM_MAX_WRITE];
		int len;

		len = parse_hex_string(argv[optind], buf,
				       CMIS_USER_EEPROM_MAX_WRITE);
		if (len <= 0) {
			fprintf(stderr,
				"Error: invalid hex data\n");
			return EXIT_FAILURE;
		}
		if (cmis_user_eeprom_write(&ctx, offset_val, buf, len))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("eeprom_write");
			print_uint(PRINT_JSON, "offset", "%u", offset_val);
			print_uint(PRINT_JSON, "bytes_written", "%u", len);
			close_json_object();
		} else {
			printf("EEPROM written: %d bytes at offset %d\n",
			       len, offset_val);
		}
		return EXIT_SUCCESS;
	}

	/* --- Host lane switching --- */

	if (lane_redir_val >= 0) {
		if (lane < 0) {
			fprintf(stderr,
				"Error: --lane-redir requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_lane_switch_set_redir(&ctx, bank, lane,
					       lane_redir_val))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("write_result");
			print_string(PRINT_JSON, "register", "%s",
				     "LaneSwitchRedir");
			print_uint(PRINT_JSON, "lane", "%u", lane);
			print_uint(PRINT_JSON, "target", "%u",
				   lane_redir_val);
			close_json_object();
		} else {
			printf("Lane %d redirected to lane %d\n",
			       lane, lane_redir_val);
		}
		return EXIT_SUCCESS;
	}

	if (lane_switch_enable_mode) {
		if (cmis_lane_switch_enable(&ctx, bank, true))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (lane_switch_disable_mode) {
		if (cmis_lane_switch_enable(&ctx, bank, false))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (lane_switch_commit_mode) {
		if (cmis_lane_switch_commit(&ctx, bank))
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("write_result");
			print_string(PRINT_JSON, "register", "%s",
				     "LaneSwitchCommit");
			print_string(PRINT_JSON, "result", "%s",
				     "committed");
			close_json_object();
		} else {
			printf("Lane switching committed\n");
		}
		return EXIT_SUCCESS;
	}

	if (lane_switch_result_mode) {
		int result = cmis_lane_switch_result(&ctx, bank);

		if (result < 0)
			return EXIT_FAILURE;
		if (is_json_context()) {
			open_json_object("lane_switch_result");
			print_uint(PRINT_JSON, "result", "0x%02x", result);
			close_json_object();
		} else {
			printf("Lane switching result: 0x%02x\n", result);
		}
		return EXIT_SUCCESS;
	}

	/* --- Tunable mask --- */

	if (tuning_mask_str) {
		__u8 bits = (__u8)strtoul(tuning_mask_str, NULL, 0);
		int onoff = parse_on_off(argv[optind]);

		if (onoff < 0) {
			fprintf(stderr,
				"Error: --tuning-mask <bits> <on|off>\n");
			return EXIT_FAILURE;
		}
		if (lane < 0) {
			fprintf(stderr,
				"Error: --tuning-mask requires --lane\n");
			return EXIT_FAILURE;
		}
		if (cmis_tunable_set_mask(&ctx, bank, lane, bits, onoff))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (eye_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_eye(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (vdm_mode) {
		new_json_obj(ctx.json);
		open_json_object(NULL);
		if (show_vdm(&ctx)) {
			close_json_object();
			delete_json_obj();
			return EXIT_FAILURE;
		}
		close_json_object();
		delete_json_obj();
		return EXIT_SUCCESS;
	}

	if (fw_update_file) {
		if (cmis_cdb_fw_download(&ctx, fw_update_file))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (fw_run_mode) {
		if (cmis_cdb_fw_run(&ctx, fw_run_val))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (fw_commit_mode) {
		if (cmis_cdb_fw_commit(&ctx))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (fw_copy_dir) {
		__u8 direction;

		if (strcmp(fw_copy_dir, "AB") == 0 ||
		    strcmp(fw_copy_dir, "ab") == 0)
			direction = 0xAB;
		else if (strcmp(fw_copy_dir, "BA") == 0 ||
			 strcmp(fw_copy_dir, "ba") == 0)
			direction = 0xBA;
		else {
			fprintf(stderr,
				"Invalid copy direction: %s (use AB or BA)\n",
				fw_copy_dir);
			return EXIT_FAILURE;
		}
		if (cmis_cdb_fw_copy(&ctx, direction))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (fw_abort_mode) {
		if (cmis_cdb_fw_abort(&ctx))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (cert_mode || cert_file) {
		if (cert_file) {
			__u8 cert_buf[4096];
			int cert_len = 0;
			FILE *fp;

			if (cmis_cdb_get_certificate(&ctx, 0, cert_buf,
						     sizeof(cert_buf),
						     &cert_len)) {
				fprintf(stderr,
					"Failed to retrieve leaf certificate\n");
				return EXIT_FAILURE;
			}

			fp = fopen(cert_file, "wb");
			if (!fp) {
				fprintf(stderr, "Cannot open %s for writing\n",
					cert_file);
				return EXIT_FAILURE;
			}
			fwrite(cert_buf, 1, cert_len, fp);
			fclose(fp);
			printf("Saved leaf certificate (%d bytes) to %s\n",
			       cert_len, cert_file);
		} else {
			cmis_cdb_show_certificates(&ctx);
		}
		return EXIT_SUCCESS;
	}

	if (auth_digest_hex) {
		__u8 digest[64];
		__u8 sig_buf[120];
		int dig_len = 0, sig_len = 0;
		const char *p = auth_digest_hex;
		unsigned int byte;
		int i;

		/* Parse hex string to bytes */
		while (*p && dig_len < (int)sizeof(digest)) {
			if (sscanf(p, "%2x", &byte) != 1)
				break;
			digest[dig_len++] = byte;
			p += 2;
		}
		if (dig_len == 0) {
			fprintf(stderr, "Invalid digest hex string\n");
			return EXIT_FAILURE;
		}

		printf("Setting digest (%d bytes)...\n", dig_len);
		if (cmis_cdb_set_digest(&ctx, digest, dig_len))
			return EXIT_FAILURE;

		printf("Getting signature...\n");
		if (cmis_cdb_get_signature(&ctx, sig_buf, sizeof(sig_buf),
					   &sig_len))
			return EXIT_FAILURE;

		printf("Signature (%d bytes):", sig_len);
		for (i = 0; i < sig_len; i++) {
			if (i % 16 == 0)
				printf("\n  ");
			printf(" %02x", sig_buf[i]);
		}
		printf("\n");
		return EXIT_SUCCESS;
	}

	if (map_mode) {
		show_wavelength_map(&ctx);
		return EXIT_SUCCESS;
	}

	if (tune_mode) {
		struct cmis_tunable_config cfg = {
			.grid_spacing    = grid_enc,
			.channel_number  = channel,
			.fine_tuning_en  = (fine_offset != 0),
			.fine_offset     = fine_offset,
			.target_power    = target_power,
		};
		int ret;

		if (lane >= 0)
			ret = cmis_tunable_set_lane(&ctx, bank, lane, &cfg);
		else
			ret = cmis_tunable_set_bank(&ctx, bank, &cfg);

		if (ret) {
			fprintf(stderr, "Tunable laser configuration failed: %d\n", ret);
			return EXIT_FAILURE;
		}
		printf("Tunable laser configuration applied successfully\n");
	}

	int ret = eeprom_parse(&ctx);
	if (ret)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
