CC      ?= gcc
FLEX    ?= flex
BISON   ?= bison
PYTHON  ?= python3
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
EMBED_C := $(BUILD)/embedded_assets.c
EMBED_H := $(BUILD)/embedded_assets.h
CSS     := css/gfm.css css/teal.css css/vermillion.css
JS      := js/highlight.js js/copy-wechat.js

CFLAGS  := -Wall -Wextra -O2 -I$(SRC) -I$(BUILD)
LDFLAGS :=

.PHONY: all clean test

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(EMBED_C) $(EMBED_H): tools/embed_assets.py $(CSS) $(JS) | $(BUILD)
	$(PYTHON) tools/embed_assets.py -o $(BUILD) --root .

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	$(BISON) -d -v -o $(BUILD)/parser.tab.c $(SRC)/parser.y

$(BUILD)/lex.yy.c: $(SRC)/lex.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o $(BUILD)/lex.yy.c $(SRC)/lex.l

$(BIN): $(SRC)/main.c $(SRC)/html.c $(EMBED_C) $(BUILD)/lex.yy.c $(BUILD)/parser.tab.c $(EMBED_H)
	$(CC) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/html.c $(EMBED_C) $(BUILD)/lex.yy.c $(BUILD)/parser.tab.c $(LDFLAGS)

clean:
	rm -rf $(BUILD)

test: $(BIN)
	$(PYTHON) tests/run_tests.py
