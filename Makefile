# Makefile

CC = gcc
CFLAGS = -Wall -g
TARGETS = head_tracking ./testing/rocking_test ./testing/consistency_test

all: $(TARGETS)

head_tracking: head_tracking.c
	$(CC) $(CFLAGS) head_tracking.c -o head_tracking -lm -liio

./testing/rocking_test: ./testing/rocking_test.c
	$(CC) $(CFLAGS) ./testing/rocking_test.c -o ./testing/rocking_test -lm -liio

./testing/consistency_test: ./testing/consistency_test.c
	$(CC) $(CFLAGS) ./testing/consistency_test.c -o ./testing/consistency_test

clean:
	rm -f  head_tracking ./testing/rocking_test ./testing/consistency_test
