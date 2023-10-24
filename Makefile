BIN:=http-server

CPP?=g++
CC?=gcc

LIBS:=
SRC:=

SRC+=src/http-server.c
SRC+=example.c

override CFLAGS?=-Wall -s -O2

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
