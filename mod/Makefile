BUILD_DIR := build

# Mods are compiled for the N64's MIPS CPU. On Windows the official LLVM builds lack the MIPS
# target, so default to MSYS2's clang for compiling and the official LLVM ld.lld for linking
# (see Quest64-Recomp/BUILDING.md). Override with: make CC=... LD=...
ifeq ($(OS),Windows_NT)
    CC ?= C:/msys64/clang64/bin/clang.exe
    LD := C:/PROGRA~1/LLVM/bin/ld.lld.exe
else
    CC ?= clang
    LD ?= ld.lld
endif
ifeq ($(origin CC),default)
    CC := $(if $(filter Windows_NT,$(OS)),C:/msys64/clang64/bin/clang.exe,clang)
endif

RECOMP     := ../../Quest64-Recomp
MOD_TOOL   := $(RECOMP)/lib/N64ModernRuntime/N64Recomp/build/RecompModTool.exe
MODS_DIR   := $(LOCALAPPDATA)/Quest64Recompiled/mods
NRM        := $(BUILD_DIR)/eltale_odyssey.nrm

TARGET  := $(BUILD_DIR)/mod.elf

LDSCRIPT := mod.ld
ARCHFLAGS := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
             -fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset
WARNFLAGS := -Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-variable \
             -Wno-missing-braces -Wno-unsupported-floating-point-opt -Werror=section
CFLAGS   := $(ARCHFLAGS) $(WARNFLAGS) -D_LANGUAGE_C -nostdinc -ffunction-sections
CPPFLAGS := -DMIPS -I include -I src
LDFLAGS  := -nostdlib -T $(LDSCRIPT) -Map $(BUILD_DIR)/mod.map --unresolved-symbols=ignore-all --emit-relocs -e 0 --no-nmagic -gc-sections

C_SRCS := $(wildcard src/*.c)
C_OBJS := $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o))
C_DEPS := $(C_OBJS:.o=.d)

all: $(NRM)

$(TARGET): $(C_OBJS) $(LDSCRIPT)
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

# Packages the mod (checks every hooked or called game function exists in Quest64Syms).
$(NRM): $(TARGET) mod.toml
	$(MOD_TOOL) mod.toml $(BUILD_DIR)

# Copies the mod into the game's mods folder. Close the game first.
install: $(NRM)
	cp $(NRM) "$(MODS_DIR)/"

clean:
	rm -rf $(BUILD_DIR)

-include $(C_DEPS)

.PHONY: clean all install
