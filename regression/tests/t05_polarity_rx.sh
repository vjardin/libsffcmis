#!/bin/bash
# t05_polarity_rx — Test Rx polarity flip/normal
source "$(dirname "$0")/../lib/common.sh"

# Flip
sff_cmd --polarity-rx flip -l "$LANE"
assert_rc 0 "polarity-rx flip"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].polarity_rx")
assert_eq "$val" "true" "polarity_rx set"

# Normal
sff_cmd --polarity-rx normal -l "$LANE"
assert_rc 0 "polarity-rx normal"

sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].polarity_rx")
assert_eq "$val" "false" "polarity_rx cleared"

pass
