#!/bin/bash
# t04_polarity_tx — Test Tx polarity flip/normal
source "$(dirname "$0")/../lib/common.sh"

# Flip
sff_cmd --polarity-tx flip -l "$LANE"
assert_rc 0 "polarity-tx flip"

# Verify via JSON
sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].polarity_tx")
assert_eq "$val" "true" "polarity_tx set"

# Normal
sff_cmd --polarity-tx normal -l "$LANE"
assert_rc 0 "polarity-tx normal"

# Verify restored
sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].polarity_tx")
assert_eq "$val" "false" "polarity_tx cleared"

pass
