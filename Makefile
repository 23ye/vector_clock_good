CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c99 -g -O0 -I./include
LDFLAGS ?=

BUILD_DIR := build
SRCS := src/vector_clock.c src/log_entry.c
OBJS := $(BUILD_DIR)/vector_clock.o $(BUILD_DIR)/log_entry.o
TEST_BIN := $(BUILD_DIR)/test_vector_clock.exe
TEST_SRC := test/test_vector_clock.c

all: $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/vector_clock.o: src/vector_clock.c include/vector_clock.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/log_entry.o: src/log_entry.c include/log_entry.h include/vector_clock.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(OBJS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJS) $(TEST_BIN)

.PHONY: all test clean
