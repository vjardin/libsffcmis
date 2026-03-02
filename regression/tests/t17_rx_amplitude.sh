#!/bin/bash
# t17_rx_amplitude — Test Rx output amplitude target
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --rx-amplitude 3 -l "$LANE"
assert_rc 0 "rx-amplitude set 3"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_amplitude")
assert_eq "$val" "3" "rx_amplitude is 3"

sff_cmd --rx-amplitude 0 -l "$LANE"
assert_rc 0 "rx-amplitude set 0"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_amplitude")
assert_eq "$val" "0" "rx_amplitude is 0"

pass
