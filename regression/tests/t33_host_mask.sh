#!/bin/bash
# t33_host_mask — Test host flag mask (Page 3Bh) set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set host mask at offset 0, mask=0xFF, on
sff_json --host-mask 0 0xFF on
assert_rc 0 "host-mask set on"
val=$(jq_field '.write_result.new_value')
[ "$val" != "null" ] || { fail "no write_result.new_value"; false; }

# Clear
sff_json --host-mask 0 0xFF off
assert_rc 0 "host-mask set off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0" "host-mask cleared"

pass
