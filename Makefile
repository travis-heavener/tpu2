GPPFLAGS = -Wall -Wextra -g
BUILD = ./build
BASE = $(BUILD)/main.o
TCC = ./tlang/tcc
POSTPROC = $(BUILD)/postproc

BASE_SRCS = ./*.cpp ./util/*.cpp
BASE_DEPS = $(BASE_SRCS) ./*.hpp ./util/*.hpp

TCC_SRCS = ./tlang/*.cpp ./tlang/*/*.cpp
TCC_DEPS = $(TCC_SRCS) ./tlang/*.hpp ./tlang/*/*.hpp

all: $(BASE) $(TCC) $(POSTPROC)
base: $(BASE)
tcc: $(TCC)
postproc: $(POSTPROC)

$(BASE): $(BASE_DEPS)
	@echo -n "Building main executable..."
	@g++ $(BASE_SRCS) -o $@ -lncurses $(GPPFLAGS)
	@echo " Done."

$(TCC): $(TCC_DEPS)
	@echo -n "Building TCC (T compiler)..."
	@g++ $(TCC_SRCS) -o $@ $(GPPFLAGS)
	@if ! [ -L ./tlang/postproc ]; then\
		ln -s ../$(POSTPROC) ./tlang/postproc;\
	fi
	@echo " Done."

$(POSTPROC): ./postprocessor/postprocessor.cpp
	@echo -n "Building TPU Post-Processor..."
	@g++ $^ -o $@ $(GPPFLAGS)
	@echo " Done."