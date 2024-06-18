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

#define BUFSIZE 1024

int main(int argc, char** argv){
    char* job = NULL;
    char* N = NULL;
    char* jobID = NULL;
    int mode = 0;


    if (argc < 4){
        printf("Usage: jobCommander [serverName] [portNum] [jobCommanderInputCommand]\n");
        return 1;
    }

    if (strcmp(argv[3], "issueJob") == 0) {                 
        if (argc < 5) {                                     
            printf("Usage: issueJob <job>\n");
            return 1;
        }
        // Construct the entire command string
        char jobCommand[BUFSIZE] = "";
        for (int i = 4; i < argc; ++i) {
            strcat(jobCommand, argv[i]);
            strcat(jobCommand, " ");
        }

        mode = 1;
        job = strdup(jobCommand);
    } else if (strcmp(argv[3], "setConcurrency") == 0) {
        if (argc < 5) {
            printf("Usage: setConcurrency <N>\n");
            return 1;
        }
        mode = 2;
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
        printf("Invalid command: %s\n", argv[1]);
        return 1;
    }

    char* serverName = argv[1];
    char* portNum = argv[2];
    char symbolicip[BUFSIZE];
    struct hostent* mymachine;
    struct in_addr** addrlist;

    if ((mymachine = gethostbyname(serverName)) == NULL){
        perror("gethostbyname");
        return 1;
    }else {
        printf ("Name To Be Resolved : %s \n", mymachine->h_name); 
        printf ("Name Length in Bytes : %d \n", mymachine->h_length);
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
            char buf[strlen("issueJob") + strlen(job) + 1]; 
            sprintf(buf, "issueJob %s", job);
            write(sockfd, buf, strlen("issueJob") + strlen(job) + 1);
            break;
        }
        case 2:{
            char buf2[strlen("setConcurrency") + strlen(N) + 1];
            sprintf(buf2, "setConcurrency %s", N);
            write(sockfd, buf2, strlen("setConcurrency") + strlen(N) + 1);
            break;
        }
        case 3:{
            char buf3[strlen("stop") + strlen(jobID) + 1];
            sprintf(buf3, "stop %s", jobID);
            write(sockfd, buf3, strlen("stop") + strlen(jobID) + 1);
            break;
        }
        case 4:{
            write(sockfd, "poll", strlen("poll"));
            break;
        }
        case 5:{
            write(sockfd, "exit", strlen("exit"));
            break;
        }
        default:
            break;
    }
    if (mode == 1) {
            while (1) {
                char buffer[BUFSIZE];
                int bytes_read = read(sockfd, buffer, BUFSIZE);
                if (bytes_read < 0) {
                    perror("read");
                    return 1;
                }
                
                buffer[bytes_read] = '\0';
                printf("%s", buffer);

                if (strstr(buffer, "end") != NULL) { // job_%d output end contains "end" so i use that!
                    break;
                }

            }
        } else {
            char response[BUFSIZE];
            int n = read(sockfd, response, BUFSIZE);
            if (n < 0) {
                perror("read");
                return 1;
            }
            response[n] = '\0';
            printf("Server response: %s\n", response);
            

        }

    close(sockfd);


    // Free allocated memory
    if (job != NULL) {
        free(job);
    }
    

    return 0;
}