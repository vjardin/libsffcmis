#!/bin/bash
# t68_xfp_thresholds — Four threshold objects present, each with 4 levels
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

for obj in laser_bias_current laser_output_power module_temperature laser_rx_power; do
    # Object must exist
    val=$(jq_field ".[0].$obj | type")
    assert_eq "$val" "object" "$obj is object"

    # Must have 4 threshold levels
    for level in high_alarm_threshold low_alarm_threshold high_warning_threshold low_warning_threshold; do
        val=$(jq_field ".[0].$obj.$level | type")
        assert_eq "$val" "number" "$obj.$level is number"
    done
done

pass
