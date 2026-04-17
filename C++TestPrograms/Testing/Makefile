# Makefile

CC = gcc
CFLAGS = -Wall -g -liio -lm
BUILD_DIR = build
TARGETS = $(BUILD_DIR)/head_tracking 

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/head_tracking: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) main.c -o $(BUILD_DIR)/head_tracking -lm -liio

clean:
	rm -rf $(BUILD_DIR)
