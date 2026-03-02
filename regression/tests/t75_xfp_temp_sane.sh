#!/bin/bash
# t75_xfp_temp_sane — Module temperature in -40 to +100 C range
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

temp=$(jq_field '.[0].module_temperature_measurement')
if [ "$temp" = "null" ]; then
    fail "module_temperature_measurement is null"
fi

in_range=$(echo "$temp >= -40 && $temp <= 100" | bc -l)
if [ "$in_range" -ne 1 ]; then
    fail "temperature $temp C not in -40 to +100 range"
fi

pass
