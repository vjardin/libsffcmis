#!/bin/bash
# t70_xfp_flags — 16 alarm/warning flags present as booleans
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

flags=(
    laser_bias_current_high_alarm
    laser_bias_current_low_alarm
    laser_output_power_high_alarm
    laser_output_power_low_alarm
    module_temperature_high_alarm
    module_temperature_low_alarm
    laser_rx_power_high_alarm
    laser_rx_power_low_alarm
    laser_bias_current_high_warning
    laser_bias_current_low_warning
    laser_output_power_high_warning
    laser_output_power_low_warning
    module_temperature_high_warning
    module_temperature_low_warning
    laser_rx_power_high_warning
    laser_rx_power_low_warning
)

for flag in "${flags[@]}"; do
    val=$(jq_field ".[0].$flag | type")
    assert_eq "$val" "boolean" "$flag is boolean"
done

pass
