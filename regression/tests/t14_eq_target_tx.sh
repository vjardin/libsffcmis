#!/bin/bash
# t14_eq_target_tx — Test fixed Tx EQ target setting
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --eq-target-tx 5 -l "$LANE"
assert_rc 0 "eq-target-tx set 5"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_target_tx")
assert_eq "$val" "5" "eq_target_tx is 5"

sff_cmd --eq-target-tx 0 -l "$LANE"
assert_rc 0 "eq-target-tx set 0"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_target_tx")
assert_eq "$val" "0" "eq_target_tx is 0"

pass
