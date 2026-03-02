#!/bin/bash
# t28_host_threshold — Test host BER threshold write
source "$(dirname "$0")/../lib/common.sh"

# Set host FDD raise threshold
sff_cmd --host-threshold host-fdd-raise 1e-5
assert_rc 0 "host-threshold host-fdd-raise"

# Verify via JSON (host_thresholds under coherent display)
sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fdd_raise_ber")
# Verify it is a non-null float (exact value depends on F16 rounding)
if [ "$val" = "null" ] || [ -z "$val" ]; then
    fail "host_fdd_raise_ber is null or empty"
fi

# Set host FED raise threshold
sff_cmd --host-threshold host-fed-raise 1e-3
assert_rc 0 "host-threshold host-fed-raise"

sff_json -K
val=$(jq_field ".[0].host_thresholds.bank_0.host_fed_raise_ber")
if [ "$val" = "null" ] || [ -z "$val" ]; then
    fail "host_fed_raise_ber is null or empty"
fi

pass
