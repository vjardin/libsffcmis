#!/bin/bash
# t62_xfp_connector — Connector field present and has a description
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# connector must exist (uint)
val=$(jq_field '.[0].connector | type')
assert_eq "$val" "number" "connector is number"

# connector_description must be a non-empty string
val=$(jq_field '.[0].connector_description')
if [ -z "$val" ] || [ "$val" = "null" ]; then
    fail "connector_description is empty or null"
fi

pass
