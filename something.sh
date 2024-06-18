#!/bin/sh

# Define the number of terminals you want to open
NUM_TERMINALS=5

# Function to open a new terminal and send a message
open_terminal() {
 ./jobCommander localhost 2001 issueJob ./progDelay 5 &
}

# Loop to open the defined number of terminals
for i in $(seq 1 $NUM_TERMINALS); do
  open_terminal
done

wait # Wait for all background jobs to finish