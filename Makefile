CFLAGS ::= -std=c17 -Wpedantic -Wall -Wshadow -Wextra -Werror -O2 -pipe

default: all

all: encrypt-me

encrypt-me: encrypt-me.c

clean:
	rm -rf encrypt-me *~

.PHONY: clean default all
