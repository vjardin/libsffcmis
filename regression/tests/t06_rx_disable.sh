#!/bin/bash
# t06_rx_disable — Test Rx output disable/enable toggle
source "$(dirname "$0")/../lib/common.sh"

# Disable
sff_cmd --rx-disable -l "$LANE"
assert_rc 0 "rx-disable command"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_disable")
assert_eq "$val" "true" "rx_disable set"

# Re-enable
sff_cmd --rx-enable -l "$LANE"
assert_rc 0 "rx-enable command"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_disable")
assert_eq "$val" "false" "rx_disable cleared"

pass
