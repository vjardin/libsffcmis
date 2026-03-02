#!/bin/bash
# regression/run_all.sh — Main regression runner
# SPDX-License-Identifier: GPL-2.0
#
# Usage: ./run_all.sh <module_json> <i2c_bus> [options]
#   Options:
#     -v          Verbose (show test details)
#     -f <name>   Run only test matching <name> (e.g., t01_tx_disable)
#     -k          Keep going after unexpected failure (default: stop)

set -euo pipefail

REGRESSION_DIR="$(cd "$(dirname "$0")" && pwd)"

usage() {
    echo "Usage: $0 <module_json> <i2c_bus> [options]"
    echo ""
    echo "Options:"
    echo "  -v          Verbose output"
    echo "  -f <name>   Run only a specific test (e.g., t01_tx_disable)"
    echo "  -k          Keep going after unexpected failure"
    echo ""
    echo "Example:"
    echo "  $0 modules/FTLC3355.json 0"
    exit 1
}

# --- Argument parsing ---
if [ $# -lt 2 ]; then
    usage
fi

MODULE_JSON="$1"; shift
I2C_BUS="$1"; shift

VERBOSE=0
FILTER=""
KEEP_GOING=false

while getopts "vf:k" opt; do
    case "$opt" in
        v) VERBOSE=1 ;;
        f) FILTER="$OPTARG" ;;
        k) KEEP_GOING=true ;;
        *) usage ;;
    esac
done

# Resolve module JSON path
if [ ! -f "$MODULE_JSON" ]; then
    # Try relative to regression dir
    if [ -f "$REGRESSION_DIR/$MODULE_JSON" ]; then
        MODULE_JSON="$REGRESSION_DIR/$MODULE_JSON"
    else
        echo "Error: module JSON not found: $MODULE_JSON" >&2
        exit 1
    fi
fi

# --- Prerequisites ---
if ! command -v jq >/dev/null 2>&1; then
    echo "Error: jq is required but not found in PATH" >&2
    exit 1
fi

if ! command -v bc >/dev/null 2>&1; then
    echo "Error: bc is required but not found in PATH" >&2
    exit 1
fi

# --- Read module properties ---
MODULE_NAME=$(jq -r '.name' "$MODULE_JSON")
MODULE_DESC=$(jq -r '.description // ""' "$MODULE_JSON")
WAKE_NEEDED=$(jq -r '.wake_needed // false' "$MODULE_JSON")
DEFAULT_BANK=$(jq -r '.default_bank // 0' "$MODULE_JSON")
DEFAULT_LANE=$(jq -r '.default_lane // 0' "$MODULE_JSON")
EEPROM_WRITE_DELAY_MS=$(jq -r '.eeprom_write_delay_ms // 500' "$MODULE_JSON")

echo "=============================================="
echo "Regression Test Suite"
echo "=============================================="
echo "Module:  $MODULE_NAME — $MODULE_DESC"
echo "Bus:     $I2C_BUS"
echo "Bank:    $DEFAULT_BANK  Lane: $DEFAULT_LANE"
echo "=============================================="
echo ""

# Export environment for test scripts
export BUS="$I2C_BUS"
export BANK="$DEFAULT_BANK"
export LANE="$DEFAULT_LANE"
export VERBOSE
export EEPROM_WRITE_DELAY_MS

# --- Wake module if needed ---
if [ "$WAKE_NEEDED" = "true" ]; then
    echo "Waking module..."
    source "$REGRESSION_DIR/lib/common.sh"
    sff_cmd -w
    if [ "$CMD_RC" -ne 0 ]; then
        echo "Error: failed to wake module (rc=$CMD_RC)" >&2
        exit 1
    fi
    sleep 2
    echo "Module awake."
    echo ""
fi

# --- Helper: lookup expected result for a test ---
get_expect() {
    local test_name="$1"
    local val
    val=$(jq -r ".tests.\"$test_name\".expect // \"pass\"" "$MODULE_JSON")
    echo "$val"
}

get_reason() {
    local test_name="$1"
    jq -r ".tests.\"$test_name\".reason // \"\"" "$MODULE_JSON"
}

# --- Counters ---
total=0
pass_count=0
xfail_count=0
xpass_count=0
fail_count=0
skip_count=0

# Results array for summary
declare -a results_name=()
declare -a results_status=()
declare -a results_detail=()

# --- Run tests ---
for test_script in "$REGRESSION_DIR"/tests/t*.sh; do
    test_file=$(basename "$test_script" .sh)

    # Filter if requested
    if [ -n "$FILTER" ] && [ "$test_file" != "$FILTER" ]; then
        continue
    fi

    total=$((total + 1))

    expect=$(get_expect "$test_file")

    # Skip
    if [ "$expect" = "skip" ]; then
        skip_count=$((skip_count + 1))
        results_name+=("$test_file")
        results_status+=("SKIP")
        reason=$(get_reason "$test_file")
        results_detail+=("${reason}")
        printf "  %-35s SKIP" "$test_file"
        if [ -n "$reason" ]; then
            printf "  (%s)" "$reason"
        fi
        echo ""
        continue
    fi

    # Run the test
    printf "  %-35s " "$test_file"

    set +e
    test_output=$(bash "$test_script" 2>&1)
    test_rc=$?
    set -e

    # Determine actual result
    if [ $test_rc -eq 0 ]; then
        actual="pass"
    else
        actual="fail"
    fi

    # Map actual vs expected
    detail=""
    if [ "$actual" = "pass" ] && [ "$expect" = "pass" ]; then
        status="PASS"
        pass_count=$((pass_count + 1))
    elif [ "$actual" = "fail" ] && [ "$expect" = "xfail" ]; then
        status="XFAIL"
        xfail_count=$((xfail_count + 1))
        reason=$(get_reason "$test_file")
        detail="$reason"
    elif [ "$actual" = "pass" ] && [ "$expect" = "xfail" ]; then
        status="XPASS"
        xpass_count=$((xpass_count + 1))
        detail="expected xfail but passed"
    elif [ "$actual" = "fail" ] && [ "$expect" = "pass" ]; then
        status="FAIL"
        fail_count=$((fail_count + 1))
        # Extract fail message from output
        detail=$(echo "$test_output" | grep "FAIL:" | head -1 || true)
        if [ -z "$detail" ]; then
            detail="rc=$test_rc"
        fi
    else
        status="FAIL"
        fail_count=$((fail_count + 1))
        detail="unexpected: actual=$actual expect=$expect"
    fi

    results_name+=("$test_file")
    results_status+=("$status")
    results_detail+=("$detail")

    printf "%s" "$status"
    if [ -n "$detail" ]; then
        printf "  (%s)" "$detail"
    fi
    echo ""

    # Verbose: show test output
    if [ "$VERBOSE" -ge 1 ] && { [ "$status" = "FAIL" ] || [ "$status" = "XPASS" ]; }; then
        echo "    --- output ---"
        printf '%s\n' "${test_output//$'\n'/$'\n'    }"
        echo "    --- end ---"
    fi

    # Stop on unexpected failure unless -k
    if [ "$status" = "FAIL" ] && [ "$KEEP_GOING" = "false" ]; then
        echo ""
        echo "Stopping on unexpected failure. Use -k to keep going."
        break
    fi
done

# --- Summary ---
echo ""
echo "=============================================="
echo "Summary: $total tests"
echo "  PASS:  $pass_count"
echo "  XFAIL: $xfail_count"
echo "  XPASS: $xpass_count"
echo "  FAIL:  $fail_count"
echo "  SKIP:  $skip_count"
echo "=============================================="

# Exit code: 0 if no unexpected results (FAIL or XPASS)
if [ $fail_count -gt 0 ] || [ $xpass_count -gt 0 ]; then
    exit 1
fi
exit 0
