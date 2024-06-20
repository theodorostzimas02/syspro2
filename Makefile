# Compiler
CC = gcc

# Source directory
SRC = ./src


# Output directory
BIN = ./bin

# Test directory
TESTS = ./tests

# Compile Options
CFLAGS = -g -Wall -Werror -std=c99 -D_POSIX_C_SOURCE=200809L -pthread


# Compile rules
$(BIN)/jobCommander: $(SRC)/jobCommander.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/jobExecutorServer: $(SRC)/jobExecutorServer.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/progDelay: $(TESTS)/progDelay.c | $(TESTS)
	$(CC) $(CFLAGS) $< -o $@


all: $(BIN)/jobCommander $(BIN)/jobExecutorServer $(BIN)/progDelay

clean:
	rm -rf $(BIN)/jobCommander $(BIN)/jobExecutorServer $(BIN)/progDelay


