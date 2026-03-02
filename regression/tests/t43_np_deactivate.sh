#!/bin/bash
# t43_np_deactivate — Test network path deactivation
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --np-deactivate -l "$LANE"
assert_rc 0 "np-deactivate"

sleep 1

sff_json -N
assert_json ".[0].netpath_status.netpath_bank_0[$LANE].np_state" "NPDeactivated" \
  "np_state is NPDeactivated"

pass
