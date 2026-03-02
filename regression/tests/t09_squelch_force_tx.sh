#!/bin/bash
# t09_squelch_force_tx — Test Tx squelch force on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --squelch-force-tx on -l "$LANE"
assert_rc 0 "squelch-force-tx on"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].squelch_force_tx")
assert_eq "$val" "true" "squelch_force_tx set"

sff_cmd --squelch-force-tx off -l "$LANE"
assert_rc 0 "squelch-force-tx off"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].squelch_force_tx")
assert_eq "$val" "false" "squelch_force_tx cleared"

pass
