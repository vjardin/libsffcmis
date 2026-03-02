#!/bin/bash
# t76_xfp_device_tech — Device technology field and 4 booleans present
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# device_technology is a number (hex value)
val=$(jq_field '.[0].device_technology | type')
assert_eq "$val" "number" "device_technology is number"

# 4 boolean sub-fields
for field in wavelength_control cooled_transmitter apd_detector tunable_transmitter; do
    val=$(jq_field ".[0].$field | type")
    assert_eq "$val" "boolean" "$field is boolean"
done

pass
