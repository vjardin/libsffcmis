#!/bin/bash
# t65_xfp_transceiver — transceiver_codes is an array of 8 elements
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].transceiver_codes | type')
assert_eq "$val" "array" "transceiver_codes is array"

val=$(jq_field '.[0].transceiver_codes | length')
assert_eq "$val" "8" "transceiver_codes has 8 elements"

# Each element must be a number
for i in $(seq 0 7); do
    val=$(jq_field ".[0].transceiver_codes[$i] | type")
    assert_eq "$val" "number" "transceiver_codes[$i] is number"
done

pass
