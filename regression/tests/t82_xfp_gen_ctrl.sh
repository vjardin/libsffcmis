#!/bin/bash
# t82_xfp_gen_ctrl — General control/status object with status booleans
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].general_control_status | type')
assert_eq "$val" "object" "general_control_status is object"

# Check key status booleans from both bytes 110 and 111
for field in tx_disable_state soft_tx_disable mod_nr p_down_state rx_los data_not_ready tx_fault tx_cdr_not_locked rx_cdr_not_locked; do
    val=$(jq_field ".[0].general_control_status.$field | type")
    assert_eq "$val" "boolean" "gen_ctrl.$field is boolean"
done

pass
