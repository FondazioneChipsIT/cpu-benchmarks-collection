#!/bin/bash
source /etc/profile.d/modules.sh 2>/dev/null || true
module load vivado_lab/2025.1 2>/dev/null || true
module load openocd/0.12.0 2>/dev/null || true
module load gcc/riscv64-15.1.0 2>/dev/null || true

cd /home/fseratini/fseratini/cpu-benchmarks-collection

GDB_PORT=${GDB_PORT:-${1:-3333}}
UART_DEV=${UART_DEV:-${2:-/dev/ttyUSB0}}
SINGLE_BENCH=${3:-}

SPM_SIZE=$((128 * 1024))
SPM_STACK=$((32 * 1024))
SPM_HEAP=$((32  * 1024))

# Valori della build DRAM attuale — devono corrispondere a libtarg.c
DRAM_STACK=$((64  * 1024))
DRAM_HEAP=$((2 * 1024 * 1024))

printf "%-30s %12s %12s %12s %s\n" \
    "BENCHMARK" "STACK_USED" "HEAP_USED" "SPM_FPRINT" "SPM_128K?"
echo "──────────────────────────────────────────────────────────────────"

if [ -n "$SINGLE_BENCH" ]; then
    BENCH_LIST="$SINGLE_BENCH"
else
    BENCH_LIST=$(ls benchmarks/deps/bringup-bench | sort)
fi

for bench in $BENCH_LIST; do
    elf="build/cheshire/bringup-bench/$bench/$bench.dram.elf"
    if [ ! -f "$elf" ]; then
        printf "%-30s %12s\n" "$bench" "SKIP (no ELF)"
        continue
    fi

    read text data bss <<< $(riscv64-unknown-elf-size "$elf" 2>/dev/null | \
        awk 'NR==2 {print $1, $2, $3}')

    # Codice puro: rimuovi stack e heap della build DRAM dal BSS
    bss_pure=$((bss - DRAM_STACK - DRAM_HEAP))
    [ $bss_pure -lt 0 ] && bss_pure=0

    # Footprint SPM = codice puro + stack SPM + heap SPM
    footprint_spm=$((text + data + bss_pure + SPM_STACK + SPM_HEAP))

    case "$bench" in
        anagram|lz-compress|pi-calc) T=50 ;;
        c-interp|heldkarp-tsp)       T=50 ;;
        *)                           T=20 ;;
    esac

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    "$SCRIPT_DIR/run_test.sh" "$bench" "$GDB_PORT" "$UART_DEV" "$T" \
        >/dev/null 2>&1 || true

    OUT="/tmp/${bench}_uart.txt"
    stack=$(grep -o "STACK_USED: *[0-9]*" "$OUT" | awk -F: '{print $2}')
    heap=$(grep -o "HEAP_USED: *[0-9]*" "$OUT" | awk -F: '{print $2}')

    # Normalizza valori (rimuove CR e spazi)
    stack=$(echo "$stack" | tr -d '\r' | xargs)
    heap=$(echo "$heap" | tr -d '\r' | xargs)

    sed -i '/^# STACK_USED:\|^# HEAP_USED:/d' "$OUT" 2>/dev/null || true

    stack=${stack:-"N/A"}
    heap=${heap:-"N/A"}

    # --- 1) Footprint SPM già fuori budget ---
    if [ $footprint_spm -gt $SPM_SIZE ]; then
        spm="❌ NO  (spm_footprint=$((footprint_spm/1024))KB > 128KB)"

    # --- 2) Misure runtime disponibili ---
    elif [[ "$stack" =~ ^[0-9]+$ ]] && [[ "$heap" =~ ^[0-9]+$ ]]; then
        stack_ok=1; heap_ok=1
        [ $stack -gt $SPM_STACK ] && stack_ok=0
        [ $heap  -gt $SPM_HEAP  ] && heap_ok=0

        if [ $stack_ok -eq 1 ] && [ $heap_ok -eq 1 ]; then
            spm="✅ YES (stack=${stack}B heap=${heap}B spm_fprint=${footprint_spm}B)"
        else
            reasons=""
            [ $stack_ok -eq 0 ] && \
                reasons="${reasons}stack_used=${stack}B > spm_limit=${SPM_STACK}B; "
            [ $heap_ok  -eq 0 ] && \
                reasons="${reasons}heap_used=${heap}B > spm_limit=${SPM_HEAP}B"
            spm="❌ NO  ($reasons)"
        fi

    # --- 3) Footprint SPM OK ma nessuna misura runtime ---
    else
        spm="⚠️  N/A (spm_fprint=${footprint_spm}B OK, no runtime data — crash/timeout)"
    fi

    printf "%-30s %12s %12s %12d %s\n" \
        "$bench" "$stack" "$heap" "$footprint_spm" "$spm"

    sleep 2
done