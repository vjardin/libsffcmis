#!/bin/bash
# t60_xfp_json_valid — Run with -j, rc=0, output is valid JSON
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json exit code"

# Validate JSON is parseable
if ! jq empty "$JSON_TMP" 2>/dev/null; then
    fail "output is not valid JSON"
fi

# Must be a non-empty array
val=$(jq_field '. | type')
assert_eq "$val" "array" "top-level is array"

val=$(jq_field '. | length')
if [ "$val" -lt 1 ]; then
    fail "JSON array is empty"
fi

pass
