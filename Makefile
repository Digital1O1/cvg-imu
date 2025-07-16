# Makefile

CC = gcc
CFLAGS = -Wall -g
LDFLAGS_SENDER = -liio -lm -lrt
LDFLAGS_RECEIVER =
TARGETS = head_tracking

all: $(TARGETS)

head_tracking: head_tracking.c
	$(CC) $(CFLAGS) head_tracking.c -o head_tracking -lm -liio

clean:
	rm -f $(TARGETS)
