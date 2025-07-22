# Makefile

CC = gcc
CFLAGS = -Wall -g
BUILD_DIR = build
TARGETS = $(BUILD_DIR)/head_tracking $(BUILD_DIR)/rocking_test $(BUILD_DIR)/consistency_test $(BUILD_DIR)/timing_test $(BUILD_DIR)/timing_test_writer

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/head_tracking: head_tracking.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) head_tracking.c -o $(BUILD_DIR)/head_tracking -lm -liio

$(BUILD_DIR)/rocking_test: ./testing/rocking_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) ./testing/rocking_test.c -o $(BUILD_DIR)/rocking_test -lm -liio

$(BUILD_DIR)/consistency_test: ./testing/consistency_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) ./testing/consistency_test.c -o $(BUILD_DIR)/consistency_test

$(BUILD_DIR)/timing_test: ./testing/timing_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) ./testing/timing_test.c -o $(BUILD_DIR)/timing_test

$(BUILD_DIR)/timing_test_writer: ./testing/timing_test_writer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) ./testing/timing_test_writer.c -o $(BUILD_DIR)/timing_test_writer

clean:
	rm -rf $(BUILD_DIR)
