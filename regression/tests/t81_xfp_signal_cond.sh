#!/bin/bash
# t81_xfp_signal_cond — Signal conditioner control object with data_rate_select
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].signal_conditioner_control | type')
assert_eq "$val" "object" "signal_conditioner_control is object"

val=$(jq_field '.[0].signal_conditioner_control.data_rate_select | type')
assert_eq "$val" "number" "data_rate_select is number"

# Check loopback booleans
for field in lineside_loopback xfi_loopback refclk_mode; do
    val=$(jq_field ".[0].signal_conditioner_control.$field | type")
    assert_eq "$val" "boolean" "signal_cond.$field is boolean"
done

pass
