#!/bin/bash
# t11_vdm_power_saving — Test VDM power saving on/off
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --vdm-power-saving on
assert_rc 0 "vdm-power-saving on"

sff_json -V
val=$(jq_field ".[0].vdm_monitors.power_saving")
assert_eq "$val" "true" "power_saving set"

sff_cmd --vdm-power-saving off
assert_rc 0 "vdm-power-saving off"

sff_json -V
val=$(jq_field ".[0].vdm_monitors.power_saving")
assert_eq "$val" "false" "power_saving cleared"

pass
