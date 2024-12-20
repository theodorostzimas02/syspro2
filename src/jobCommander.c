#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../include/types.h"



//Sending character by character the job to the server
int sendJob(int sockfd, const char *job) {
    char buf[strlen("issueJob") + strlen(job) + 2]; // 2 for space and terminating character '@'
    sprintf(buf, "issueJob %s", job);
    for (int i = 0; i < (int)strlen(buf); i ++) {
        int n = write(sockfd, &buf[i], 1);
        if (n < 0) {
            perror("write");
            return 1;
        }
    }

    // Send the terminating character ('@' in this case)
    int n = write(sockfd, "@", 1);
    if (n < 0) {
        perror("write");
        return 1;
    }    
    return 0;
}



int main(int argc, char** argv){
    char* job = NULL;
    char* N = NULL;  //Concurrency
    char* jobID = NULL; 
    int mode = 0; // 1: issueJob, 2: setConcurrency, 3: stop, 4: poll, 5: exit


    if (argc < 4){
        printf("Usage: jobCommander [serverName] [portNum] [jobCommanderInputCommand]\n");
        return 1;
    }

    if (strcmp(argv[3], "issueJob") == 0) {                 
        if (argc < 5) {                                     
            printf("Usage: issueJob <job>\n");
            return 1;
        }
        
        //First, allocate memory for the job command with BUFSIZE 
        char* jobCommand = malloc(BUFSIZE);
        if (jobCommand == NULL) {
            perror("malloc");
            return 1;
        }

        jobCommand[0] = '\0';
        
        //for every argument, we need to check if the required size is greater than BUFSIZE, if it is, we need to reallocate memory
        for (int i = 4; i < argc; ++i) {
            // Calculating required size
            size_t requiredSize = strlen(jobCommand) + strlen(argv[i]) + 2;
            if (requiredSize > BUFSIZE) {
                
                jobCommand = realloc(jobCommand, requiredSize);
            }
            strcat(jobCommand, argv[i]);
            strcat(jobCommand, " ");
        }
        jobCommand = realloc(jobCommand, strlen(jobCommand) + 2);

        // Add terminating character '@'
        jobCommand[strlen(jobCommand) - 1] = '@';
        mode = 1;
        // Copy the job command to the job variable
        job = strdup(jobCommand);
        free(jobCommand);
        

    } else if (strcmp(argv[3], "setConcurrency") == 0) {
        if (argc < 5) {
            printf("Usage: setConcurrency <N>\n");
            return 1;
        }
        mode = 2;
        // Copy the value of N to the N variable for concurrency
        N = argv[4];
    } else if (strcmp(argv[3], "stop") == 0) {
        if (argc < 5) {
            printf("Usage: stop <jobID>\n");
            return 1;
        }
        mode = 3;
        jobID = argv[4];
    } else if (strcmp(argv[3], "poll") == 0) {
        mode = 4;
    } else if (strcmp(argv[3], "exit") == 0) {
        mode = 5;
    } else {
        printf("Invalid command: %s\n", argv[3]);
        printf("Please use one of the following commands: issueJob, setConcurrency, stop, poll, exit\n");
        return 1;
    }

    char* serverName = argv[1];
    char* portNum = argv[2];
    char symbolicip[BUFSIZE];
    struct hostent* mymachine;
    struct in_addr** addrlist;

    // Get the IP address of the server
    if ((mymachine = gethostbyname(serverName)) == NULL){
        perror("gethostbyname");
        return 1;
    }else {
        printf ("Connecting to  : %s \n", mymachine->h_name); 
        addrlist = ( struct in_addr **) mymachine->h_addr_list ;
        for (int i = 0; addrlist[i] != NULL ; i ++) {
            strcpy(symbolicip, inet_ntoa(*addrlist[i]));
            printf ("%s resolved to %s \n", mymachine->h_name, symbolicip);
        }
    }

    int sockfd;
    struct sockaddr_in server;
    struct sockaddr * serverptr = (struct sockaddr*) &server;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        return 1;
    }
    int port = atoi(portNum);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(symbolicip);
    server.sin_port = htons(port);

    if (connect(sockfd, serverptr, sizeof(server)) < 0){
        perror("connect");
        return 1;
    }
    
    switch (mode){
        case 1:{
            sendJob(sockfd, job);
            break;
        }
        case 2:{
            char buf2[strlen("setConcurrency") + strlen(N) + 1];
            sprintf(buf2, "setConcurrency %s@", N);
            write(sockfd, buf2, strlen("setConcurrency") + strlen(N) + 2);
            break;
        }
        case 3:{
            char buf3[strlen("stop") + strlen(jobID) + 3];
            sprintf(buf3, "stop %s@", jobID);
            write(sockfd, buf3, strlen("stop") + strlen(jobID) + 3);
            break;
        }
        case 4:{
            write(sockfd, "poll@", strlen("poll@"));
            break;
        }
        case 5:{
            write(sockfd, "exit@", strlen("exit@"));
            break;
        }
        default:
            break;
    }
    if (mode == 1) {

        //We will eventually reallocate all of this. At first we allocate memory for 1 character
        char* commitBuf = malloc(sizeof(char) * 1);
        if (commitBuf == NULL) {
            perror("malloc");
            exit(1);
        }


        //We will read the response character by character until we find the terminating character '@'
        int total_chunks = 0;
        while (1) {
            int n = read(sockfd, commitBuf + total_chunks, 1);
            if (n < 0) {
                perror("read");
                exit(1);
            }
           
           //If we find the terminating character '@', we will replace it with the null character
           if (commitBuf[total_chunks] == '@') {
                commitBuf[total_chunks] = '\0';
                break;
            }
           
            total_chunks++;
            commitBuf = realloc(commitBuf, total_chunks + 1);
            if (commitBuf == NULL) {
                perror("realloc");
                exit(1);
            }
        }

        printf("%s\n", commitBuf);
        free(commitBuf);
        

        //We do the exact same thing for the actual response of the job
        char* buf = malloc(sizeof(char) * 1);
        if (buf == NULL) {
            perror("malloc");
            exit(1);
        }

        total_chunks = 0;
        while (1) {
            int n = read(sockfd, buf + total_chunks, 1);
            if (n < 0) {
                perror("read");
                exit(1);
            }
           
           if (buf[total_chunks] == '@') {
                buf[total_chunks] = '\0';
                break;
            }
           
            total_chunks++;
            buf = realloc(buf, total_chunks + 1);
            if (buf == NULL) {
                perror("realloc");
                exit(1);
            }
        }

        printf("Response for command: %s\n", argv[3]);
        printf("%s\n", buf);
        free(buf);

    } else {
        // For any other command, we will read the response in a single read
        char response[BUFSIZE];
        int n = read(sockfd, response, BUFSIZE);
        if (n < 0) {
            perror("read");
            return 1;
        }
        response[n] = '\0';
        printf("Response for command: %s\n", argv[3]);
        printf("%s\n", response);
        
    }

    close(sockfd);

    
    // Free allocated memory
    if (job != NULL) {
        free(job);
    }
    
    return 0;
}