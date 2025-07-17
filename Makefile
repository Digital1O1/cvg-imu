# Makefile

CC = gcc
CFLAGS = -Wall -g
LDFLAGS_SENDER = -liio -lm -lrt
LDFLAGS_RECEIVER =
TARGETS = head_tracking ./testing/rocking_test

all: $(TARGETS)

head_tracking: head_tracking.c
	$(CC) $(CFLAGS) head_tracking.c -o head_tracking -lm -liio

clean:
	rm -f head_tracking ./testing/rocking_test

./testing/rocking_test: ./testing/rocking_test.c
	$(CC) $(CFLAGS) ./testing/rocking_test.c -o ./testing/rocking_test
