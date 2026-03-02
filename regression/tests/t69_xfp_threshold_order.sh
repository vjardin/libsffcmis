#!/bin/bash
# t69_xfp_threshold_order — high_alarm > high_warning > low_warning > low_alarm
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

for obj in laser_bias_current laser_output_power module_temperature laser_rx_power; do
    ha=$(jq_field ".[0].$obj.high_alarm_threshold")
    hw=$(jq_field ".[0].$obj.high_warning_threshold")
    lw=$(jq_field ".[0].$obj.low_warning_threshold")
    la=$(jq_field ".[0].$obj.low_alarm_threshold")

    # high_alarm >= high_warning
    ok=$(echo "$ha >= $hw" | bc -l)
    if [ "$ok" -ne 1 ]; then
        fail "$obj: high_alarm ($ha) < high_warning ($hw)"
    fi

    # high_warning >= low_warning
    ok=$(echo "$hw >= $lw" | bc -l)
    if [ "$ok" -ne 1 ]; then
        fail "$obj: high_warning ($hw) < low_warning ($lw)"
    fi

    # low_warning >= low_alarm
    ok=$(echo "$lw >= $la" | bc -l)
    if [ "$ok" -ne 1 ]; then
        fail "$obj: low_warning ($lw) < low_alarm ($la)"
    fi
done

pass
