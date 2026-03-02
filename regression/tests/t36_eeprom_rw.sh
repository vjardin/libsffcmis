#!/bin/bash
# t36_eeprom_rw — Test user EEPROM read/write via JSON
source "$(dirname "$0")/../lib/common.sh"

# Write test pattern
sff_json --eeprom-write 0 "CAFEBABE"
assert_rc 0 "eeprom-write"
assert_json ".eeprom_write.bytes_written" "4" "bytes written"

eeprom_delay

# Read back and verify
sff_json --eeprom-read 0 4
assert_rc 0 "eeprom-read"
assert_json ".eeprom_read.length" "4" "read length"

val=$(jq_field '.eeprom_read.data[0]')
assert_eq "$val" "0xca" "byte 0"
val=$(jq_field '.eeprom_read.data[1]')
assert_eq "$val" "0xfe" "byte 1"
val=$(jq_field '.eeprom_read.data[2]')
assert_eq "$val" "0xba" "byte 2"
val=$(jq_field '.eeprom_read.data[3]')
assert_eq "$val" "0xbe" "byte 3"

# Write back zeros
sff_json --eeprom-write 0 "00000000"
assert_rc 0 "eeprom-write restore"

eeprom_delay

pass
