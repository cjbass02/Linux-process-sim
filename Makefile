CC = gcc
CFLAGS = -Wall -Wextra

all: main

main: main.c
	$(CC) $(CFLAGS) -o procsim.out main.c

clean:
	rm -f procsim.out