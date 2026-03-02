#!/bin/bash
# t31_module_mask — Test module-level mask set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set module mask at offset 0, mask=0x01, on
sff_json --module-mask 0 0x01 on
assert_rc 0 "module-mask set on"
val=$(jq_field '.write_result.new_value')
[ "$val" != "null" ] || { fail "no write_result.new_value"; false; }

# Clear
sff_json --module-mask 0 0x01 off
assert_rc 0 "module-mask set off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0x00" "module-mask cleared"

pass
