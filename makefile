# Compiler
CC = gcc

# Compile Options
CFLAGS = -g -Wall -Werror -std=c99 -D_POSIX_C_SOURCE=200809L -pthread

# Compile rules
jobCommander: jobCommander.c 
	$(CC) $(CFLAGS) $^ -o $@

jobExecutorServer: jobExecutorServer.c 
	$(CC) $(CFLAGS) $^ -o $@

# progDelay: processes/progDelay.c
# 	$(CC) $(CFLAGS) $^ -o $@

all: jobCommander jobExecutorServer 
#progDelay

clean:
	rm -rf jobCommander jobExecutorServer 
