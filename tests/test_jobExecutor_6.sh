killall progDelay
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 & 
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000 &
./bin/jobCommander localhost 2000 setConcurrency 4 &
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander localhost 2000 stop job_4 &
./bin/jobCommander localhost 2000 stop job_5 &
./bin/jobCommander localhost 2000 poll &
./bin/jobCommander localhost 2000 exit