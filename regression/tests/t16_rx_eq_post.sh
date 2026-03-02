#!/bin/bash
# t16_rx_eq_post — Test Rx output EQ post-cursor target
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --rx-eq-post 3 -l "$LANE"
assert_rc 0 "rx-eq-post set 3"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_eq_post")
assert_eq "$val" "3" "rx_eq_post is 3"

sff_cmd --rx-eq-post 0 -l "$LANE"
assert_rc 0 "rx-eq-post set 0"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].rx_eq_post")
assert_eq "$val" "0" "rx_eq_post is 0"

pass
