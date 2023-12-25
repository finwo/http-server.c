BIN:=http-server

CPP?=g++
CC?=gcc

LIBS:=
SRC:=

SRC+=src/http-server.c
SRC+=example.c

override CFLAGS?=-Wall -s -O2

ifeq ($(OS),Windows_NT)
    # CFLAGS += -D WIN32
    # override CPPFLAGS+=-I external/libs/Microsoft.Web.WebView2.1.0.1150.38/build/native/include
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        # CFLAGS += -D AMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            # CFLAGS += -D AMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            # CFLAGS += -D IA32
        endif
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        # CFLAGS += -D LINUX
        # override CFLAGS+=$(shell pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0 glib-2.0)
        # override CFLAGS+=-D _GNU_SOURCE
    endif
    ifeq ($(UNAME_S),Darwin)
        # CFLAGS += -D OSX
        override CFLAGS+=-I /usr/local/include/libepoll-shim/
        override CFLAGS+=-L /usr/local/lib -lepoll-shim
        # override CFLAGS+=-D _BSD_SOURCE
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        # CFLAGS += -D AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        # CFLAGS += -D IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        # CFLAGS += -D ARM
    endif
    # TODO: flags for riscv
endif

INCLUDES:=
INCLUDES+=-I src

include lib/.dep/config.mk

OBJ:=$(SRC:.c=.o)
OBJ:=$(OBJ:.cc=.o)

override CFLAGS+=$(INCLUDES)

LDFLAGS?=-s
LDFLAGS+=$(CFLAGS)

default: $(BIN)

.PHONY: clean
clean:
	rm -rf $(OBJ)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@
