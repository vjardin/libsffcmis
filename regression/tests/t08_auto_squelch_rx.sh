#!/bin/bash
# t08_auto_squelch_rx — Test Rx auto-squelch on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --auto-squelch-rx off -l "$LANE"
assert_rc 0 "auto-squelch-rx off"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].auto_squelch_dis_rx")
assert_eq "$val" "true" "auto_squelch_dis_rx set (squelch off)"

sff_cmd --auto-squelch-rx on -l "$LANE"
assert_rc 0 "auto-squelch-rx on"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].auto_squelch_dis_rx")
assert_eq "$val" "false" "auto_squelch_dis_rx cleared (squelch on)"

pass
