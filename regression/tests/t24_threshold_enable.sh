#!/bin/bash
# t24_threshold_enable — Test threshold enable/disable toggle
source "$(dirname "$0")/../lib/common.sh"

# Enable total-pwr threshold
sff_cmd --threshold-enable total-pwr
assert_rc 0 "threshold-enable total-pwr"

# Verify
sff_json -K
assert_json ".[0].coherent_thresholds.bank_0.use_cfg_total_pwr" "true" \
  "threshold enabled"

# Disable
sff_cmd --threshold-disable total-pwr
assert_rc 0 "threshold-disable total-pwr"

sff_json -K
assert_json ".[0].coherent_thresholds.bank_0.use_cfg_total_pwr" "false" \
  "threshold disabled"

pass
