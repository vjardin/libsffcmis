#!/bin/bash
# t84_xfp_epon_text — EPON detection: if module is EPON, text contains "EPON"
#                      For non-EPON modules, this test always passes.
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# Check if epon_detection object exists in JSON
epon_type=$(jq_field '.[0].epon_detection.epon_type // empty')

if [ -n "$epon_type" ]; then
    # EPON detected in JSON — verify text output also mentions EPON
    sff_cmd
    assert_rc 0 "xfp text output"
    assert_output_contains "EPON" "EPON in text output"
fi

# Non-EPON modules: always pass
pass
