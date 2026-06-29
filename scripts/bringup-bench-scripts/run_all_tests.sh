#!/bin/bash
# run_all_tests.sh — runs all bringup-bench benchmarks

source /etc/profile.d/modules.sh 2>/dev/null || true
module load vivado_lab/2025.1 2>/dev/null || true
module load openocd/0.12.0 2>/dev/null || true
module load gcc/riscv64-15.1.0 2>/dev/null || true

# Default parameters 
: ${GDB_PORT:=3333}
: ${UART_DEV:=/dev/ttyUSB0}
: ${TEST_TIMEOUT:=20}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LIST=$(ls $ROOT/benchmarks/deps/bringup-bench)

PASS=0
FAIL=0
SKIP=0

for B in $LIST; do
    # Cleanup UART port before each test
    OLD_PICO=$(lsof -t "$UART_DEV" 2>/dev/null)
    [ -n "$OLD_PICO" ] && kill "$OLD_PICO" 2>/dev/null || true

    case "$B" in
        anagram|c-interp|lz-compress) T=50 ;;
        *) T=$TEST_TIMEOUT ;;
    esac

    "$SCRIPT_DIR/run_test.sh" "$B" "$GDB_PORT" "$UART_DEV" "$T"
    STATUS=$?
    if [ $STATUS -eq 0 ]; then PASS=$((PASS+1)); fi
    if [ $STATUS -eq 1 ]; then FAIL=$((FAIL+1)); fi
    if [ $STATUS -eq 2 ]; then SKIP=$((SKIP+1)); fi

    # Pause between tests to ensure the UART port is released
    sleep 2
done

echo ""
echo "═══════════════════════════════════════"
echo "FINAL RESULTS"
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "SKIP: $SKIP"
echo "TOTAL: $((PASS+FAIL+SKIP))"
