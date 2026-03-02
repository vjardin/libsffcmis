#!/bin/bash
# t23_set_threshold — Test coherent threshold write/verify
source "$(dirname "$0")/../lib/common.sh"

# Set total power high alarm to 5.0 dBm
sff_cmd --set-threshold total-pwr-hi-alarm 5.0
assert_rc 0 "set-threshold total-pwr-hi-alarm"

# Verify via JSON
sff_json -K
val=$(jq_field ".[0].coherent_thresholds.bank_0.total_pwr_hi_alarm_dbm")
# Check it's close to 5.0 (float comparison)
diff=$(echo "$val - 5.0" | bc -l)
abs_diff="${diff#-}"
if [ "$(echo "$abs_diff > 0.1" | bc -l)" -eq 1 ]; then
    fail "threshold value $val not close to 5.0"
fi

# Set back to a safe default
sff_cmd --set-threshold total-pwr-hi-alarm 10.0
assert_rc 0 "restore threshold"

pass
