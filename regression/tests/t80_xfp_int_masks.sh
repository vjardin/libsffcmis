#!/bin/bash
# t80_xfp_int_masks — Interrupt masks object present with booleans
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].interrupt_masks | type')
assert_eq "$val" "object" "interrupt_masks is object"

# Spot-check a few mask fields
for field in temp_high_alarm bias_high_alarm tx_power_high_alarm rx_power_high_alarm tx_nr rx_los; do
    val=$(jq_field ".[0].interrupt_masks.$field | type")
    assert_eq "$val" "boolean" "interrupt_masks.$field is boolean"
done

pass
