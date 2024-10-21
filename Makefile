GPPFLAGS = -Wall -Wextra -g
BUILD = ./build
BASE = $(BUILD)/main.o
TCC = ./tlang/tcc
POSTPROC = $(BUILD)/postproc
ASSEMBLER = $(BUILD)/assembler

BASE_SRCS = ./*.cpp ./util/*.cpp ./kernel/*.cpp
BASE_DEPS = $(BASE_SRCS) ./*.hpp ./util/*.hpp ./kernel/*.hpp

TCC_SRCS = ./tlang/*.cpp ./tlang/*/*.cpp ./util/globals.cpp
TCC_DEPS = $(TCC_SRCS) ./tlang/*.hpp ./tlang/*/*.hpp ./util/globals.hpp

ASSEMBLER_SRCS = ./assembler/*.cpp ./memory.cpp ./util/*.cpp
ASSEMBLER_DEPS = $(ASSEMBLER_SRCS) ./assembler/*.hpp ./memory.hpp ./util/*.hpp

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

# for remaking the boot image
image:
	@./tlang/tcc ./os/os_0.0.1.t -f
	@python3 ./os/make_boot_drive.py A
	@./build/assembler ./os/os_0.0.1.tpu ./os/A.dsk