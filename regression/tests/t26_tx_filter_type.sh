#!/bin/bash
# t26_tx_filter_type — Test Tx filter type setting
source "$(dirname "$0")/../lib/common.sh"

# Set to RRC (type 1)
sff_cmd --tx-filter-type 1 -l "$LANE"
assert_rc 0 "tx-filter-type set 1 (RRC)"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].tx_filter_type")
assert_eq "$val" "Root Raised Cosine (RRC)" "tx_filter_type is RRC"

# Restore to None (type 0)
sff_cmd --tx-filter-type 0 -l "$LANE"
assert_rc 0 "tx-filter-type set 0 (None)"

sff_json -K
val=$(jq_field ".[0].media_lane_provisioning.bank_0[$LANE].tx_filter_type")
assert_eq "$val" "None" "tx_filter_type is None"

pass
