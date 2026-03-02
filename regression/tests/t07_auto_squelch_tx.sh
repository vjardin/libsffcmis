#!/bin/bash
# t07_auto_squelch_tx — Test Tx auto-squelch on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --auto-squelch-tx off -l "$LANE"
assert_rc 0 "auto-squelch-tx off"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].auto_squelch_dis_tx")
assert_eq "$val" "true" "auto_squelch_dis_tx set (squelch off)"

sff_cmd --auto-squelch-tx on -l "$LANE"
assert_rc 0 "auto-squelch-tx on"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].auto_squelch_dis_tx")
assert_eq "$val" "false" "auto_squelch_dis_tx cleared (squelch on)"

pass
