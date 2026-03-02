#!/bin/bash
# t67_xfp_diag_readings — Temperature, bias, TX power, RX power exist and are numbers
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

# Laser bias current
val=$(jq_field '.[0].laser_tx_bias_current | type')
assert_eq "$val" "number" "laser_tx_bias_current is number"

# Transmit optical power
val=$(jq_field '.[0].transmit_avg_optical_power | type')
assert_eq "$val" "number" "transmit_avg_optical_power is number"

# RX power (nested in rx_power object)
val=$(jq_field '.[0].rx_power.value | type')
assert_eq "$val" "number" "rx_power.value is number"

# Module temperature
val=$(jq_field '.[0].module_temperature_measurement | type')
assert_eq "$val" "number" "module_temperature_measurement is number"

pass
