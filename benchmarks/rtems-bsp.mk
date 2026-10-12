# ============================================================
# RTEMS BSP 
# ============================================================
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

NUM_CORES ?= 2

BSP_DIR          ?= $(ROOT)/target/$(PLATFORM)/ports/rtems
RTEMS_DIR        ?= $(ROOT)/benchmarks/deps/rtems
RTEMS_TOOLS      ?= $(ROOT)/target/cheshire/ports/rtems/toolchain/compiler-strict
PREFIX           ?= $(BSP_DIR)/install
RTEMS_BSP        := riscv/rv64imafdc

DTC              := dtc
PYTHON           := python3

ifeq ($(findstring astral,$(BSP_DIR)),astral)
    DTS_BASENAME := astral
else
    DTS_BASENAME := cheshire
endif

DTS_SRC         := $(BSP_DIR)/$(DTS_BASENAME).dts
DTB_OUT         := $(BSP_DIR)/$(DTS_BASENAME).dtb
DTB_HEADER      := $(BSP_DIR)/$(DTS_BASENAME)-dtb.h
GEN_SCRIPT      := $(BSP_DIR)/gen_dtb_header.py
GEN_SCRIPT       := $(BSP_DIR)/gen_dtb_header.py

.DEFAULT_GOAL := all

# ============================================================
# DTB & header generation
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

ifeq ($(shell test $(NUM_CORES) -gt 1 && echo yes),yes)
    RTEMS_SMP_VAL := True
else
    RTEMS_SMP_VAL := False
endif

.PHONY: gen-config-ini
gen-config-ini:
	@echo "[$(RTEMS_BSP)]" > $(CONFIG_INI)
	@echo "RTEMS_SMP = $(RTEMS_SMP_VAL)" >> $(CONFIG_INI)
	@echo "BUILD_TESTS = False" >> $(CONFIG_INI)
	@echo "BUILD_SAMPLES = False" >> $(CONFIG_INI)
	@echo "RTEMS_DEBUG = False" >> $(CONFIG_INI)
	@echo "LDFLAGS = -Wl,--gc-sections" >> $(CONFIG_INI)
	@echo "[CONFIG] Written $(CONFIG_INI) with RTEMS_SMP = $(RTEMS_SMP_VAL)"
	@echo "RISCV_RAM_REGION_BEGIN = 0x10000000" >> $(CONFIG_INI)
	@echo "RISCV_RAM_REGION_SIZE = 0x00020000" >> $(CONFIG_INI)
	@echo "RISCV_L2_REGION_BEGIN = 0x78000000" >> $(CONFIG_INI)
	@echo "RISCV_L2_REGION_SIZE = 0x00010000" >> $(CONFIG_INI)	
	@echo "RISCV_WORK_REGION_BEGIN = 0x50000000" >> $(CONFIG_INI)
	@echo "RISCV_WORK_REGION_SIZE = 0x00020000" >> $(CONFIG_INI)	

# ============================================================
# Patch RTEMS
# ============================================================
.PHONY: patch
patch: $(DTB_HEADER)
	@echo "[BSP] Copying Cheshire BSP files into RTEMS..."
	install -Dm644 $(DTS_SRC) \
		$(RTEMS_DIR)/bsps/riscv/riscv/dts/$(notdir $(DTS_SRC))
	install -Dm644 $(DTB_HEADER) \
		$(RTEMS_DIR)/bsps/riscv/riscv/include/bsp/$(notdir $(DTB_HEADER))
	install -Dm644 $(BSP_DIR)/optdtb.yml \
		$(RTEMS_DIR)/spec/build/bsps/optdtb.yml
	install -Dm644 $(BSP_DIR)/optdtbheaderpath.yml \
		$(RTEMS_DIR)/spec/build/bsps/optdtbheaderpath.yml
	install -Dm644 $(BSP_DIR)/optfdtuboot.yml \
		$(RTEMS_DIR)/spec/build/bsps/optfdtuboot.yml
	install -Dm644 $(BSP_DIR)/optramsize.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/optramsize.yml
	install -Dm644 $(BSP_DIR)/optrambegin.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/optrambegin.yml
	install -Dm644 $(BSP_DIR)/bsprv64imafdc.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/riscv/bsprv64imafdc.yml
	install -Dm644 $(BSP_DIR)/opthtif.yml \
		$(RTEMS_DIR)/spec/build/bsps/riscv/riscv/opthtif.yml
	install -Dm644 $(BSP_DIR)/start.S \
		$(RTEMS_DIR)/bsps/riscv/shared/start/start.S
	install -Dm644 $(BSP_DIR)/init.c \
 		$(RTEMS_DIR)/testsuites/samples/hello/init.c
	install -Dm644 $(BSP_DIR)/linkcmds.yml \
 		$(RTEMS_DIR)/spec/build/bsps/riscv/linkcmds.yml
	install -Dm644 $(BSP_DIR)/optworkbegin.yml \
 		$(RTEMS_DIR)/spec/build/bsps/riscv/optworkbegin.yml
	install -Dm644 $(BSP_DIR)/optworksize.yml \
 		$(RTEMS_DIR)/spec/build/bsps/riscv/optworksize.yml
	install -Dm644 $(BSP_DIR)/optL2begin.yml \
 		$(RTEMS_DIR)/spec/build/bsps/riscv/optL2begin.yml
	install -Dm644 $(BSP_DIR)/optL2size.yml \
 		$(RTEMS_DIR)/spec/build/bsps/riscv/optL2size.yml
	install -Dm644 $(BSP_DIR)/bspgetworkarea.c \
 		$(RTEMS_DIR)/bsps/riscv/shared/start/bspgetworkarea.c
		

# ============================================================
# Configure — run waf configure (depends on patch + config.ini)
# ============================================================		
.PHONY: configure
configure: gen-config-ini patch
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf configure \
		--prefix=$(PREFIX) \
		--rtems-bsps=$(RTEMS_BSP) \
		--rtems-tools=$(RTEMS_TOOLS) 

# ============================================================
# Build / install
# ============================================================
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