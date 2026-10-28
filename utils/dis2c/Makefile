CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS += -I../../include

all: dis2c

dis2c: dis2c.c ops.h ../../include/isa.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -Wall -Wextra -Werror dis2c.c -o $@

# Python is needed only when the VM opcode table changes.
regen:
	python3 genops.py

test:
	python3 tests/test.py
	python3 tests/native-slots.py

clean:
	rm -f dis2c

.PHONY: all regen test clean
