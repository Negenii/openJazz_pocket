# cxx.mk — shared C++ toolchain rules for OpenJazz on openfpgaOS.
# Included by src/openjazz/Makefile and src/ehspike/Makefile.
#
# Expects from the includer: APP ROOT SDK OBJDIR SRCS_C SRCS_CXX APPINC DEFS
# Optional: EXCEPTIONS=1 (link libgcc_eh, build with -fexceptions), HOT_SRCS
# Provides: app.elf, CC CXX CFLAGS CXXFLAGS LDFLAGS LIBS CRT, container guard.

MUSL := $(SDK)/musl
PLAT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))platform)

CROSS ?= $(shell \
	if   command -v riscv-none-elf-g++      >/dev/null 2>&1; then echo riscv-none-elf-; \
	elif command -v riscv64-unknown-elf-g++ >/dev/null 2>&1; then echo riscv64-unknown-elf-; \
	else echo riscv64-elf-; fi)
CC    := $(CROSS)gcc
CXX   := $(CROSS)g++
SIZE  := $(CROSS)size
ARCH  := -march=rv32imafc -mabi=ilp32f

CXXINC    := $(shell echo | $(CXX) $(ARCH) -x c++ -E -Wp,-v - 2>&1 | sed -n 's,^ \(/.*c++.*\),-isystem \1,p')
GXXINC    := $(shell $(CXX) $(ARCH) -print-file-name=include)
LIBSTDCXX := $(shell $(CXX) $(ARCH) -print-file-name=libstdc++.a)
LIBSUPCXX := $(shell $(CXX) $(ARCH) -print-file-name=libsupc++.a)
LIBGCC    := $(shell $(CXX) $(ARCH) -print-libgcc-file-name)
LIBGCC_EH := $(shell $(CXX) $(ARCH) -print-file-name=libgcc_eh.a)

SYSINC := -nostdinc $(CXXINC) -isystem $(MUSL)/include -isystem $(GXXINC) -I$(SDK)/include

BASE_OPT ?= -O2
COMMON := $(ARCH) $(BASE_OPT) -fno-math-errno -fno-trapping-math \
          -ffunction-sections -fdata-sections -fno-tree-loop-distribute-patterns \
          -include $(PLAT_DIR)/of_ctype_compat.h $(DEFS) $(APPINC) $(SYSINC) \
          -Wall -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare

EXCEPTIONS ?= 0
ifeq ($(EXCEPTIONS),1)
  EH_FLAG := -fexceptions
  EH_LIB  := $(LIBGCC_EH)
  SRCS_C  += $(PLAT_DIR)/of_eh_frames.c
else
  EH_FLAG := -fno-exceptions
  EH_LIB  :=
endif

CXXFLAGS := -std=gnu++17 $(EH_FLAG) -fno-rtti -fno-threadsafe-statics $(COMMON)
CFLAGS   := -std=gnu11 $(COMMON)

LDFLAGS := $(ARCH) -nostdlib -static -T $(SDK)/app.ld -L$(MUSL)/lib \
           -Wl,--gc-sections -Wl,--no-warn-rwx-segments
LIBS := -Wl,--start-group -lc -lm $(LIBSTDCXX) $(LIBSUPCXX) $(EH_LIB) $(LIBGCC) -Wl,--end-group
CRT  := $(MUSL)/lib/crt1.o $(MUSL)/lib/crti.o $(MUSL)/lib/crtn.o

# Objects mirror the source tree under OBJDIR, keyed by path relative to ROOT.
obj = $(OBJDIR)/$(patsubst $(ROOT)/%,%,$(1)).o
C_OBJS   := $(foreach s,$(SRCS_C),$(call obj,$(s)))
CXX_OBJS := $(foreach s,$(SRCS_CXX),$(call obj,$(s)))
ALL_OBJS := $(C_OBJS) $(CXX_OBJS)

HOT_OPT ?= -O3
ifneq ($(strip $(HOT_SRCS)),)
  HOT_OBJS := $(foreach s,$(HOT_SRCS),$(call obj,$(s)))
  $(HOT_OBJS): CXXFLAGS += $(HOT_OPT)
  $(HOT_OBJS): CFLAGS   += $(HOT_OPT)
endif

$(OBJDIR)/%.o: $(ROOT)/% $(MAKEFILE_LIST)
	@mkdir -p $(dir $@)
	@case "$<" in \
	  *.cpp) echo "CXX $*"; $(CXX) $(CXXFLAGS) -c -o $@ "$<" ;; \
	  *.c)   echo "CC  $*"; $(CC)  $(CFLAGS)  -c -o $@ "$<" ;; \
	esac

# Container guard (mirrors Diablo): by default app.elf is built inside the
# openfpgaos-openjazz image; USE_SDK_CONTAINER=0 builds on the host toolchain.
USE_SDK_CONTAINER ?= 1
SDK_CONTAINER    ?= $(ROOT)/tools/sdk-container.sh
export SDK_IMG        ?= openfpgaos-openjazz
export SDK_DOCKERFILE ?= $(ROOT)/tools/docker/Dockerfile.openjazz
ifeq ($(USE_SDK_CONTAINER)_$(OF_SDK_IN_CONTAINER),1_)
app.elf: SDK_FORCE
	@bash $(SDK_CONTAINER) make app.elf
SDK_FORCE:
.PHONY: SDK_FORCE
else
app.elf: $(ALL_OBJS)
	@echo "LD  $@ ($(words $(ALL_OBJS)) objects)"
	$(CXX) $(LDFLAGS) -o $@ $(CRT) $(ALL_OBJS) $(LIBS)
	@$(SIZE) $@ 2>/dev/null || true
endif
