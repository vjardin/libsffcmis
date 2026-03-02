#!/bin/bash
# t45_np_in_use — Test NP in-use flag with np-config via JSON
source "$(dirname "$0")/../lib/common.sh"

# Ensure deactivated first
sff_cmd --np-deactivate -l "$LANE"
assert_rc 0 "np-deactivate before config"
sleep 1

# Configure with np-in-use set via JSON
sff_json --np-config --npid 0 --np-in-use -l "$LANE"
assert_rc 0 "np-config with np-in-use"
assert_json ".write_result.register" "NPConfig" "register name"
assert_json ".write_result.np_in_use" "true" "np_in_use set"

pass
