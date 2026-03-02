#!/bin/bash
# t50_dp_deactivate_all — Test data path deactivation for all lanes
source "$(dirname "$0")/../lib/common.sh"

# Deactivate all lanes (omit --lane)
sff_cmd --dp-deactivate
assert_rc 0 "dp-deactivate all lanes"

sleep 1

# Verify all lanes are DPDeactivated
sff_json -D
for i in 0 1; do
    state=$(jq_field ".[0].datapath_status.datapath_bank_0[$i].dp_state")
    assert_eq "$state" "DPDeactivated" "lane $i dp_state is DPDeactivated"
done

pass
