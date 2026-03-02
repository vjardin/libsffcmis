#!/bin/bash
# t48_tuning_mask — Test per-lane tuning flag mask via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set mask bits on
sff_json --tuning-mask 0xFF on -l "$LANE"
assert_rc 0 "tuning-mask on"
assert_json ".write_result.register" "TuningFlagMask" "register name"

# Clear mask bits
sff_json --tuning-mask 0xFF off -l "$LANE"
assert_rc 0 "tuning-mask off"
assert_json ".write_result.register" "TuningFlagMask" "register name off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0" "tuning-mask cleared"

pass
