#!/bin/bash
# t03_scratchpad — Test host scratchpad write/read via JSON
source "$(dirname "$0")/../lib/common.sh"

TEST_PATTERN="DEADBEEF01020304"

# Write scratchpad
sff_json --scratchpad-write "$TEST_PATTERN"
assert_rc 0 "scratchpad write"
assert_json ".scratchpad_write.bytes_written" "8" "bytes written"

eeprom_delay

# Read back via JSON
sff_json --scratchpad-read
assert_rc 0 "scratchpad read"

# Verify data array
val=$(jq_field '.scratchpad.data[0]')
assert_eq "$val" "0xde" "byte 0"
val=$(jq_field '.scratchpad.data[1]')
assert_eq "$val" "0xad" "byte 1"
val=$(jq_field '.scratchpad.data[4]')
assert_eq "$val" "0x01" "byte 4"
val=$(jq_field '.scratchpad.data[7]')
assert_eq "$val" "0x04" "byte 7"

# Clear scratchpad
sff_json --scratchpad-write "0000000000000000"
assert_rc 0 "scratchpad clear"

pass
