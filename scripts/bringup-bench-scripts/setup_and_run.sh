#!/bin/bash
source /etc/profile.d/modules.sh 2>/dev/null || true
module load vivado_lab/2025.1 2>/dev/null || true
module load openocd/0.12.0 2>/dev/null || true
module load gcc/riscv64-15.1.0 2>/dev/null || true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Tutte le variabili vengono dal Makefile via environment
# In caso di lancio diretto, usa questi default
: ${FPGA_SCRIPTS:=~/fseratini/riscv_fpga_scripts}
: ${USB_SERIAL:=200300BB8B7BB}
: ${DEVICE:=xc7k325t_0}
: ${PROJECT_NAME:=~/fseratini/bit_under_test/cheshire.genesys2}
: ${OPENOCD_SCRIPT:=$FPGA_SCRIPTS/scripts_openocd/openocd.genesys2.noshutdown.tcl}
: ${VIVADO_PORT:=3334}
: ${GDB_PORT:=3333}
: ${UART_DEV:=/dev/ttyUSB0}
: ${TEST_TIMEOUT:=20}

echo "=== Configurazione ==="
echo "  FPGA_SCRIPTS  = $FPGA_SCRIPTS"
echo "  USB_SERIAL    = $USB_SERIAL"
echo "  DEVICE        = $DEVICE"
echo "  PROJECT_NAME  = $PROJECT_NAME"
echo "  OPENOCD_SCRIPT= $OPENOCD_SCRIPT"
echo "  GDB_PORT      = $GDB_PORT"
echo "  UART_DEV      = $UART_DEV"
echo "  TEST_TIMEOUT  = $TEST_TIMEOUT"

# 1) Programma la board
echo "=== Programmazione FPGA ==="
pkill hw_server 2>/dev/null || true
sleep 1
make -C $FPGA_SCRIPTS program \
    DEVICE=$DEVICE \
    PROJECT_NAME=$PROJECT_NAME \
    USB_SERIAL=$USB_SERIAL \
    PORT=$VIVADO_PORT
echo "=== FPGA programmata ==="

# 2) Lancia OpenOCD in background
echo "=== Avvio OpenOCD ==="
pkill openocd 2>/dev/null || true
sleep 1
openocd \
    -c "set CoreNames {cheshire}" \
    -c "set serial $USB_SERIAL" \
    -c "set gdb_port $GDB_PORT" \
    -f $OPENOCD_SCRIPT \
    >/tmp/openocd.log 2>&1 &
sleep 4
echo "=== OpenOCD avviato (log: /tmp/openocd.log) ==="

# 3) Lancia i test
echo "=== Avvio test ==="
bash "$SCRIPT_DIR/run_all_tests.sh" $GDB_PORT $UART_DEV $TEST_TIMEOUT
