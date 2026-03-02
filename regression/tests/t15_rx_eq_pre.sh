#!/bin/bash
# t15_rx_eq_pre — Test Rx output EQ pre-cursor target
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --rx-eq-pre 3 -l "$LANE"
assert_rc 0 "rx-eq-pre set 3"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_eq_pre")
assert_eq "$val" "3" "rx_eq_pre is 3"

sff_cmd --rx-eq-pre 0 -l "$LANE"
assert_rc 0 "rx-eq-pre set 0"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_eq_pre")
assert_eq "$val" "0" "rx_eq_pre is 0"

pass
