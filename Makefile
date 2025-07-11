# Makefile

CC = gcc
CFLAGS = -Wall -g
LDFLAGS_SENDER = -liio -lm -lrt
LDFLAGS_RECEIVER =
TARGETS = sender receiver hid_sender print_gravity head_tracking

all: $(TARGETS)

sender: sender.c
	$(CC) $(CFLAGS) sender.c -o sender $(LDFLAGS_SENDER)

receiver: receiver.c
	$(CC) $(CFLAGS) receiver.c -o receiver $(LDFLAGS_RECEIVER)

hid_sender: hid_sender.c
	$(CC) $(CFLAGS) hid_sender.c -o hid_sender -lm

print_gravity: print_gravity.c
	$(CC) $(CFLAGS) print_gravity.c -o print_gravity -lm

head_tracking: head_tracking.c
	$(CC) $(CFLAGS) head_tracking.c -o head_tracking -lm

clean:
	rm -f $(TARGETS)
