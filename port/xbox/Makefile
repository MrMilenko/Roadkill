# Roadkill for the original Xbox: build/xbox/default.xbe, with OXDK (clang + lld-link + cxbe) and
# its SDL2x port, on macOS or Linux. Needs OXDK (https://github.com/MrMilenko/OXDK) in ~/OXDK or
# OXDK_DIR, with your own XDK's libraries and headers in OXDK's xbox/xdk (not supplied here).
#
#   make -C port/xbox            (from Roadkill's root; -j recommended)
#
# The game is Roadkill's own src/ as it is, plus this folder: the POSIX / OS pieces the desktop
# gets from its C library (compat/, xbox_*.c) and the entry point.

ROADKILL  := $(abspath ../..)
OXDK_DIR  ?= $(firstword $(wildcard $(HOME)/OXDK))
ifeq ($(strip $(OXDK_DIR)),)
$(error OXDK not found: git clone https://github.com/MrMilenko/OXDK ~/OXDK, or set OXDK_DIR)
endif

XBE_TITLE     = roadkill
XBE_NAME      = Roadkill
XBE_MODE      = RETAIL
XBE_LIMIT64MB = no
OUTPUT_DIR    = $(ROADKILL)/build/xbox

include $(OXDK_DIR)/xbox/third-party/SDL2x/oxdk/sdl2x.mk

GAME_SRCS := $(shell find $(ROADKILL)/src -name '*.c' \
                 ! -path '*/src/app/*' ! -name sdl_display.c ! -path '*/src/host/os/*' | sort)
SRCS  = $(ROADKILL)/src/app/tm2.c $(GAME_SRCS) $(wildcard $(ROADKILL)/third_party/indeo/*.c)
SRCS += $(wildcard $(CURDIR)/*.c)
SRCS += $(SDL2X_SRC_LIST)

# Roadkill's code: cdecl as on the desktop (OXDK's default is stdcall, for the XDK's samples),
# C99, the Xbox pieces first on the include path.
# float math in SSE (the Pentium III has it), as every desktop build does it: on the x87 the game's
# floats keep extra precision, and the AI drove out of the arenas (doubles stay on the x87, held
# to 53 bits by xbox_main.c)
RK_FLAGS = -std=gnu99 -Xclang -fdefault-calling-conv=cdecl -ffp-contract=off -fno-omit-frame-pointer \
           -msse -mfpmath=sse \
           -DROADKILL_XBOX=1 -DROADKILL_NO_TLS=1 \
           -I$(CURDIR)/compat -I$(ROADKILL)/src -I$(ROADKILL)/src/rt -I$(ROADKILL)/third_party/indeo -I$(ROADKILL)/third_party/dr_libs \
           -include $(CURDIR)/compat/rk_xbox.h \
           -Wno-microsoft-anon-tag -Wno-unused-parameter -Wno-incompatible-pointer-types-discards-qualifiers
# every file, SDL2x's too: clang's va_start / va_arg (compat/stdarg.h says why)
CFLAGS   += -include $(CURDIR)/compat/stdarg.h $(SDL2X_DEFINES) $(SDL2X_INCLUDES)
CXXFLAGS += -include $(CURDIR)/compat/stdarg.h $(SDL2X_DEFINES) $(SDL2X_INCLUDES)

# xnet.lib (the devkit network library, not xnets.lib): it honours XNET_STARTUP_BYPASS_SECURITY,
# which online play with PCs needs (xbox_net.c)
OXDK_LIBS = $(SDL2X_LIBS) oldnames.lib xnet.lib
# the main thread runs the whole game (the desktop gives it 8 MB): 2 MB here
LDFLAGS  += /stack:0x200000
OXDK_OPT  ?= -O2

include $(OXDK_DIR)/oxdk.mk

# Roadkill's files get RK_FLAGS (SDL2x's own keep OXDK's); tm2.c's main is called by xbox_main.c
RK_OBJS := $(foreach s,$(filter $(ROADKILL)/src/% $(ROADKILL)/third_party/% $(CURDIR)/%,$(SRCS)),$(call oxdk_obj,$(s)))
$(RK_OBJS): CFLAGS += $(RK_FLAGS)
$(RK_OBJS): OXDK_CFLAGS := -I$(CURDIR)/compat $(OXDK_CFLAGS)
$(call oxdk_obj,$(ROADKILL)/src/app/tm2.c): CFLAGS += -Dmain=roadkill_main
$(call oxdk_obj,$(ROADKILL)/src/host/cd_music.c) $(call oxdk_obj,$(ROADKILL)/third_party/indeo/indeo3.c) $(call oxdk_obj,$(ROADKILL)/third_party/indeo/indeo4.c) \
$(call oxdk_obj,$(ROADKILL)/third_party/indeo/ivi.c) $(call oxdk_obj,$(ROADKILL)/third_party/indeo/ivi_dsp.c): CFLAGS += -w
$(foreach f,en10_viewport en10_view en10_dsound,$(call oxdk_obj,$(ROADKILL)/src/engine/$(f).c)): CFLAGS += -fno-builtin

# just Roadkill's objects (porting aid)
rk-objs: $(RK_OBJS)
