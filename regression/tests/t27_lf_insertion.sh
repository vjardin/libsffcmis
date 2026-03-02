#!/bin/bash
# t27_lf_insertion — Test LF insertion on LD enable/disable
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --lf-insertion on -l "$LANE"
assert_rc 0 "lf-insertion on"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].lf_insertion_on_ld")
assert_eq "$val" "true" "lf_insertion_on_ld set"

sff_cmd --lf-insertion off -l "$LANE"
assert_rc 0 "lf-insertion off"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].lf_insertion_on_ld")
assert_eq "$val" "false" "lf_insertion_on_ld cleared"

pass
