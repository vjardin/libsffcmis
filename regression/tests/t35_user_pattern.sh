#!/bin/bash
# t35_user_pattern — Test PRBS user-defined pattern write via JSON
source "$(dirname "$0")/../lib/common.sh"

sff_json --user-pattern "A5A5A5A5"
assert_rc 0 "user-pattern write"
assert_json ".user_pattern_write.bytes_written" "4" "bytes written"

pass
