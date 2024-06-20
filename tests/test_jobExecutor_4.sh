#Run server with buffer 4 worker threads 2 concurrency 1
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 10 &
./bin/jobCommander localhost 2000 poll & #empty
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 10 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 10 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 10 &
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander localhost 2000 exit &