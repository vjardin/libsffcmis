#!/bin/bash
# t66_xfp_encoding — encoding field present (uint value)
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].encoding | type')
assert_eq "$val" "number" "encoding is number"

# encoding_description should be a non-empty string
val=$(jq_field '.[0].encoding_description')
if [ -z "$val" ] || [ "$val" = "null" ]; then
    fail "encoding_description is empty or null"
fi

pass
