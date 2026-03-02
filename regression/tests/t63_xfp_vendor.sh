#!/bin/bash
# t63_xfp_vendor — Vendor name, PN, SN, rev are non-empty strings
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

for field in vendor_name vendor_pn vendor_sn vendor_rev; do
    val=$(jq_field ".[0].$field")
    if [ -z "$val" ] || [ "$val" = "null" ]; then
        fail "$field is empty or null"
    fi
    typ=$(jq_field ".[0].$field | type")
    assert_eq "$typ" "string" "$field is string"
done

pass
