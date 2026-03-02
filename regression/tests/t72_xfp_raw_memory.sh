#!/bin/bash
# t72_xfp_raw_memory — Two raw memory arrays present, each with 128 elements
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# Raw lower memory (0x00-0x7F)
val=$(jq_field '.[0]["raw_lower_memory_(0x00-0x7f)"] | type')
assert_eq "$val" "array" "raw lower memory is array"

val=$(jq_field '.[0]["raw_lower_memory_(0x00-0x7f)"] | length')
assert_eq "$val" "128" "raw lower memory has 128 elements"

# Raw upper memory (0x80-0xFF)
val=$(jq_field '.[0]["raw_upper_memory_(0x80-0xff)"] | type')
assert_eq "$val" "array" "raw upper memory is array"

val=$(jq_field '.[0]["raw_upper_memory_(0x80-0xff)"] | length')
assert_eq "$val" "128" "raw upper memory has 128 elements"

pass
