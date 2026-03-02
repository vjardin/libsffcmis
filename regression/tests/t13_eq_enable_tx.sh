#!/bin/bash
# t13_eq_enable_tx — Test adaptive EQ enable on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --eq-enable-tx on -l "$LANE"
assert_rc 0 "eq-enable-tx on"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_enable_tx")
assert_eq "$val" "true" "eq_enable_tx set"

sff_cmd --eq-enable-tx off -l "$LANE"
assert_rc 0 "eq-enable-tx off"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_enable_tx")
assert_eq "$val" "false" "eq_enable_tx cleared"

pass
