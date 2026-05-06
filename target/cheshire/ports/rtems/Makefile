# Paths
RTEMS_DIR        ?= /data2/fseratini/rtems-project/rtems
RTEMS_TOOLS      ?= /data2/fseratini/rtems-project/compiler-strict
BSP_DIR          := $(CURDIR)
PREFIX           := $(BSP_DIR)/install
RTEMS_BSP        := riscv/rv64imafdc

.PHONY: all patch configure build install clean

all: install

# Copia i file BSP nella repo RTEMS
patch:
	@echo "Applying Cheshire BSP files to RTEMS..."
	install -D $(BSP_DIR)/cheshire.dts \
	    $(RTEMS_DIR)/bsps/riscv/riscv/dts/cheshire.dts
	install -D $(BSP_DIR)/cheshire-dtb.h \
	    $(RTEMS_DIR)/bsps/riscv/riscv/include/bsp/cheshire-dtb.h
	install -D $(BSP_DIR)/optdtb.yml \
	    $(RTEMS_DIR)/spec/build/bsps/optdtb.yml
	install -D $(BSP_DIR)/optdtbheaderpath.yml \
	    $(RTEMS_DIR)/spec/build/bsps/optdtbheaderpath.yml
	install -D $(BSP_DIR)/optfdtuboot.yml \
	    $(RTEMS_DIR)/spec/build/bsps/optfdtuboot.yml
	install -D $(BSP_DIR)/optramsize.yml \
	    $(RTEMS_DIR)/spec/build/bsps/riscv/optramsize.yml
	install -D $(BSP_DIR)/bsprv64imafdc.yml \
	    $(RTEMS_DIR)/spec/build/bsps/riscv/riscv/bsprv64imafdc.yml
	install -D $(BSP_DIR)/start.S \
	    $(RTEMS_DIR)/bsps/riscv/shared/start/start.S

# Configura RTEMS
configure: patch
	@echo "Generating config.ini for $(RTEMS_BSP)..."
	@echo "[$(RTEMS_BSP)]" > $(RTEMS_DIR)/config.ini
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(RTEMS_BSP) \
	    --rtems-tools=$(RTEMS_TOOLS)

# Compila RTEMS
build: configure
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf

# Installa RTEMS
install: build
	cd $(RTEMS_DIR) && \
	PATH=$(RTEMS_TOOLS)/bin:$$PATH \
	./waf install

clean:
	rm -rf $(PREFIX)
	rm -f $(RTEMS_DIR)/config.ini
	cd $(RTEMS_DIR) && ./waf clean || true
