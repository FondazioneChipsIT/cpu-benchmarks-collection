#!/bin/bash
BENCH=$1

# Load required modules
source /etc/profile.d/modules.sh 2>/dev/null || true
module load vivado_lab/2025.1 2>/dev/null || true
module load openocd/0.12.0 2>/dev/null || true
module load gcc/riscv64-15.1.0 2>/dev/null || true

# Set parameters
GDB_PORT=${2:-$GDB_PORT}
UART_DEV=${3:-$UART_DEV}
TIMEOUT=${4:-$TEST_TIMEOUT}

: ${FPGA_SCRIPTS:=~/fseratini/riscv_fpga_scripts}
: ${USB_SERIAL:=200300BB8B7BB}
: ${DEVICE:=xc7k325t_0}
: ${PROJECT_NAME:=~/fseratini/bit_under_test/cheshire.genesys2}
: ${VIVADO_PORT:=3334}


BAUD=115200
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ELF="$ROOT/build/cheshire/bringup-bench/$BENCH/$BENCH.spm.elf"
REF="$ROOT/benchmarks/deps/bringup-bench/$BENCH/$BENCH.out"

OUT="/tmp/${BENCH}_uart.txt"
RAW="/tmp/${BENCH}_uart_raw.txt"

if [ -z "$BENCH" ]; then
    echo "Uso: $0 <benchmark>"
    exit 1
fi

if [ ! -f "$ELF" ]; then
    echo "SKIP: ELF non trovato: $ELF"
    exit 2
fi

rm -f "$RAW" "$OUT"

# Cleanup any previous picocom instance
OLD_PICO=$(lsof -t "$UART_DEV" 2>/dev/null)
if [ -n "$OLD_PICO" ]; then
    kill "$OLD_PICO" 2>/dev/null || true
    sleep 1
fi

# Reset hardware via VIO (without touching OpenOCD)

make -C $FPGA_SCRIPTS set_vio \
    VIO_PROBE=reset \
    VIO_COMMAND=1 \
    USB_SERIAL=$USB_SERIAL \
    DEVICE=$DEVICE \
    PROJECT_NAME=$PROJECT_NAME \
    PORT=$VIVADO_PORT \
    >/dev/null 2>&1 || true
sleep 3

# 1) Start picocom in background BEFORE launching GDB
picocom -b $BAUD --imap lfcrlf \
    --logfile "$RAW" \
    --exit-after $((TIMEOUT * 1000)) \
    "$UART_DEV" \
    </dev/null >/dev/null 2>/dev/null &
PICOPID=$!
sleep 1

# 2) Estimate GDB load time based on ELF size
# JTAG load is slow (~2KB/sec), so estimate load duration
ELF_SIZE=$(stat -c%s "$ELF" 2>/dev/null || echo 100000)
# 2000 bytes/sec JTAG transfer rate, round up
GDB_LOAD_SLEEP=$(( (ELF_SIZE / 2000) + 10 ))
# Minimum 15s, maximum TIMEOUT-5
if [ $GDB_LOAD_SLEEP -lt 15 ]; then GDB_LOAD_SLEEP=15; fi

if [ $GDB_LOAD_SLEEP -gt $((TIMEOUT - 5)) ]; then 
    GDB_LOAD_SLEEP=$((TIMEOUT - 5)); 
fi

# 3) Launch GDB in background
riscv64-unknown-elf-gdb "$ELF" --batch \
    -ex "set remotetimeout 60" \
    -ex "target extended-remote :$GDB_PORT" \
    -ex "monitor reset halt" \
    -ex "load" \
    -ex "set \$pc = 0x10000000" \
    -ex "continue" \
    >/dev/null 2>&1 &
GDBPID=$!

# 4) Kill GDB after the estimated load time
# The core continues running autonomously after GDB is killed
sleep $GDB_LOAD_SLEEP
kill $GDBPID 2>/dev/null || true
wait $GDBPID 2>/dev/null || true

# 5) Wait for picocom to exit (exit-after timeout)
wait $PICOPID 2>/dev/null || true

# 6) Clean UART output
grep -v "picocom\|Terminal ready\|Type \[C-a\]\|port is\|baudrate\|parity\|databits\|stopbits\|txdelay\|escape is\|local echo\|noinit\|noreset\|hangup\|nolock\|send_cmd\|receive_cmd\|imap\|omap\|emap\|logfile\|initstring\|exit_after\|exit is\|minimal\|Terminating\|Thanks\|read zero\|STDIN\|flowcontrol\|\*\* read" \
    "$RAW" 2>/dev/null | sed 's/\r//g' > "$OUT" || true


# 6b) Remove memory measurement lines (MEASURE_MEMORY) before comparison
sed -i '/^# STACK_USED:\|^# HEAP_USED:/d' "$OUT" 2>/dev/null || true

# 7) Normalize final newline to match reference output
if [ -f "$REF" ]; then
    REF_LAST=$(tail -c1 "$REF" | xxd | awk '{print $2}')
    OUT_LAST=$(tail -c1 "$OUT" | xxd | awk '{print $2}')
    if [ "$REF_LAST" != "0a" ] && [ "$OUT_LAST" = "0a" ]; then
        truncate -s -1 "$OUT"
    elif [ "$REF_LAST" = "0a" ] && [ "$OUT_LAST" != "0a" ]; then
        printf '\n' >> "$OUT"
    fi
fi

# 8) Compare output
if [ ! -f "$REF" ]; then
    echo "⚠️  Nessun file .out per $BENCH"
    cat "$OUT"
    exit 2
fi

if diff -q "$REF" "$OUT" >/dev/null 2>&1; then
    echo "✅ PASS $BENCH"
    exit 0
else
    echo "❌ FAIL $BENCH"
    diff "$REF" "$OUT" || true
    exit 1
fi