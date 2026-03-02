#!/bin/bash
# t39_vdm_mask — Test VDM alarm mask nibble set/clear via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set mask for instance 0, nibble=0xF (all masked)
sff_json --vdm-mask 0 0xF
assert_rc 0 "vdm-mask set 0xF"
assert_json ".write_result.register" "VDM_Mask" "register name"
assert_json ".write_result.instance" "0" "instance"

# Clear mask
sff_json --vdm-mask 0 0x0
assert_rc 0 "vdm-mask clear 0x0"
assert_json ".write_result.register" "VDM_Mask" "register name"
assert_json ".write_result.mask" "0" "mask cleared"

pass
