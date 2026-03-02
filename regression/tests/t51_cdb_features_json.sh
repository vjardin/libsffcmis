#!/bin/bash
# t51_cdb_features_json — Validate JSON output of CDB features (-j -C)
source "$(dirname "$0")/../lib/common.sh"

# Run CDB features with JSON output
sff_json -C
assert_rc 0 "cdb features json"

# Validate top-level structure
assert_json '.[0] | has("cdb_features")' "true" \
  "top-level cdb_features key"

# Validate module_features sub-object
assert_json '.[0].cdb_features | has("module_features")' "true" \
  "module_features present"

val=$(jq_field '.[0].cdb_features.module_features.status')
assert_eq "$val" "Success" "module_features status"

# supported_commands must be an array
val=$(jq_field '.[0].cdb_features.module_features.supported_commands | type')
assert_eq "$val" "array" "supported_commands is array"

# command_count must be a number >= 1
val=$(jq_field '.[0].cdb_features.module_features.command_count')
if [ "$val" -lt 1 ] 2>/dev/null; then
    fail "command_count < 1: $val"
fi

# Each command entry must have id and name
val=$(jq_field '.[0].cdb_features.module_features.supported_commands[0] | (has("id") and has("name"))')
assert_eq "$val" "true" "command entry has id+name"

# Validate fw_mgmt_features sub-object
assert_json '.[0].cdb_features | has("fw_mgmt_features")' "true" \
  "fw_mgmt_features present"

val=$(jq_field '.[0].cdb_features.fw_mgmt_features.status')
assert_eq "$val" "Success" "fw_mgmt status"

# Check boolean fields exist
for field in image_readback skip_erased_blocks copy_command abort_command; do
    val=$(jq_field ".[0].cdb_features.fw_mgmt_features.$field | type")
    assert_eq "$val" "boolean" "fw_mgmt $field is boolean"
done

# Validate fw_info sub-object
assert_json '.[0].cdb_features | has("fw_info")' "true" \
  "fw_info present"

val=$(jq_field '.[0].cdb_features.fw_info.status')
assert_eq "$val" "Success" "fw_info status"

# fw_status_byte must be a hex string
val=$(jq_field '.[0].cdb_features.fw_info.fw_status_byte')
case "$val" in
    0x*) ;; # ok
    *) fail "fw_status_byte not hex: $val" ;;
esac

# image_a must have running/committed/valid/version fields
for field in running committed valid version; do
    val=$(jq_field ".[0].cdb_features.fw_info.image_a | has(\"$field\")")
    assert_eq "$val" "true" "image_a has $field"
done

# Validate bert_features sub-object
assert_json '.[0].cdb_features | has("bert_features")' "true" \
  "bert_features present"

# Validate security_features sub-object
assert_json '.[0].cdb_features | has("security_features")' "true" \
  "security_features present"

val=$(jq_field '.[0].cdb_features.security_features | (has("command_count") or has("error"))')
assert_eq "$val" "true" "security has command_count or error"

pass
