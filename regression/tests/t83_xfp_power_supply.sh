#!/bin/bash
# t83_xfp_power_supply — Power supply object with max_total_power number
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

val=$(jq_field '.[0].power_supply | type')
assert_eq "$val" "object" "power_supply is object"

val=$(jq_field '.[0].power_supply.max_total_power | type')
assert_eq "$val" "number" "max_total_power is number"

# Check other power supply fields
for field in max_p_down_power "+5v_supply_current" "+3.3v_supply_current" "+1.8v_supply_current" "-5.2v_supply_current"; do
    val=$(jq_field ".[0].power_supply.\"$field\" | type")
    assert_eq "$val" "number" "power_supply.$field is number"
done

pass
