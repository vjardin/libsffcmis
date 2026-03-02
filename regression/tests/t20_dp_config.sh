#!/bin/bash
# t20_dp_config — Test data path lane configuration
source "$(dirname "$0")/../lib/common.sh"

# Ensure deactivated first
sff_cmd --dp-deactivate -l "$LANE"
assert_rc 0 "dp-deactivate before config"
sleep 1

# Configure with appsel=1, dpid=0
sff_cmd --dp-config --appsel 1 --dpid 0 -l "$LANE"
assert_rc 0 "dp-config appsel=1 dpid=0"

# Verify via JSON
sff_json -D
val=$(jq_field ".[0].datapath_status.datapath_control_bank_0[$LANE].dp_id")
assert_eq "$val" "0" "dp_id is 0"

pass
