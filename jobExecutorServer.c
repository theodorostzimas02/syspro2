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
#include <pthread.h>

struct job{
    char* job;
    int jobID;
    int status; // 0: running, 1: queued
};


#define BUFSIZE 1024

int main(int argc, char** argv){
    int sock, newsock ;
    struct sockaddr_in server, client ;
    socklen_t clientlen ;
    struct sockaddr* serverptr = (struct sockaddr*) &server ;
    struct sockaddr* clientptr = (struct sockaddr*) &client ;
    struct hostent* rem ;

    if (argc < 4){
        printf("Usage: jobExecutorServer [portNum] [bufferSize] [threadPoolSize]\n");
        exit(1);
    }
    int portNum = atoi(argv[1]);
    int bufferSize = atoi(argv[2]);
    int threadPoolSize = atoi(argv[3]);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portNum);
    if (bind(sock, serverptr, sizeof(server)) < 0){
        perror("bind");
        exit(1);
    }
    if (listen(sock, bufferSize) < 0){
        perror("listen");
        exit(1);
    }
    printf("Listening for connections to port %d\n", portNum);
    while (1){
        clientlen = sizeof(client);
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0){
            perror("accept");
            exit(1);
        }
        if ((rem = gethostbyaddr((char*) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL){
            perror("gethostbyaddr");
            exit(1);
        }
        printf("Accepted connection from %s\n", rem->h_name);
        printf("Accepted connection from %s\n", inet_ntoa(client.sin_addr));
        char buf[BUFSIZE];
        while (1){
            int n = read(newsock, buf, BUFSIZE);
            if (n < 0){
                perror("read");
                exit(1);
            }
            if (n == 0){
                break;
            }
            buf[n] = '\0';
            printf("Server received %d bytes: %s\n", n, buf);
        }
    }





}