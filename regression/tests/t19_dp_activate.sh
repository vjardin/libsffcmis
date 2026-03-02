#!/bin/bash
# t19_dp_activate — Test data path activation for a lane
# Note: requires fiber connected and valid application selected
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --dp-activate -l "$LANE"
assert_rc 0 "dp-activate"

# Wait for activation
sleep 3

# Check state reached DPActivated
sff_json -D
state=$(jq_field ".[0].datapath_status.datapath_bank_0[$LANE].dp_state")
assert_eq "$state" "DPActivated" "dp_state is DPActivated"

pass
