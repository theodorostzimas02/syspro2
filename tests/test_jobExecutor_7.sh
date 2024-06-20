killall progDelay
./bin/jobCommander localhost 2000 issueJob ./bin/progDelay 1000
./bin/jobCommander localhost 2000 stop job_2
./bin/jobCommander localhost 2000 exit