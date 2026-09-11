# OpenJazz — openfpgaOS custom core
#
# This Makefile builds a CUSTOM CORE: a standalone openFPGA core
# wrapping a single app. It owns its own dist/, build/, and ZIP.
#
# Targets:
#   make              Build the custom core
#   make debug        Build, push via UART, stream console
#   make copy         Copy this custom core to Pocket SD
#   make package      Package this custom core into a ZIP
#   make test         Test on desktop (SDL2)
#   make clean        Remove build artifacts
#

APP       = openjazz
# Platform target: pocket (APF SD tree under build/pocket/<app>/) or
# mister (disk-image bundle under build/mister/<app>/). Overridable per
# invocation: `make TARGET=mister`, or from the SDK root Makefile.
TARGET   ?= pocket
# MiSTer network target for `make copy TARGET=mister` (override: MISTER_IP=...).
MISTER_IP ?= mister.local
SDK_DIR   = ../sdk
ROOT      = $(realpath $(CURDIR)/../..)
OBJ_DIR   = $(ROOT)/.obj/$(APP)
BUILD_DIR = $(OBJ_DIR)
DIST      = $(ROOT)/dist/$(APP)
OUT       = $(ROOT)/build/$(TARGET)/$(APP)
RUNTIME   = $(ROOT)/runtime

SRCS     = $(wildcard *.c)
SRCS_CXX = $(wildcard *.cpp)

# Data files bundled into the per-target deliverable (override per core).
MISTER_ASSETS ?= $(wildcard *.mid *.mod *.wav *.dat *.png *.bin *.cfg *.iso)

# Declare default target before sdk.mk's rules
.DEFAULT_GOAL := all

include $(SDK_DIR)/sdk.mk

$(CRT_DIR)/start.o: $(CRT_DIR)/start.S
	$(AS) $(ASFLAGS) -c -o $@ $<

# Build ELF, then assemble SD card image in build/<app>/
all: $(OBJ_DIR)/app.elf release
	@$(SIZE) $<

# Assemble the per-target deliverable.  Each platform owns its layout
# (pocket = APF tree, mister = disk image); the rule names no target —
# adding a target = add platforms/<target>/image.sh.
release: $(OBJ_DIR)/app.elf
	@$(SDK_DIR)/platforms/$(TARGET)/image.sh "$(APP)" "$(OBJ_DIR)/app.elf" "$(ROOT)" "$(DIST)" $(MISTER_ASSETS)

debug: all
	@$(ROOT)/scripts/debug.sh $$(ls -d $(OUT)/Assets/*/common 2>/dev/null)/$(APP).elf

# Deploy to the device.  MISTER_IP rides the environment so the same
# invocation serves every platform's copy.sh (pocket ignores it and
# auto-detects the SD card; mister uses it as the network host).
copy: all
	@MISTER_IP="$(MISTER_IP)" $(SDK_DIR)/platforms/$(TARGET)/copy.sh "$(APP)" "$(OBJ_DIR)/app.elf"

package: all
	@$(ROOT)/scripts/package.sh $(APP)

test: app_pc

clean: sdk-clean
	rm -rf $(ROOT)/build/pocket/$(APP) $(ROOT)/build/mister/$(APP)

.PHONY: all release debug copy package test clean
