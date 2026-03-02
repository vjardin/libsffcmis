#!/bin/bash
# t46_np_activate — Test network path activation
# Note: requires fiber connected for NP to reach NPActivated state
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --np-activate -l "$LANE"
assert_rc 0 "np-activate"

sleep 3

sff_json -N
state=$(jq_field ".[0].netpath_status.netpath_bank_0[$LANE].np_state")
assert_eq "$state" "NPActivated" "np_state is NPActivated"

pass
