#!/bin/bash
# t53_cdb_features_text — Verify CDB features text output (-C) is unchanged
source "$(dirname "$0")/../lib/common.sh"

# Run CDB features in text mode
sff_cmd -C
assert_rc 0 "cdb features text"

# Verify key text markers are present
assert_output_contains "CDB Module Features (CMD 0040h)" "module features header"
assert_output_contains "Supported standard CDB commands" "commands list header"
assert_output_contains "CMD 0x00" "at least one CMD listed"
assert_output_contains "Total:" "total count"

assert_output_contains "Firmware Management Features (CMD 0041h)" "fw mgmt header"
assert_output_contains "Image readback:" "fw mgmt readback field"

assert_output_contains "Get Firmware Info (CMD 0x0100h)" "fw info header"
assert_output_contains "Firmware Status:" "fw status field"
assert_output_contains "Image A:" "image a present"

assert_output_contains "BERT and Diagnostics Features" "bert header"
assert_output_contains "Security Features" "security header"

pass
