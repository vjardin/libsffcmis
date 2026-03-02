#!/bin/bash
# t41_np_source_tx — Test NP signal source Tx replace/internal
source "$(dirname "$0")/../lib/common.sh"

sff_cmd --np-source-tx replace -l "$LANE"
assert_rc 0 "np-source-tx replace"

sff_json -N
assert_json ".[0].netpath_status.netpath_bank_0[$LANE].np_source_tx_replace" "true" \
  "np_source_tx set to replace"

sff_cmd --np-source-tx hp -l "$LANE"
assert_rc 0 "np-source-tx hp"

sff_json -N
assert_json ".[0].netpath_status.netpath_bank_0[$LANE].np_source_tx_replace" "false" \
  "np_source_tx set to hp"

pass
