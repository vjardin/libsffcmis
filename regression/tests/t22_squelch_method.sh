#!/bin/bash
# t22_squelch_method — Test squelch method oma/pav
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --squelch-method pav
assert_rc 0 "squelch-method pav"

sff_json
val=$(jq_field ".[0].module_level_controls.squelch_method_pav")
assert_eq "$val" "true" "squelch_method_pav set"

sff_cmd --squelch-method oma
assert_rc 0 "squelch-method oma"

sff_json
val=$(jq_field ".[0].module_level_controls.squelch_method_pav")
assert_eq "$val" "false" "squelch_method_pav cleared"

pass
