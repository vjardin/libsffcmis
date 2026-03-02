#!/bin/bash
# t37_password_entry — Test password entry via JSON
source "$(dirname "$0")/../lib/common.sh"

sff_json --password-entry 00000000
assert_rc 0 "password-entry"
assert_json ".password_entry.result" "success" "password entry result"

pass
