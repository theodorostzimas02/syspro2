./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 110 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 115 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 120 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 125 &
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander localhost 2000 setConcurrency 2 &
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander localhost 2000 exit