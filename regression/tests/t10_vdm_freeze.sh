#!/bin/bash
# t10_vdm_freeze — Test VDM freeze/unfreeze
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --vdm-freeze
assert_rc 0 "vdm-freeze"

sff_json -V
val=$(jq_field ".[0].vdm_monitors.freeze_active")
assert_eq "$val" "true" "freeze_active set"

sff_cmd --vdm-unfreeze
assert_rc 0 "vdm-unfreeze"

sff_json -V
val=$(jq_field ".[0].vdm_monitors.freeze_active")
assert_eq "$val" "false" "freeze_active cleared"

pass
