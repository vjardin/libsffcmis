#!/bin/bash
# t25_tx_filter — Test Tx filter enable/disable
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --tx-filter on -l "$LANE"
assert_rc 0 "tx-filter on"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].tx_filter_enable")
assert_eq "$val" "true" "tx_filter_enable set"

sff_cmd --tx-filter off -l "$LANE"
assert_rc 0 "tx-filter off"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].tx_filter_enable")
assert_eq "$val" "false" "tx_filter_enable cleared"

pass
