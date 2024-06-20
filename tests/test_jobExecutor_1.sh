./bin/jobCommander localhost 2000 issueJob touch myFile.txt
ls jobExecutorServer.txt
ls myFile.txt
./bin/jobCommander localhost 2000 issueJob rm myFile.txt
ls myFile.txt
./bin/jobCommander localhost 2000 exit
ls jobExecutorServer.txt