CC      ?= gcc
FLEX    ?= flex
BISON   ?= bison
MSYS    ?= D:/msys64

ifeq ($(OS),Windows_NT)
  EXE := .exe
  PATH := $(MSYS)/usr/bin;$(MSYS)/mingw64/bin;$(PATH)
else
  EXE :=
endif

SRC     := src
BUILD   := build
BIN     := $(BUILD)/md-convert$(EXE)

CFLAGS  := -Wall -Wextra -O2 -I$(SRC) -I$(BUILD)
LDFLAGS :=

.PHONY: all clean test

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	$(BISON) -d -v -o $(BUILD)/parser.tab.c $(SRC)/parser.y

$(BUILD)/lex.yy.c: $(SRC)/lex.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o $(BUILD)/lex.yy.c $(SRC)/lex.l

$(BIN): $(SRC)/main.c $(SRC)/html.c $(BUILD)/lex.yy.c $(BUILD)/parser.tab.c
	$(CC) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/html.c $(BUILD)/lex.yy.c $(BUILD)/parser.tab.c $(LDFLAGS)

clean:
	rm -rf $(BUILD)

test: $(BIN)
	python tests/run_tests.py
