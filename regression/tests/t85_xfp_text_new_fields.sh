#!/bin/bash
# t85_xfp_text_new_fields — Text output contains new field labels
source "$(dirname "$0")/../lib/common.sh"

sff_cmd
assert_rc 0 "xfp text output"

assert_output_contains "Device technology" "Device technology present"
assert_output_contains "CDR support" "CDR support present"
assert_output_contains "CC_BASE" "CC_BASE present"
assert_output_contains "Data rate select" "Signal conditioner data rate present"

pass
