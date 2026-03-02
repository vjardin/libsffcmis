#!/bin/bash
# t78_xfp_checksums — CC_BASE and CC_EXT checksum strings present
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# cc_base must be "pass" or "fail"
val=$(jq_field '.[0].cc_base')
case "$val" in
    pass|fail) ;;
    *) fail "cc_base unexpected value: $val" ;;
esac

# cc_ext must be "pass" or "fail"
val=$(jq_field '.[0].cc_ext')
case "$val" in
    pass|fail) ;;
    *) fail "cc_ext unexpected value: $val" ;;
esac

pass
