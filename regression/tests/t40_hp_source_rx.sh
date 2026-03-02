#!/bin/bash
# t40_hp_source_rx — Test HP signal source Rx replace/internal
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --hp-source-rx replace -l "$LANE"
assert_rc 0 "hp-source-rx replace"

sff_json -N
assert_json ".[0].netpath_status.netpath_bank_0[$LANE].hp_source_rx_replace" "true" \
  "hp_source_rx set to replace"

sff_cmd --hp-source-rx np -l "$LANE"
assert_rc 0 "hp-source-rx np"

sff_json -N
assert_json ".[0].netpath_status.netpath_bank_0[$LANE].hp_source_rx_replace" "false" \
  "hp_source_rx set to np"

pass
