#!/bin/bash
# t44_np_config — Test network path lane configuration via JSON
source "$(dirname "$0")/../lib/common.sh"

# Ensure deactivated first
sff_cmd --np-deactivate -l "$LANE"
assert_rc 0 "np-deactivate before config"
sleep 1

# Configure with npid=0 via JSON
sff_json --np-config --npid 0 -l "$LANE"
assert_rc 0 "np-config npid=0"
assert_json ".write_result.register" "NPConfig" "register name"
assert_json ".write_result.np_id" "0" "np_id"

pass
