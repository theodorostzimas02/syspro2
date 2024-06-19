# Compiler
CC = gcc

# Include directories
INCLUDES = -I./include

# Source directory
SRC = ./src

# Library directory (if any)
LIB = ./lib

# Output directory
BIN = ./bin

# Compile Options
CFLAGS = -g -Wall -Werror -std=c99 -D_POSIX_C_SOURCE=200809L -pthread

# Create bin directory if it doesn't exist
$(BIN):
	mkdir -p $(BIN)

# Compile rules
$(BIN)/jobCommander: $(SRC)/jobCommander.c | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN)/jobExecutorServer: $(SRC)/jobExecutorServer.c | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

# progDelay: $(SRC)/progDelay.c | $(BIN)
# 	$(CC) $(CFLAGS) $(INCLUDES) $< -o $(BIN)/progDelay

all: $(BIN)/jobCommander $(BIN)/jobExecutorServer 
# $(BIN)/progDelay

clean:
	rm -rf $(BIN)/jobCommander $(BIN)/jobExecutorServer 
# $(BIN)/progDelay

.PHONY: all clean
