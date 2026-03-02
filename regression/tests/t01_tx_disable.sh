#!/bin/bash
# t01_tx_disable — Test Tx disable/enable toggle
source "$(dirname "$0")/../lib/common.sh"

# Disable Tx
sff_cmd --tx-disable -l "$LANE"
assert_rc 0 "tx-disable command"

# Verify via JSON
sff_json -D
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].tx_disable" "true" \
  "tx_disable set"

# Re-enable Tx
sff_cmd --tx-enable -l "$LANE"
assert_rc 0 "tx-enable command"

# Verify restored
sff_json -D
assert_json ".[0].datapath_status.datapath_control_bank_0[$LANE].tx_disable" "false" \
  "tx_disable cleared"

pass
