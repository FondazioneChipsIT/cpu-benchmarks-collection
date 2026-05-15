# ============================================================
# Cheshire RTEMS BSP — ports/rtems/Makefile
# ============================================================
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

BSP_DIR          := $(ROOT)/target/cheshire/ports/rtems

RTEMS_DIR        ?= $(ROOT)/benchmarks/deps/rtems
RTEMS_TOOLS      ?= $(ROOT)/target/cheshire/ports/rtems/toolchain/compiler-strict
PREFIX           ?= $(BSP_DIR)/install
RTEMS_BSP        := riscv/rv64imafdc

DTC              := dtc
PYTHON           := python3

DTS_SRC          := $(BSP_DIR)/cheshire.dts
DTB_OUT          := $(BSP_DIR)/cheshire.dtb
DTB_HEADER       := $(BSP_DIR)/cheshire-dtb.h
GEN_SCRIPT       := $(BSP_DIR)/gen_dtb_header.py

.DEFAULT_GOAL := all

CONFIG_INI := $(RTEMS_DIR)/config.ini

# ============================================================
# Generazione DTB e header
# ============================================================
$(DTB_OUT): $(DTS_SRC)
	@echo "[DTB] Compiling DTS -> DTB"
	$(DTC) -I dts -O dtb -o $@ $<

$(DTB_HEADER): $(DTB_OUT) $(GEN_SCRIPT)
	@echo "[DTB] Generating C header from DTB"
	$(PYTHON) $(GEN_SCRIPT) $< $@
# ============================================================
# RTEMS config.ini
# ============================================================
CONFIG_INI := $(RTEMS_DIR)/config.ini

$(CONFIG_INI):
	@echo "[riscv/rv64imafdc]" > $@
	
# ============================================================
# Patch RTEMS
# ============================================================
.PHONY: patch
patch: $(DTB_HEADER)
	@echo "[BSP] Copying Cheshire BSP files into RTEMS..."
	install -Dm644 $(BSP_DIR)/cheshire.dts \
		$(RTEMS_DIR)/bsps/riscv/riscv/dts/cheshire.dts
	install -Dm644 $(DTB_HEADER) \
		$(RTEMS_DIR)/bsps/riscv/riscv/include/bsp/cheshire-dtb.h
	install -Dm644 $(BSP_DIR)/optdtb.yml \
		$(RTEMS_DIR)/spec/build/bsps/optdtb.yml
	install -Dm644 $(BSP_DIR)/optdtbheaderpath.yml \
		$(RTEMS_DIR)/spec/build/bsps/optdtbheaderpath.yml
	install -Dm644 $(BSP_DIR)/optfdtuboot.yml \
		$(RTEMS_DIR)/spec/build/bsps/optfdtuboot.yml
	install -Dm644 $(BSP_DIR)/optramsize.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/optramsize.yml
	install -Dm644 $(BSP_DIR)/bsprv64imafdc.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/riscv/bsprv64imafdc.yml
	install -Dm644 $(BSP_DIR)/opthtif.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/riscv/opthtif.yml
	install -Dm644 $(BSP_DIR)/start.S \
		$(RTEMS_DIR)/bsps/riscv/shared/start/start.S

# ============================================================
# Build RTEMS
# ============================================================


.PHONY: configure
configure: $(CONFIG_INI) patch
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf configure \
		--prefix=$(PREFIX) \
		--rtems-bsps=$(RTEMS_BSP) \
		--rtems-tools=$(RTEMS_TOOLS)

.PHONY: build
build: configure
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf

.PHONY: install
install: build
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf install

.PHONY: all
all: install

# ============================================================
# Clean
# ============================================================
.PHONY: clean
clean:
	rm -f $(DTB_OUT) $(DTB_HEADER)
	rm -rf $(PREFIX)

.PHONY: distclean
distclean: clean
	cd $(RTEMS_DIR) && ./waf clean || true