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
#define BUFSIZE 1024

struct job{
    char* job;
    char* jobID;
};

struct CommanderBuffer{
    struct job* jobBuffer;
    int bufferSize;
    int currentJobs;
    pthread_mutex_t bufferMutex;
};


int addJob(char* job,struct CommanderBuffer* p){
    struct job* newJob = malloc(sizeof(struct job));
    newJob->job = job;
    sprintf(newJob->jobID,"job_%d", p->currentJobs + 1);

    pthread_mutex_lock(&p->bufferMutex);
    if (p->currentJobs == p->bufferSize){
        return -1;
    }
    for (int i = 0; i < p->bufferSize; i++){
        if (p->jobBuffer[i].job == NULL){
            p->jobBuffer[i] = *newJob;
            p->currentJobs++;
            return 0;
        }
    }
    pthread_mutex_unlock(&p->bufferMutex);
    return -1;

}

int removeJob(struct job* jobBuffer, struct job* newJob, int bufferSize){
    for (int i = 0; i < bufferSize; i++){
        if (jobBuffer[i].job == newJob->job){
            jobBuffer[i].job = NULL;
            return 0;
        }
    }
    return -1;
}


void* workerThread(void* arg) {
    struct CommanderBuffer* p = (struct CommanderBuffer*)arg;
    while (1) {
        struct job currentJob;
        pthread_mutex_lock(&p->bufferMutex);
        if (p->currentJobs > 0) {
            for (int i = 0; i < p->bufferSize; i++) {
                if (p->jobBuffer[i].job != NULL) {
                    currentJob = p->jobBuffer[i];
                    p->jobBuffer[i].job = NULL;
                    p->currentJobs--;
                    break;
                }
            }
        } else {
            pthread_mutex_unlock(&p->bufferMutex);
            sleep(1);  // Sleep for a while if no jobs are available
            continue;
        }
        pthread_mutex_unlock(&p->bufferMutex);

        printf("Worker thread processing job ID: %d\n", currentJob.jobID);
        // Process the job
        free(currentJob.job);  // Free the job string after processing
    }
    return NULL;
}


void* controllerThread(void* arg){
    int newsock = *((int*)arg);
    struct CommanderBuffer* CB= ((struct CommanderBuffer*)(arg + sizeof(int)));
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

    if (strncmp(buf, "issueJob", 8) == 0){
        char* job = buf + 9;
        printf("Job: %s\n", job);
        addJob(job, CB);


    }
}





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

    if (argv[1] < 0 || argv[2] <= 0 || argv[3] <= 0){
        printf("Usage: jobExecutorServer [portNum] [bufferSize] [threadPoolSize]\n");
        exit(1);
    }
    int portNum = atoi(argv[1]);
    int bufferSize = atoi(argv[2]);
    int threadPoolSize = atoi(argv[3]);

    struct CommanderBuffer* Commanderbuffer = malloc(sizeof(struct CommanderBuffer));
    Commanderbuffer->jobBuffer = malloc(sizeof(struct job) * bufferSize);
    Commanderbuffer->bufferSize = bufferSize;
    Commanderbuffer->currentJobs = 0;
    pthread_mutex_init(&Commanderbuffer->bufferMutex, NULL);

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

        pthread_t controller;
        void* arg = malloc(sizeof(int) + sizeof(struct Commanderbuffer*));
        *((int*)arg) = newsock;
        memcpy(arg + sizeof(int), &Commanderbuffer, sizeof(struct Commanderbuffer*));
        pthread_create(&controller, NULL, controllerThread, arg);



        // char buf[BUFSIZE];
        // while (1){
        //     int n = read(newsock, buf, BUFSIZE);
        //     if (n < 0){
        //         perror("read");
        //         exit(1);
        //     }
        //     if (n == 0){
        //         break;
        //     }
        //     buf[n] = '\0';
        //     printf("Server received %d bytes: %s\n", n, buf);
        // }
    }

    for (int i = 0; i < threadPoolSize; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, workerThread, NULL);
    }




}