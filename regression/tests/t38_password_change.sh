#!/bin/bash
# t38_password_change — Test password change and restore via JSON
source "$(dirname "$0")/../lib/common.sh"

# Change from default to test password
sff_json --password-change 00000000 12345678
assert_rc 0 "password-change to test"
assert_json ".password_change.result" "success" "password change result"

# Enter with new password
sff_json --password-entry 12345678
assert_rc 0 "password-entry new"
assert_json ".password_entry.result" "success" "password entry with new"

# Change back to default
sff_json --password-change 12345678 00000000
assert_rc 0 "password-change restore"
assert_json ".password_change.result" "success" "password restore result"

# Enter with default
sff_json --password-entry 00000000
assert_rc 0 "password-entry default"
assert_json ".password_entry.result" "success" "password entry default"

pass
