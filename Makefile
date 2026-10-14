CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
RAYLIB  ?= $(HOME)/raylib/src
LDLIBS   = $(RAYLIB)/libraylib.a -lm -lpthread -ldl -lX11

.PHONY: all run clean

all: tetris

tetris: tetris.c
	$(CC) $(CFLAGS) -I$(RAYLIB) -o $@ $< $(LDLIBS)

run: tetris
	./run.sh

clean:
	rm -f tetris tetris.log
