#!/bin/bash
# Build RTEMS toolchain with -mstrict-align for CVA6/Cheshire
set -e

RSB_DIR=${1:-$(pwd)/rsb-strict}
PREFIX=${2:-$(pwd)/compiler-strict}
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

echo "[TOOLCHAIN] Starting build of strict-align toolchain..."

echo "[TOOLCHAIN] Cloning RTEMS Source Builder..."
if [ ! -d "$RSB_DIR" ]; then
    git clone https://github.com/RTEMS/rtems-source-builder.git "$RSB_DIR"
    cd "$RSB_DIR"
    git checkout c282e2402dffd81a0bf03c1f72a05a51b7810517
else
    cd "$RSB_DIR"
fi

echo "[TOOLCHAIN] Applying strict-align patch..."
CFG="$RSB_DIR/rtems/config/tools/rtems-gcc-15.2-newlib-head.cfg"
if ! grep -q "mstrict-align" "$CFG"; then
    sed -i 's/%include %{_configdir}\/gcc-14.cfg/%define gcc_configure_extra_options CFLAGS_FOR_TARGET="-mstrict-align -g -O2"\n%include %{_configdir}\/gcc-14.cfg/' "$CFG"
    echo "[TOOLCHAIN] Patch applied."
else
    echo "[TOOLCHAIN] Patch already applied, skipping."
fi

echo "[TOOLCHAIN] Creating custom build set without SIS..."
cat > "$RSB_DIR/rtems/config/7/rtems-riscv-nosis.bset" << 'BSET'
%define release 1
%define rtems_arch riscv
%define with_libgomp
%define with_newlib_tls
%include 7/rtems-default.bset
BSET

echo "[TOOLCHAIN] Building toolchain (Direct CFG with Target)..."
cd "$RSB_DIR/source-builder"
./sb-set-builder \
    --prefix="$PREFIX" \
    --log="${PREFIX}-build.log" \
    7/rtems-riscv-nosis

echo "[TOOLCHAIN] Done. Toolchain installed in $PREFIX"
