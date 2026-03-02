#!/bin/bash
# t42_np_state_mask — Test NP state-changed mask on/off via JSON
source "$(dirname "$0")/../lib/common.sh"

sff_json --np-state-mask on -l "$LANE"
assert_rc 0 "np-state-mask on"
assert_json ".write_result.register" "NPStateChangedMask" "register name"

sff_json --np-state-mask off -l "$LANE"
assert_rc 0 "np-state-mask off"
assert_json ".write_result.register" "NPStateChangedMask" "register name off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0" "np-state-mask cleared"

pass
