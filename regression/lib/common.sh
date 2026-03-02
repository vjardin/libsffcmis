#!/bin/bash
# regression/lib/common.sh — Shared helpers for regression tests
# SPDX-License-Identifier: GPL-2.0

set -euo pipefail

# --- Paths ---
REGRESSION_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$(cd "$REGRESSION_DIR/.." && pwd)"
SFF_BIN="${SFF_BIN:-$PROJECT_DIR/test/sffcmis_test}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-$PROJECT_DIR/src}"

# --- Environment (set by run_all.sh) ---
BUS="${BUS:-0}"
BANK="${BANK:-0}"
LANE="${LANE:-0}"
DEBUG="${DEBUG:-0}"

# --- Per-test state ---
CMD_OUT=""
CMD_ERR=""
CMD_RC=0
JSON_TMP=""

_test_passed=false
_test_msg=""

# --- Core execution helpers ---

# Run sffcmis_test with arbitrary arguments.
# Captures stdout -> CMD_OUT, stderr -> CMD_ERR, exit code -> CMD_RC.
sff_cmd() {
    local out_file err_file
    out_file=$(mktemp)
    err_file=$(mktemp)

    set +e
    "$SFF_BIN" -d "$DEBUG" -b "$BANK" "$@" "$BUS" >"$out_file" 2>"$err_file"
    CMD_RC=$?
    set -e

    CMD_OUT=$(cat "$out_file")
    CMD_ERR=$(cat "$err_file")
    rm -f "$out_file" "$err_file"
}

# Run sffcmis_test with -j (JSON output) and store parsed JSON in JSON_TMP.
sff_json() {
    local out_file err_file
    out_file=$(mktemp)
    err_file=$(mktemp)

    set +e
    "$SFF_BIN" -j -d "$DEBUG" -b "$BANK" "$@" "$BUS" >"$out_file" 2>"$err_file"
    CMD_RC=$?
    set -e

    CMD_OUT=$(cat "$out_file")
    CMD_ERR=$(cat "$err_file")
    JSON_TMP="$out_file"
    rm -f "$err_file"
}

# Extract a field from the last sff_json output using jq.
jq_field() {
    local expr="$1"
    if [ -z "$JSON_TMP" ] || [ ! -f "$JSON_TMP" ]; then
        echo "ERROR: no JSON_TMP file"
        return 1
    fi
    jq -r "$expr" "$JSON_TMP"
}

# --- Assertion helpers ---

assert_rc() {
    local expected="$1"
    local msg="${2:-exit code check}"
    if [ "$CMD_RC" -ne "$expected" ]; then
        fail "$msg: expected rc=$expected, got rc=$CMD_RC"
        return 1
    fi
}

assert_eq() {
    local actual="$1"
    local expected="$2"
    local msg="${3:-equality check}"
    if [ "$actual" != "$expected" ]; then
        fail "$msg: expected '$expected', got '$actual'"
        return 1
    fi
}

assert_json() {
    local jq_expr="$1"
    local expected="$2"
    local msg="${3:-JSON field check}"
    local actual
    actual=$(jq_field "$jq_expr")
    if [ "$actual" != "$expected" ]; then
        fail "$msg: jq($jq_expr) expected '$expected', got '$actual'"
        return 1
    fi
}

assert_output_contains() {
    local pattern="$1"
    local msg="${2:-output contains check}"
    if ! echo "$CMD_OUT" | grep -q "$pattern"; then
        fail "$msg: output does not contain '$pattern'"
        return 1
    fi
}

# --- Result helpers ---

pass() {
    _test_passed=true
    _test_msg="${1:-}"
}

fail() {
    _test_passed=false
    _test_msg="$1"
    if [ "${VERBOSE:-0}" -ge 1 ]; then
        echo "  FAIL: $1" >&2
        if [ -n "$CMD_ERR" ]; then
            echo "  stderr: $CMD_ERR" >&2
        fi
    fi
}

# --- Cleanup ---

_cleanup_files=()

cleanup_register() {
    _cleanup_files+=("$@")
}

cleanup_run() {
    for f in "${_cleanup_files[@]+"${_cleanup_files[@]}"}"; do
        rm -f "$f"
    done
    if [ -n "$JSON_TMP" ] && [ -f "$JSON_TMP" ]; then
        rm -f "$JSON_TMP"
    fi
}

trap cleanup_run EXIT

# --- Utility ---

# Sleep helper for EEPROM write settling.
eeprom_delay() {
    local ms="${EEPROM_WRITE_DELAY_MS:-500}"
    sleep "$(echo "scale=3; $ms / 1000" | bc)"
}
