GPPFLAGS = -Wall -Wextra -g
BUILD = ./build
BASE = $(BUILD)/main.o
TCC = ./tlang/tcc
POSTPROC = $(BUILD)/postproc
ASSEMBLER = $(BUILD)/assembler

BASE_SRCS = ./*.cpp ./util/*.cpp ./kernel/*.cpp ./assembler/instructions.cpp ./assembler/asm_loader.cpp
BASE_DEPS = $(BASE_SRCS) ./*.hpp ./util/*.hpp ./kernel/*.hpp ./assembler/instructions.hpp ./assembler/asm_loader.hpp

TCC_SRCS = ./tlang/*.cpp ./tlang/*/*.cpp ./util/globals.cpp
TCC_DEPS = $(TCC_SRCS) ./tlang/*.hpp ./tlang/*/*.hpp ./util/globals.hpp

ASSEMBLER_SRCS = ./assembler/*.cpp ./kernel/kernel.cpp ./tpu.cpp ./memory.cpp ./util/*.cpp
ASSEMBLER_DEPS = $(ASSEMBLER_SRCS) ./assembler/*.hpp ./kernel/kernel.hpp ./tpu.hpp ./memory.hpp ./util/*.hpp

all: $(BASE) $(TCC) $(POSTPROC) $(ASSEMBLER)
base: $(BASE)
tcc: $(TCC)
postproc: $(POSTPROC)
assembler: $(ASSEMBLER)

$(BASE): $(BASE_DEPS)
	@echo -n "Building main executable... "
	@g++ $(BASE_SRCS) -o $@ -lncurses $(GPPFLAGS)
	@echo "Done."

$(TCC): $(TCC_DEPS)
	@echo -n "Building TCC (T compiler)... "
	@g++ $(TCC_SRCS) -o $@ $(GPPFLAGS)
	@if ! [ -L ./tlang/postproc ]; then\
		ln -sf ../build/postproc ./tlang/postproc;\
	fi
	@echo "Done."

$(POSTPROC): ./postprocessor/postprocessor.cpp
	@echo -n "Building TPU Post-Processor... "
	@g++ $^ -o $@ $(GPPFLAGS)
	@echo "Done."

$(ASSEMBLER): $(ASSEMBLER_DEPS)
	@echo -n "Building assembler... "
	@g++ $(ASSEMBLER_SRCS) -o $@ -lncurses $(GPPFLAGS)
	@echo "Done."