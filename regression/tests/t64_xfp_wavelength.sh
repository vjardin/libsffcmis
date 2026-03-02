#!/bin/bash
# t64_xfp_wavelength — Wavelength present and in 700-1700 nm range
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].laser_wavelength')
if [ "$val" = "null" ]; then
    fail "laser_wavelength is null"
fi

# Check range: 700 <= wavelength <= 1700
in_range=$(echo "$val >= 700 && $val <= 1700" | bc -l)
if [ "$in_range" -ne 1 ]; then
    fail "laser_wavelength $val not in 700-1700 nm range"
fi

pass
