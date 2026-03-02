#!/bin/bash
# t29_host_threshold_enable — Test host threshold enable/disable
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --host-threshold-enable host-fdd
assert_rc 0 "host-threshold-enable host-fdd"

sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fdd_enable")
assert_eq "$val" "true" "host_fdd_enable set"

sff_cmd --host-threshold-disable host-fdd
assert_rc 0 "host-threshold-disable host-fdd"

sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fdd_enable")
assert_eq "$val" "false" "host_fdd_enable cleared"

sff_cmd --host-threshold-enable host-fed
assert_rc 0 "host-threshold-enable host-fed"

sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fed_enable")
assert_eq "$val" "true" "host_fed_enable set"

sff_cmd --host-threshold-disable host-fed
assert_rc 0 "host-threshold-disable host-fed"

sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fed_enable")
assert_eq "$val" "false" "host_fed_enable cleared"

pass
