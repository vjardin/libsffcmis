#!/bin/bash
# t71_xfp_enhanced_opts — 8 enhanced option booleans present
source "$(dirname "$0")/../lib/common.sh"

sff_json
assert_rc 0 "xfp json"

opts=(
    vps_support
    soft_tx_disable
    soft_p_down
    vps_lv_regulator_mode
    vps_bypassed_regulator_mode
    active_fec_control
    wavelength_tunability
    cmu_support
)

for opt in "${opts[@]}"; do
    val=$(jq_field ".[0].$opt | type")
    assert_eq "$val" "boolean" "$opt is boolean"
done

pass
