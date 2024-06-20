killall progDelay
./bin/jobExecutorServer 2000 4 2 
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander exit