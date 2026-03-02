#!/bin/bash
# t79_xfp_ext_int_flags — Extended interrupt flags object present with booleans
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].extended_interrupt_flags | type')
assert_eq "$val" "object" "extended_interrupt_flags is object"

# Spot-check a few key flags
for field in tx_nr tx_fault rx_los mod_nr apd_fault tec_fault wavelength_unlocked; do
    val=$(jq_field ".[0].extended_interrupt_flags.$field | type")
    assert_eq "$val" "boolean" "ext_int_flags.$field is boolean"
done

pass
