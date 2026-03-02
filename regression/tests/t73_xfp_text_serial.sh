#!/bin/bash
# t73_xfp_text_serial — Text output contains key serial ID fields
source "$(dirname "$0")/../lib/common.sh"

sff_cmd
assert_rc 0 "xfp text output"

assert_output_contains "Identifier" "Identifier present"
assert_output_contains "Connector" "Connector present"
assert_output_contains "Vendor name" "Vendor name present"
assert_output_contains "Vendor PN" "Vendor PN present"
assert_output_contains "Vendor SN" "Vendor SN present"
assert_output_contains "Laser wavelength" "Laser wavelength present"
assert_output_contains "Encoding" "Encoding present"

pass
