#!/bin/bash
# t32_media_mask — Test media flag mask (Page 32h) set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set media mask at offset 0, mask=0xFF, on
sff_json --media-mask 0 0xFF on
assert_rc 0 "media-mask set on"
val=$(jq_field '.write_result.new_value')
[ "$val" != "null" ] || { fail "no write_result.new_value"; false; }

# Clear
sff_json --media-mask 0 0xFF off
assert_rc 0 "media-mask set off"
val=$(jq_field '.write_result.new_value')
assert_eq "$val" "0" "media-mask cleared"

pass
