#!/bin/bash
# t47_lane_switch — Test host lane switching via JSON
source "$(dirname "$0")/../lib/common.sh"

# Set redirection: lane 0 -> lane 1
sff_json --lane-redir 1 -l "$LANE"
assert_rc 0 "lane-redir set"
assert_json ".write_result.register" "LaneSwitchRedir" "redir register"
assert_json ".write_result.target" "1" "redir target"

# Enable lane switching
sff_json --lane-switch-enable
assert_rc 0 "lane-switch-enable"
assert_json ".write_result.register" "LaneSwitchEnable" "enable register"

# Commit
sff_json --lane-switch-commit
assert_rc 0 "lane-switch-commit"
assert_json ".write_result.register" "LaneSwitchCommit" "commit register"
assert_json ".write_result.result" "committed" "commit result"

# Read result
sff_json --lane-switch-result
assert_rc 0 "lane-switch-result"
val=$(jq_field '.lane_switch_result.result')
[ "$val" != "null" ] || { fail "no lane_switch_result"; false; }

# Disable lane switching
sff_json --lane-switch-disable
assert_rc 0 "lane-switch-disable"
assert_json ".write_result.register" "LaneSwitchEnable" "disable register"

# Commit disable
sff_json --lane-switch-commit
assert_rc 0 "lane-switch-commit after disable"

pass
