# Makefile

CC = gcc
CFLAGS = -Wall -g
LDFLAGS_SENDER = -liio -lm -lrt
LDFLAGS_RECEIVER =
TARGETS = sender receiver

all: $(TARGETS)

sender: sender.c
	$(CC) $(CFLAGS) sender.c -o sender $(LDFLAGS_SENDER)

receiver: receiver.c
	$(CC) $(CFLAGS) receiver.c -o receiver $(LDFLAGS_RECEIVER)

clean:
	rm -f $(TARGETS)
