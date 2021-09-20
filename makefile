CC = gcc
CFLAGS = -g -Wall -std=c99

smallsh : smallsh.c
	$(CC) $(CFLAGS) -o $@ $^

clean :
	-rm smallsh