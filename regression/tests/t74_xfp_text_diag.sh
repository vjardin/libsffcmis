#!/bin/bash
# t74_xfp_text_diag — Text output contains diagnostic sections
source "$(dirname "$0")/../lib/common.sh"

sff_cmd
assert_rc 0 "xfp text output"

assert_output_contains "bias" "bias current present"
assert_output_contains "power" "power present"
assert_output_contains "temperature" "temperature present"
assert_output_contains "threshold" "threshold present"

pass
