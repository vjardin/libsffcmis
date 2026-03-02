#!/bin/bash
# t61_xfp_identifier — Identifier field present, value is XFP (0x06)
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# identifier is uint 6 for XFP
assert_json '.[0].identifier' "6" "identifier value"

# identifier_description contains "XFP"
val=$(jq_field '.[0].identifier_description')
case "$val" in
    *XFP*) ;;
    *) fail "identifier_description does not contain XFP: $val" ;;
esac

pass
