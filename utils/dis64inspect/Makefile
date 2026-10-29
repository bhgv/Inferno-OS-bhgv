CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra

all: dis64inspect

dis64inspect: dis64inspect.c ../../include/dis64.h
	$(CC) $(CFLAGS) -std=c99 -I../../include dis64inspect.c -o $@

clean:
	rm -f dis64inspect

.PHONY: all clean
