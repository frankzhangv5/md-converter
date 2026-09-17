# Make's builtin default is CC=cc; ?= cannot override it. Prefer gcc unless
# the user/env already set CC (origin != default).
ifeq ($(origin CC),default)
  CC := gcc
endif
FLEX    ?= flex
BISON   ?= bison
MSYS    ?= D:/msys64

ifeq ($(OS),Windows_NT)
  EXE := .exe
  # Windows installs expose `python`, not `python3` (Store stub often exits 49).
  PYTHON ?= python
  # Native Windows make (cmd/powershell): inject MSYS tool paths.
  # Under MSYS2/MINGW (MSYSTEM set, e.g. CI shell: msys2), PATH is already
  # correct; rewriting with ';' breaks /mingw64/bin so gcc disappears while
  # /usr/bin flex/bison still work.
  ifeq ($(MSYSTEM),)
    PATH := $(MSYS)/usr/bin;$(MSYS)/mingw64/bin;$(PATH)
  endif
else
  EXE :=
  PYTHON ?= python3
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
