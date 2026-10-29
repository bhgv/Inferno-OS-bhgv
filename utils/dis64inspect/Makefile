CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra

all: dis64inspect

dis64inspect: dis64inspect.c $(wildcard ../../include/dis64*.h ../../include/dis-*.h) ../../include/isa.h
	$(CC) $(CFLAGS) -Wno-unused-function -std=c99 -I../../include dis64inspect.c -o $@

clean:
	rm -f dis64inspect

.PHONY: all clean
