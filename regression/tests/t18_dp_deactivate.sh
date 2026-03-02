#!/bin/bash
# t18_dp_deactivate — Test data path deactivation for a lane
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --dp-deactivate -l "$LANE"
assert_rc 0 "dp-deactivate"

# Verify state is DPDeactivated
sleep 1
sff_json -D
assert_json ".[0].datapath_status.datapath_bank_0[$LANE].dp_state" "DPDeactivated" \
  "dp_state is DPDeactivated"

pass
