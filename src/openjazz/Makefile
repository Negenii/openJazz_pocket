# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
# OpenJazz — openfpgaOS custom core (Analogue Pocket)
#
#   make                 build app.elf (in the container) + assemble build/pocket/openjazz/
#   make copy            deploy to the Pocket SD card
#   make package         zip the core into releases/pocket/
#   make test            desktop SDL2 build (./openjazz_pc) for fast iteration
#   make clean

APP       := openjazz
TARGET   ?= pocket
MISTER_IP ?= mister.local
ROOT      := $(realpath $(CURDIR)/../..)
SDK       := $(ROOT)/src/sdk
OJ        := $(CURDIR)/openjazz
PLAT      := $(CURDIR)/platform
OBJDIR    := $(ROOT)/.obj/$(APP)
DIST      := $(ROOT)/dist/$(APP)
OUT       := $(ROOT)/build/$(TARGET)/$(APP)
GAME_ASSETS = $(wildcard *.iso)

OJ_VERSION ?= 0.2.4-pocket

APPINC := -I$(OJ)/src -I$(OJ)/ext/argparse -I$(OJ)/ext/miniz -I$(OJ)/ext/xmp -I$(OJ)/ext/stb
DEFS   := -DOPENFPGA -DOJ_SDL2 -DPORTABLE -DSDL_MAIN_HANDLED -DOJ_VERSION='"$(OJ_VERSION)"' \
          -DMINIZ_NO_ARCHIVE_APIS -DLIBXMP_STATIC -DLIBXMP_CORE_PLAYER -DLIBXMP_CORE_DISABLE_IT -DHAVE_POWF \
          -DOF_SDL_NO_CONTROLLER_EVENTS

# Opt-in frame-time breakdown printed as a terminal overlay on the Pocket:
#   make PERF=1
ifeq ($(PERF),1)
DEFS += -DOF_PERF
endif

SRCS_CXX := $(shell find $(OJ)/src -name '*.cpp' -not -path '*/jj2/*' | sort)
SRCS_C   := $(OJ)/ext/argparse/argparse.c $(OJ)/ext/miniz/miniz.c $(OJ)/ext/stb/stb_rect_pack.c \
            $(shell find $(OJ)/ext/xmp/src -name '*.c' | sort) \
            $(PLAT)/of_libc_compat.c $(SDK)/of_init.c $(SDK)/of_sdl2.c $(PLAT)/of_sdl_extra.c

# Speed-critical TUs at -O3: xmp mixer + the blitters.
HOT_SRCS := $(OJ)/ext/xmp/src/mixer.c $(OJ)/ext/xmp/src/mix_all.c $(OJ)/ext/xmp/src/virtual.c \
            $(OJ)/ext/xmp/src/player.c $(OJ)/src/io/gfx/video.cpp $(OJ)/src/io/gfx/sprite.cpp \
            $(OJ)/src/io/gfx/anim.cpp $(OJ)/src/io/gfx/font.cpp

.DEFAULT_GOAL := all
EXCEPTIONS ?= 1
# Optimisation level: -O2, kept deliberately. A clean A/B on hardware (same
# sources, only this flag differing) measured -O3 as SLOWER -- 29-30 fps in a
# level against 31-33 at -O2 -- because -O3's extra inlining and unrolling
# grew the text by ~4% and this core has only a 32 KB instruction cache.
# Override per build with `make BASE_OPT=-O3` if that ever changes.
BASE_OPT ?= -O2
include $(CURDIR)/cxx.mk
# cxx.mk defines CXXFLAGS with ':=', so this appends after it's set: force
# of_sdl_extra.h (SDL2 macros/decls the shim doesn't provide) into every
# C++ TU without OpenJazz sources referencing it directly (upstream can't
# take a #include of a platform-only header).
CXXFLAGS += -include $(PLAT)/of_sdl_extra.h

all: app.elf release

release: app.elf
	@$(SDK)/platforms/$(TARGET)/image.sh "$(APP)" "app.elf" "$(ROOT)" "$(DIST)" $(GAME_ASSETS)

copy: all
	@MISTER_IP="$(MISTER_IP)" $(SDK)/platforms/$(TARGET)/copy.sh "$(APP)" "app.elf"

# The release ZIP must not carry game data: players build jazz.iso from their
# own Jazz Jackrabbit files with tools/mkjazz.sh. Stage without $(GAME_ASSETS)
# for packaging only; `make` and `make copy` still include it so the SD card
# is ready to play during development.
package: app.elf
	@$(SDK)/platforms/$(TARGET)/image.sh "$(APP)" "app.elf" "$(ROOT)" "$(DIST)"
	@$(ROOT)/scripts/package.sh $(APP)

# ── Desktop build: real SDL2 from Homebrew, same sources, same platform file ──
PC_CXX  ?= c++
PC_CC   ?= cc
PC_OBJDIR := $(ROOT)/.obj/$(APP)-pc
PC_SDL_CFLAGS := $(shell sdl2-config --cflags)
PC_SDL_LIBS   := $(shell sdl2-config --libs)
PC_DEFS := -DOF_PC -DOPENFPGA -DOJ_SDL2 -DPORTABLE -DSDL_MAIN_HANDLED -DOJ_VERSION='"$(OJ_VERSION)-pc"' \
           -DMINIZ_NO_ARCHIVE_APIS -DLIBXMP_STATIC -DLIBXMP_CORE_PLAYER -DLIBXMP_CORE_DISABLE_IT -DHAVE_POWF
PC_SRCS_CXX := $(shell find $(OJ)/src -name '*.cpp' -not -path '*/jj2/*' | sort)
PC_SRCS_C   := $(OJ)/ext/argparse/argparse.c $(OJ)/ext/miniz/miniz.c $(OJ)/ext/stb/stb_rect_pack.c \
               $(shell find $(OJ)/ext/xmp/src -name '*.c' | sort)

# Desktop-only heap-peak instrumentation, opt-in: `make test MEMSTAT=1`.
# of_memstat.cpp lives under $(PLAT), never $(OJ)/src, so it can't reach
# SRCS_CXX (the cross build) even by accident.
ifeq ($(MEMSTAT),1)
PC_SRCS_CXX += $(PLAT)/of_memstat.cpp
PC_DEFS     += -DMEMSTAT
endif

pcobj = $(PC_OBJDIR)/$(patsubst $(ROOT)/%,%,$(1)).o
PC_OBJS := $(foreach s,$(PC_SRCS_CXX) $(PC_SRCS_C),$(call pcobj,$(s)))

$(PC_OBJDIR)/%.o: $(ROOT)/% $(CURDIR)/Makefile
	@mkdir -p $(dir $@)
	@case "$<" in \
	  *.cpp) echo "PC CXX $*"; $(PC_CXX) -std=gnu++17 -O2 -g $(PC_DEFS) $(APPINC) $(PC_SDL_CFLAGS) -c -o $@ "$<" ;; \
	  *.c)   echo "PC CC  $*"; $(PC_CC)  -std=gnu11  -O2 -g $(PC_DEFS) $(APPINC) $(PC_SDL_CFLAGS) -c -o $@ "$<" ;; \
	esac

openjazz_pc: $(PC_OBJS)
	$(PC_CXX) -o $@ $(PC_OBJS) $(PC_SDL_LIBS) -lm

test: openjazz_pc

clean:
	rm -rf $(OBJDIR) $(PC_OBJDIR) app.elf openjazz_pc $(ROOT)/build/pocket/$(APP)

.PHONY: all release copy package test clean
