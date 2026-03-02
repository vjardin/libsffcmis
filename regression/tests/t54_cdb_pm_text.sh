#!/bin/bash
# t54_cdb_pm_text — Verify CDB PM text output (-P) is unchanged
source "$(dirname "$0")/../lib/common.sh"

# Run CDB PM in text mode
sff_cmd -P
assert_rc 0 "cdb pm text"

# Verify key text markers are present
assert_output_contains "CDB Performance Monitoring" "pm banner"
assert_output_contains "PM Features (CMD 0x0201)" "pm features header"

assert_output_contains "Module PM (CMD 0x0210)" "module pm header"
assert_output_contains "Observable" "table header"
assert_output_contains "Temperature" "temperature record"
assert_output_contains "degC" "temperature unit"

assert_output_contains "Media Side PM (CMD 0x0214)" "media pm header"
assert_output_contains "Data Path PM (CMD 0x0216)" "dp pm header"

pass
