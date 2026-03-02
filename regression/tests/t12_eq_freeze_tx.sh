#!/bin/bash
# t12_eq_freeze_tx — Test adaptive EQ freeze on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --eq-freeze-tx on -l "$LANE"
assert_rc 0 "eq-freeze-tx on"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_freeze_tx")
assert_eq "$val" "true" "eq_freeze_tx set"

sff_cmd --eq-freeze-tx off -l "$LANE"
assert_rc 0 "eq-freeze-tx off"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].eq_freeze_tx")
assert_eq "$val" "false" "eq_freeze_tx cleared"

pass
