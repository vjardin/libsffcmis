# libsffcmis

libsffcmis is a C library and CLI tool for parsing and configuring optical transceiver
modules over I2C. It supports **SFP/SFP+** (SFF-8079/8472), **XFP** (INF-8077i),
**QSFP/QSFP28** (SFF-8636), and **CMIS** (OIF-CMIS-05.3) modules, including
**C-CMIS** (OIF-C-CMIS-01.3) coherent optics extensions.

Operates directly on `/dev/i2c-N` devices without requiring kernel ethtool/netlink drivers.
Licensed GPLv2.

## Hardware Recommendation

A CH341-based USB-to-I2C adapter is recommended for development and testing.
The library auto-detects CH341 adapters and adjusts chunk size (32 bytes) and timing.

Example: [CH341 USB I2C Adapter on AliExpress](https://fr.aliexpress.com/item/1005006659842448.html)

## Design

This library was initially bootstrapped from the `ethtool` codebase. The ethtool-origin
files (`cmis.c`, `cmis.h`, `qsfp.c`, `qsfp.h`, `sff-common.c`, `sff-common.h`, `sfpid.c`,
`sfpdiag.c`, `json_writer.c/h`, `json_print.c/h`) are kept as close to pristine as possible.
All libsffcmis additions live in dedicated extension files (`cmis-ext.c`, `cmis-internal.h`,
`sff-common-ext.c/h`, `qsfp-ext.h`, `xfp.c/h`, `cmis-*.c/h`).

### Architecture

```
sffcmis.c (public API: i2c_init, eeprom_parse, get_eeprom_page)
  |
  +-- sfpid.c          SFF-8079/8472 parser (SFP/SFP+)
  +-- qsfp.c           SFF-8636/8436 parser (QSFP/QSFP28)
  |   +-- qsfp-ext.h   Extended compliance codes, checksums
  +-- xfp.c            INF-8077i parser (XFP)
  +-- cmis.c           CMIS base parser (ethtool-origin, minimal changes)
      +-- cmis-ext.c   Extension orchestrator: module type, app descriptors,
      |                 DP state, module chars, extended caps, entry point
      +-- cmis-internal.h   All CMIS extension defines and page structures
      +-- cmis-tunable.c    Tunable laser capabilities (Page 04h/12h)
      +-- cmis-coherent.c   C-CMIS PM (35h/34h), FEC, flag decoding (33h/3Bh)
      +-- cmis-vdm.c        VDM monitors, thresholds, freeze (Pages 20h-2Fh)
      +-- cmis-datapath.c   Data path control/status (Pages 10h/11h)
      +-- cmis-netpath.c    Network path control/status (Pages 16h/17h)
      +-- cmis-diag.c       Diagnostics: PRBS, loopback, BER (Pages 13h/14h)
      +-- cmis-cdb.c        CDB messaging, FW management (Page 9Fh)
      +-- cmis-eye.c        Synthetic PAM4 eye diagram and IQ constellation

Shared modules:
  module-common.c      Shared parsing utilities (all standards)
  sff-common.c         SFF-8024 encoding, unit conversions (mW <-> dBm)
  sff-common-ext.c     SFF-8024 Rev 4.13 lookup tables (connector, compliance, etc.)
  sfpdiag.c            Diagnostic data parsing (temp, voltage, power, bias)
  i2c.c                Linux I2C device layer, CH341 auto-detection
  json_writer.c        JSON output engine (from iproute2)
  json_print.c         Dual output: plain text or JSON
```

## Build

```sh
make                    # Build libsffcmis.so + sffcmis_test
make clean              # Remove build artifacts
sudo make install       # Install to /usr/lib and /usr/bin
```

Cross-compilation: override `CC`, `LD`, `AR`, `STRIP` variables.

Dependencies: only libc, libm, and Linux kernel I2C headers.

## CLI Usage

```sh
sudo sffcmis_test [OPTIONS] <i2c_bus_number>
```

### Basic module reading

```sh
# Full module dump (auto-detects SFP, XFP, QSFP, or CMIS)
sudo sffcmis_test 10

# JSON output
sudo sffcmis_test -j 10

# Wake module from low-power mode first
sudo sffcmis_test -w 10

# Include statistics in output
sudo sffcmis_test -I 10

# Set debug level
sudo sffcmis_test -d 2 10
```

### CMIS Coherent Optics

```sh
# C-CMIS coherent PM (Page 35h) and FEC counters (Page 34h)
sudo sffcmis_test -K 10

# VDM real-time monitors (Pages 20h-2Fh)
sudo sffcmis_test -V 10

# Data path control/status (Pages 10h/11h)
sudo sffcmis_test -D 10

# Network path control/status (Pages 16h/17h)
sudo sffcmis_test -N 10

# Diagnostics capabilities (Pages 13h-14h)
sudo sffcmis_test --diag 10

# CDB supported commands and firmware info
sudo sffcmis_test -C 10

# CDB Performance Monitoring
sudo sffcmis_test -P 10

# Tunable wavelength map
sudo sffcmis_test -m 10
```

### Synthetic Eye Diagram and IQ Constellation

For coherent CMIS modules with VDM data, the tool can render synthetic visualizations:

```sh
# PAM4 eye diagram + IQ constellation (from VDM data)
sudo sffcmis_test -E 10
```

**PAM4 eye diagram**: Rendered from amplitude levels (VDM Type IDs 16-19, in mV) and
SNR measurements (Type IDs 29-34, in dB). Shows three PAM4 eyes with computed eye
heights per lane. Vertical structure is faithful to VDM data; horizontal transitions
are modeled with a raised cosine (no jitter data available). Uses a 6-level density
palette (` .░▒▓█`) with log-scale normalization.

Example output (40x70 grid, one lane):

```
Lane 0  SNR: 22.3 dB  Levels: -150/-50/50/150 mV
 +195 |                                                                      |
 +185 |....                                                              ....|
 +175 |▒▒▒▒░░░.......................░░░░░░░░░░.......................░░░▒▒▒▒|
 +165 |▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓|
 +155 |██████████████████████████████████████████████████████████████████████|
  L3  |██████████████████████████████████████████████████████████████████████|
 +135 |▓▓▓████████▓▓▓▓▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒▓▓▓▓████████▓▓▓|
 +125 |▒▒▒▓▓▓███████▓▓▓▒▒▒▓▓▓▓▓▓▓▓▓▓▓▓▒▒░░░░▒▒▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▓▓▓███████▓▓▓▒▒▒|
 +115 |..░░▒▓▓████▓▓███▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒░.    .░▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓███▓▓████▓▓▒░░..|
 +105 |    .░▒▓████▓▓▓████▓▓▓▓▓▓▓▓▓▒░.        .░▒▓▓▓▓▓▓▓▓▓████▓▓▓████▓▒░.    |
  +95 |      .▒▓▓█▓█▓▓████▓▓▓▓▓▓▓▓▒░            ░▒▓▓▓▓▓▓▓▓████▓▓█▓█▓▓▒.      |
  +85 |....░░░▒▒▓▓█████▓▓▓▓██▓▓▓▓▒▒░░..      ..░░▒▒▓▓▓▓██▓▓▓▓█████▓▓▒▒░░░....|
  +75 |▒▒▒▒▒▒▓▓▓▓██████▓▒▓▓█▓▓█▓▓▓▓▒▒▒░░░░░░░░▒▒▒▓▓▓▓█▓▓█▓▓▒▓██████▓▓▓▓▒▒▒▒▒▒|
  +65 |▓▓▓▓▓▓▓████▓█▓▓▓█▓▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▓█▓▓▓█▓████▓▓▓▓▓▓▓|
  +55 |██████████████████████████████████████████████████████████████████████|
  L2  |██████████████████████████████████████████████████████████████████████|
  +35 |▓▓▓▓███████▓▓▓▓██▓▓██▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓██▓▓██▓▓▓▓███████▓▓▓▓|
  +25 |▒▒▒▒▓▓▓█████████▓▒▓██▓▓▓▓▓▓▓▓▓▒▒░░░░░░▒▒▓▓▓▓▓▓▓▓▓██▓▒▓█████████▓▓▓▒▒▒▒|
  +15 |...░░▒▒▓▓██▓▓████▓▓▓██▓▓▓▓▓▓▒▒░..    ..░▒▒▓▓▓▓▓▓██▓▓▓████▓▓██▓▓▒▒░░...|
   +5 |     .░▒▓▓███▓▓████▓▓▓▓▓▓▓▒▒░.          .░▒▒▓▓▓▓▓▓▓████▓▓███▓▓▒░.     |
   -5 |     .░▒▓▓███▓▓████▓▓▓▓▓▓▓▒▒░.          .░▒▒▓▓▓▓▓▓▓████▓▓███▓▓▒░.     |
  -15 |...░░▒▒▓▓██▓▓████▓▓▓██▓▓▓▓▓▓▒▒░..    ..░▒▒▓▓▓▓▓▓██▓▓▓████▓▓██▓▓▒▒░░...|
  -25 |▒▒▒▒▓▓▓█████████▓▒▓██▓▓▓▓▓▓▓▓▓▒▒░░░░░░▒▒▓▓▓▓▓▓▓▓▓██▓▒▓█████████▓▓▓▒▒▒▒|
  -35 |▓▓▓▓███████▓▓▓▓██▓▓██▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓██▓▓██▓▓▓▓███████▓▓▓▓|
  -45 |██████████████████████████████████████████████████████████████████████|
  L1  |██████████████████████████████████████████████████████████████████████|
  -65 |▓▓▓▓▓▓▓████▓█▓▓▓█▓▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▓█▓▓▓█▓████▓▓▓▓▓▓▓|
  -75 |▒▒▒▒▒▒▓▓▓▓██████▓▒▓▓█▓▓█▓▓▓▓▒▒▒░░░░░░░░▒▒▒▓▓▓▓█▓▓█▓▓▒▓██████▓▓▓▓▒▒▒▒▒▒|
  -85 |....░░░▒▒▓▓█████▓▓▓▓██▓▓▓▓▒▒░░..      ..░░▒▒▓▓▓▓██▓▓▓▓█████▓▓▒▒░░░....|
  -95 |      .▒▓▓█▓█▓▓████▓▓▓▓▓▓▓▓▒░            ░▒▓▓▓▓▓▓▓▓████▓▓█▓█▓▓▒.      |
 -105 |    .░▒▓████▓▓▓████▓▓▓▓▓▓▓▓▓▒░.        .░▒▓▓▓▓▓▓▓▓▓████▓▓▓████▓▒░.    |
 -115 |..░░▒▓▓████▓▓███▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒░.    .░▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓███▓▓████▓▓▒░░..|
 -125 |▒▒▒▓▓▓███████▓▓▓▒▒▒▓▓▓▓▓▓▓▓▓▓▓▓▒▒░░░░▒▒▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▓▓▓███████▓▓▓▒▒▒|
 -135 |▓▓▓████████▓▓▓▓▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒▓▓▓▓████████▓▓▓|
 -145 |██████████████████████████████████████████████████████████████████████|
  L0  |██████████████████████████████████████████████████████████████████████|
 -165 |▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓|
 -175 |▒▒▒▒░░░.......................░░░░░░░░░░.......................░░░▒▒▒▒|
 -185 |....                                                              ....|
 -195 |                                                                      |
      +----------------------------------------------------------------------+
       0                                                      0.5 UI  1
  Eye heights:  Eye0=77.0  Eye1=77.0  Eye2=77.0 mV
  NOTE: Vertical from VDM amplitude/SNR. Horizontal modeled (no jitter data).
```

**IQ constellation**: Rendered from coherent EVM/eSNR (C-CMIS Type IDs 140-142).
Modulation format is auto-detected from the active application descriptor
(DP-QPSK, DP-8QAM, DP-16QAM, DP-64QAM). Uses the same density palette with
log-scale normalization on a 41x41 grid.

Example output (DP-16QAM, one lane):

```
Lane 0  DP-16QAM  EVM: 8.2%  eSNR: 21.7 dB
  +1  | .░░░░░░░...░░░▒░░░...░░░▒░░░...░░░░░░░. |
      |.░░▒▒▒▒▒▒░░░▒▒▒▒▒▒▒░░░▒▒▒▒▒▒▒░░░▒▒▒▒▒▒░░.|
      |░░▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒░░|
      |░▒▓▓▓▓▓▓▓▒▒▒▓▓▓█▓▓▓▒▒▒▓▓▓█▓▓▓▒▒▒▓▓▓▓▓▓▓▒░|
      |░▒▓▓████▓▓▒▒▓▓████▓▓▒▓▓████▓▓▒▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▒▓▓███▓▓▓▒▓▓▓███▓▓▒▒▓▓████▓▓▒░|
      |░▒▒▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▒▒░|
      |.░▒▒▓▓▓▓▒▒░░▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒░░▒▒▓▓▓▓▒▒░.|
      |.░░▒▒▒▒▒▒░.░░▒▒▒▒▒▒░.░▒▒▒▒▒▒░░.░▒▒▒▒▒▒░░.|
      |.░▒▒▓▓▒▒░░░▒▒▒▓▒▒▒░░░▒▒▒▓▒▒▒░░░▒▒▓▓▒▒▒░.|
      |░▒▒▓▓▓▓▓▓▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▓▓▓▓▓▓▒▒░|
      |░▒▓▓███▓▓▓▒▒▓▓███▓▓▒▒▒▓▓███▓▓▒▒▓▓▓██▓▓▓▒░|
      |░▒▓▓████▓▓▒▒▓█████▓▓▒▓▓█████▓▒▒▓▓████▓▓▒░|
      |▒▒▓█████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓█████▓▒▒|
      |░▒▓▓████▓▓▒▒▓█████▓▓▒▓▓█████▓▒▒▓▓████▓▓▒░|
      |░▒▓▓███▓▓▓▒▒▓▓███▓▓▒▒▒▓▓███▓▓▒▒▓▓▓███▓▓▒░|
      |░▒▒▓▓▓▓▓▓▒▒▒▒▓▓▓▓▓▓▒░▒▓▓▓▓▓▓▒▒▒▒▓▓▓▓▓▓▒▒░|
      |.░▒▒▓▓▓▓▒▒░░▒▒▓▓▓▒▒░░░▒▒▓▓▓▒▒░░▒▒▓▓▓▓▒▒░.|
   Q  |.░░▒▒▒▒▒▒░.░░▒▒▒▒▒░░.░░▒▒▒▒▒░░.░▒▒▒▒▒▒░░.|
      |.░▒▒▓▓▓▓▒▒░░▒▒▓▓▓▒▒░░░▒▒▓▓▓▒▒░░▒▒▓▓▓▓▒▒░.|
      |░▒▒▓▓▓▓▓▓▒▒▒▒▓▓▓▓▓▓▒░▒▓▓▓▓▓▓▒▒▒▒▓▓▓▓▓▓▒▒░|
      |░▒▓▓███▓▓▓▒▒▓▓███▓▓▒▒▒▓▓███▓▓▒▒▓▓▓███▓▓▒░|
      |░▒▓▓████▓▓▒▒▓█████▓▓▒▓▓█████▓▒▒▓▓████▓▓▒░|
      |▒▒▓█████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓█████▓▒▒|
      |░▒▓▓████▓▓▒▒▓█████▓▓▒▓▓█████▓▒▒▓▓████▓▓▒░|
      |░▒▓▓▓██▓▓▓▒▒▓▓███▓▓▒▒▒▓▓███▓▓▒▒▓▓▓██▓▓▓▒░|
      |░▒▒▓▓▓▓▓▓▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▓▓▓▓▓▓▒▒░|
      |.░▒▒▒▓▓▒▒░░░▒▒▒▓▒▒▒░░░▒▒▒▓▒▒▒░░░▒▒▓▓▒▒▒░.|
      |.░░▒▒▒▒▒▒░.░░▒▒▒▒▒▒░.░▒▒▒▒▒▒░░.░▒▒▒▒▒▒░░.|
      |.░▒▒▓▓▓▓▒▒░░▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒░░▒▒▓▓▓▓▒▒░.|
      |░▒▒▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▓▒▒▒▓▓▓▓▓▓▒▒░|
      |░▒▓▓████▓▓▒▒▓▓███▓▓▓▒▓▓▓███▓▓▒▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▓▓█████▓▓▒▓▓█████▓▓▒▓▓████▓▓▒░|
      |░▒▓▓████▓▓▒▒▓▓████▓▓▒▓▓████▓▓▒▒▓▓████▓▓▒░|
      |░▒▓▓▓▓▓▓▓▒▒▒▓▓▓█▓▓▓▒▒▒▓▓▓█▓▓▓▒▒▒▓▓▓▓▓▓▓▒░|
      |░░▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒▒░▒▒▓▓▓▓▓▒░░|
      |.░░▒▒▒▒▒▒░░░▒▒▒▒▒▒▒░░░▒▒▒▒▒▒▒░░░▒▒▒▒▒▒░░.|
  -1  | .░░░░░░░...░░░▒░░░...░░░▒░░░...░░░░░░░. |
      +-----------------------------------------+
      -1                  I                   +1
  NOTE: Synthetic from EVM/eSNR. No raw IQ samples available.
```

Both render as ASCII art in text mode, or structured data in JSON mode (`-j -E`).

### Tunable Laser Control

```sh
# Configure laser: 50 GHz grid, channel 0, lane 0
sudo sffcmis_test -t -g 50 -c 0 -l 0 10

# With fine-tuning offset (+1.5 GHz) and target power (-10.00 dBm)
sudo sffcmis_test -t -g 50 -c 0 -l 0 -f 1500 -p -1000 10
```

### Write Operations

The CLI supports a full set of write operations for configurable CMIS features:

```sh
# Data path control
sudo sffcmis_test --tx-disable -l 0 10          # Disable Tx lane 0
sudo sffcmis_test --tx-enable -l 0 10           # Enable Tx lane 0
sudo sffcmis_test --dp-deactivate -l 0 10       # Deactivate data path
sudo sffcmis_test --dp-activate -l 0 10         # Activate data path
sudo sffcmis_test --dp-config -l 0 --appsel 1 10  # Configure DP lane

# Signal integrity (Page 10h)
sudo sffcmis_test --polarity-tx flip -l 0 10    # Flip Tx polarity
sudo sffcmis_test --rx-eq-pre 5 -l 0 10         # Set Rx EQ pre-cursor
sudo sffcmis_test --rx-amplitude 8 -l 0 10      # Set Rx amplitude

# Module global controls
sudo sffcmis_test --sw-reset 10                 # Software reset
sudo sffcmis_test --bank-broadcast on 10        # Enable bank broadcast

# VDM controls
sudo sffcmis_test --vdm-freeze 10               # Freeze VDM samples
sudo sffcmis_test --vdm-unfreeze 10             # Unfreeze VDM samples

# C-CMIS coherent thresholds
sudo sffcmis_test --set-threshold total-pwr-hi-alarm 5.0 10
sudo sffcmis_test --threshold-enable total-pwr 10

# PRBS/BER testing
sudo sffcmis_test --bert-start PRBS-31Q 10      # Start PRBS test
sudo sffcmis_test --bert-read 10                 # Read BER results
sudo sffcmis_test --bert-stop 10                 # Stop PRBS test

# Loopback testing
sudo sffcmis_test --loopback-start host-input 10
sudo sffcmis_test --loopback-stop 10

# Firmware management
sudo sffcmis_test --fw-update firmware.bin 10    # Download firmware
sudo sffcmis_test --fw-run 0 10                 # Run image (reset-inactive)
sudo sffcmis_test --fw-commit 10                # Commit running image

# IDevID / Security
sudo sffcmis_test --cert 10                     # Display certificates
sudo sffcmis_test --cert-file leaf.der 10       # Save leaf cert to file

# User EEPROM (Page 03h)
sudo sffcmis_test --eeprom-read 0 16 10         # Read 16 bytes at offset 0
sudo sffcmis_test --eeprom-write 0 48656C6C6F 10  # Write hex data
```

See `sffcmis_test --help` for the complete list of options.

### Environment Variable

The `LIBSFFCMIS_ARG` environment variable can pass options to the library when used
programmatically:

```sh
export LIBSFFCMIS_ARG="--debug 2 --busnum 10 --json"
```

## Developer Integration

```c
#include <sffcmis.h>

struct cmd_context ctx = {
    .bus_num = 10,    // /dev/i2c-10
};
i2c_init(&ctx);
int ret = eeprom_parse(&ctx);

char desc[256];
printf("%s\n", i2c_get_device_desc(ctx.device, desc, sizeof(desc)));
```

```sh
gcc -o demoapp demoapp.c -lsffcmis -lm
```

## Regression Test Suite

A bash-based regression test suite verifies write operations and JSON output against
real hardware. Tests use a module-specific JSON configuration that declares expected
results per test (pass, xfail, skip) with reasons for expected failures.

### Prerequisites

- `jq` and `bc` must be installed
- A connected CMIS module on an I2C bus
- Root access (for `/dev/i2c-N`)

### Running Tests

```sh
cd regression

# Run all 54 tests against a Finisar FTLC3355
sudo ./run_all.sh modules/FTLC3355.json 0

# Verbose mode (show details on failure)
sudo ./run_all.sh modules/FTLC3355.json 0 -v

# Run a single test
sudo ./run_all.sh modules/FTLC3355.json 0 -f t01_tx_disable

# Keep going after failure
sudo ./run_all.sh modules/FTLC3355.json 0 -k
```

### Test Structure

```
regression/
  run_all.sh              Main test runner
  lib/common.sh           Shared helpers (sff_cmd, sff_json, assertions)
  modules/FTLC3355.json   Module config (expected results per test)
  tests/t01-t54.sh        Individual test scripts
```

Each test script uses helpers from `lib/common.sh`:
- `sff_cmd <args>` -- run sffcmis_test, capture stdout/stderr/rc
- `sff_json <args>` -- run with `-j`, parse JSON with `jq_field`
- `assert_rc`, `assert_eq`, `assert_json` -- assertion helpers
- `eeprom_delay` -- configurable settling delay for write operations

### Module Configuration

Create a JSON file for your module in `regression/modules/`:

```json
{
  "name": "MY_MODULE",
  "description": "Vendor 400G QSFP-DD",
  "wake_needed": true,
  "default_bank": 0,
  "default_lane": 0,
  "eeprom_write_delay_ms": 500,
  "tests": {
    "t01_tx_disable": { "expect": "pass" },
    "t02_cdr":        { "expect": "xfail", "reason": "module does not support CDR bypass" },
    "t10_vdm_freeze": { "expect": "skip" }
  }
}
```

Expected results: `pass` (must succeed), `xfail` (expected failure with reason),
`skip` (not run). Unexpected results (FAIL or XPASS) cause a non-zero exit code.

## Supported Standards

| Standard             | Form Factor                  | Module                 |
|----------------------|------------------------------|------------------------|
| SFF-8079 / SFF-8472  | SFP, SFP+                    | `sfpid.c`              |
| SFF-8636 / SFF-8436  | QSFP, QSFP28                 | `qsfp.c`               |
| INF-8077i Rev 4.5    | XFP                          | `xfp.c`                |
| SFF-8024 Rev 4.13    | (identifier tables)          | `sff-common-ext.c`     |
| OIF-CMIS-05.3        | QSFP-DD, OSFP, SFP-DD, etc. | `cmis.c` + `cmis-ext.c`|
| OIF-C-CMIS-01.3      | Coherent (ZR, ZR+, etc.)     | `cmis-coherent.c`      |

## Features

### Read-Only Parsing (all module types)
- Module identification (identifier, connector, vendor, PN, SN, date code)
- Transceiver compliance codes
- Wavelength, bit rate, link lengths
- Diagnostic monitoring (temperature, voltage, bias current, optical power)
- Alarm and warning flags with thresholds

### CMIS Extended Features
- Application descriptors and media interface technology
- Data path state machine status
- Module characteristics and extended capabilities
- Tunable laser capabilities (Page 04h) and status (Page 12h)
- C-CMIS coherent performance monitoring (Pages 34h/35h)
- C-CMIS flag decoding (Pages 33h/3Bh) with coherent auto-detection quirk
- VDM real-time monitors with thresholds (Pages 20h-2Fh)
- Data path control/status (Pages 10h/11h)
- Network path control/status (Pages 16h/17h)
- Diagnostics: PRBS, loopback, BER capabilities (Pages 13h/14h)
- CDB messaging: firmware info, feature/security advertisement (Page 9Fh)
- **Synthetic PAM4 eye diagram** from VDM amplitude/SNR data
- **IQ constellation diagram** from coherent EVM/eSNR with modulation auto-detection

### Write Operations (CMIS)
- Tx/Rx disable and data path activate/deactivate
- Signal integrity: polarity, squelch, EQ freeze/target, amplitude
- Module controls: software reset, bank broadcast, squelch method, passwords
- C-CMIS coherent thresholds and masks (media + host)
- VDM freeze/unfreeze, power saving, alarm masks
- Network path: init/deinit, configure, signal source
- Diagnostics masks, scratchpad, user pattern
- User EEPROM read/write (Page 03h)
- Host lane switching
- Tunable laser: grid/channel/fine-tuning configuration with DP state management
- Firmware download, run, commit, copy, abort
- IDevID certificate retrieval and challenge-response authentication
- PRBS/BER test start/stop/read and loopback start/stop

### Output Formats
- Plain text (default) with aligned field formatting
- JSON (`-j` flag) for programmatic consumption
- ASCII art eye diagram and constellation in text mode
- Structured eye/constellation data in JSON mode
