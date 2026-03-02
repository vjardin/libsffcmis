#!/bin/bash
# t02_cdr — Test CDR on/off toggle
source "$(dirname "$0")/../lib/common.sh"

# Disable CDR
sff_cmd --cdr off -l "$LANE"
assert_rc 0 "cdr off command"

sff_json -D
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].cdr_tx" "false" \
  "cdr_tx disabled"
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].cdr_rx" "false" \
  "cdr_rx disabled"

# Enable CDR
sff_cmd --cdr on -l "$LANE"
assert_rc 0 "cdr on command"

sff_json -D
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].cdr_tx" "true" \
  "cdr_tx enabled"
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].cdr_rx" "true" \
  "cdr_rx enabled"

pass
