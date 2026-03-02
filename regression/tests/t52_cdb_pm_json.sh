#!/bin/bash
# t52_cdb_pm_json — Validate JSON output of CDB performance monitoring (-j -P)
source "$(dirname "$0")/../lib/common.sh"

# Run CDB PM with JSON output
sff_json -P
assert_rc 0 "cdb pm json"

# Validate top-level structure
assert_json '.[0] | has("cdb_pm")' "true" \
  "top-level cdb_pm key"

# Validate pm_features sub-object
assert_json '.[0].cdb_pm | has("pm_features")' "true" \
  "pm_features present"

# pm_features may have an "error" if the CDB command is not supported;
# if supported, verify boolean fields
pm_err=$(jq_field '.[0].cdb_pm.pm_features.error // empty')
if [ -z "$pm_err" ]; then
    for field in host_snr host_pam4_ltp host_pre_fec_ber media_snr media_ltp; do
        val=$(jq_field ".[0].cdb_pm.pm_features.$field | type")
        assert_eq "$val" "boolean" "pm_features.$field is boolean"
    done
fi

# Validate module_pm sub-object
assert_json '.[0].cdb_pm | has("module_pm")' "true" \
  "module_pm present"

mod_err=$(jq_field '.[0].cdb_pm.module_pm.error // empty')
if [ -z "$mod_err" ]; then
    val=$(jq_field '.[0].cdb_pm.module_pm | (has("has_current") or has("records"))')
    assert_eq "$val" "true" "module_pm has has_current or records"

    val=$(jq_field '.[0].cdb_pm.module_pm.records | type')
    assert_eq "$val" "array" "module_pm records is array"

    # Check first record (Temperature) has expected fields
    rec_count=$(jq_field '.[0].cdb_pm.module_pm.records | length')
    if [ "$rec_count" -ge 1 ]; then
        val=$(jq_field '.[0].cdb_pm.module_pm.records[0] | (has("observable") and has("min") and has("avg") and has("max") and has("unit"))')
        assert_eq "$val" "true" "module_pm record has required fields"

        val=$(jq_field '.[0].cdb_pm.module_pm.records[0].observable')
        assert_eq "$val" "Temperature" "first record is Temperature"

        val=$(jq_field '.[0].cdb_pm.module_pm.records[0].unit')
        assert_eq "$val" "degC" "temperature unit is degC"

        for stat in min avg max; do
            val=$(jq_field ".[0].cdb_pm.module_pm.records[0].$stat | type")
            assert_eq "$val" "number" "temperature $stat is number"
        done
    fi
fi

# Validate media_pm sub-object
assert_json '.[0].cdb_pm | has("media_pm")' "true" \
  "media_pm present"

media_err=$(jq_field '.[0].cdb_pm.media_pm.error // empty')
if [ -z "$media_err" ]; then
    val=$(jq_field '.[0].cdb_pm.media_pm.records | type')
    assert_eq "$val" "array" "media_pm records is array"

    rec_count=$(jq_field '.[0].cdb_pm.media_pm.records | length')
    if [ "$rec_count" -ge 1 ]; then
        val=$(jq_field '.[0].cdb_pm.media_pm.records[0] | has("lane")')
        assert_eq "$val" "true" "media_pm record has lane"
    fi
fi

# Validate dp_pm sub-object
assert_json '.[0].cdb_pm | has("dp_pm")' "true" \
  "dp_pm present"

dp_err=$(jq_field '.[0].cdb_pm.dp_pm.error // empty')
if [ -z "$dp_err" ]; then
    val=$(jq_field '.[0].cdb_pm.dp_pm.records | type')
    assert_eq "$val" "array" "dp_pm records is array"
fi

pass
