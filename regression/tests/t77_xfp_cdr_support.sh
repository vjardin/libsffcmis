#!/bin/bash
# t77_xfp_cdr_support — CDR support field and 7 booleans present
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# cdr_support is a number (hex value)
val=$(jq_field '.[0].cdr_support | type')
assert_eq "$val" "number" "cdr_support is number"

# 7 CDR boolean sub-fields
for field in 9_95g 10_3g 10_5g 10_7g 11_1g lineside_loopback xfi_loopback; do
    val=$(jq_field ".[0].\"$field\" | type")
    assert_eq "$val" "boolean" "$field is boolean"
done

pass
