#!/bin/bash
# t34_diag_mask — Test diagnostics mask set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set diag mask at offset 0, on
sff_json --diag-mask 0 on -l "$LANE"
assert_rc 0 "diag-mask set on"
val=$(jq_field '.write_result.new_value')
[ "$val" != "null" ] || { fail "no write_result.new_value"; false; }

# Clear
sff_json --diag-mask 0 off -l "$LANE"
assert_rc 0 "diag-mask set off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0x00" "diag-mask cleared"

pass
