#!/bin/bash
# t21_bank_broadcast — Test bank broadcast enable/disable
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --bank-broadcast on
assert_rc 0 "bank-broadcast on"

sff_json
val=$(jq_field ".[0].module_level_controls.bank_broadcast_enable")
assert_eq "$val" "true" "bank_broadcast_enable set"

sff_cmd --bank-broadcast off
assert_rc 0 "bank-broadcast off"

sff_json
val=$(jq_field ".[0].module_level_controls.bank_broadcast_enable")
assert_eq "$val" "false" "bank_broadcast_enable cleared"

pass
