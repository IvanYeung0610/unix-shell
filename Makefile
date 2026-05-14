CC = gcc
CFLAGS = -Wall -Wextra -g

all: shell

shell: src/shell.c
	$(CC) $(CFLAGS) -Iinclude -o shell src/shell.c

clean:
	rm -f shell
