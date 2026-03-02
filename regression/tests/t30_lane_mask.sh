#!/bin/bash
# t30_lane_mask — Test lane interrupt mask set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set lane mask byte at offset 0, mask=0xFF, on
sff_json --lane-mask 0 0xFF on
assert_rc 0 "lane-mask set on"
val=$(jq_field '.write_result.new_value')
# After setting mask bits on, new_value should have 0xFF bits set
[ "$val" != "null" ] || { fail "no write_result.new_value"; false; }

# Clear
sff_json --lane-mask 0 0xFF off
assert_rc 0 "lane-mask set off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0" "lane-mask cleared"

pass
