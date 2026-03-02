#!/bin/bash
# t49_sw_reset — Test software reset
# NOTE: This is a destructive test — module will reset and need re-wake.
# Placed near the end of the suite intentionally.
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --sw-reset
assert_rc 0 "sw-reset"

# Wait for module to come back
sleep 5

# Re-wake module
sff_cmd -w
assert_rc 0 "wake after reset"

sleep 2

# Verify module is responsive with a basic JSON dump
sff_json -D
assert_rc 0 "module responsive after reset"

pass
