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

struct job {
    char* job;
    char* jobID;
    int socket;

};

struct CommanderBuffer {
    struct job* jobBuffer;
    int bufferSize;
    int currentJobs;
    int allJobs;
    pthread_mutex_t bufferMutex;
    pthread_cond_t bufferCond;
};

int freeBuffer(struct CommanderBuffer* p) {
    for (int i = 0; i < p->bufferSize; i++) {
        if (p->jobBuffer[i].job != NULL) {
            free(p->jobBuffer[i].job);
            free(p->jobBuffer[i].jobID);
        }
    }
    free(p->jobBuffer);
    return 0;
}

int addJob(char* job, struct CommanderBuffer* p) {
    struct job* newJob = malloc(sizeof(struct job));
    newJob->job = strdup(job); 
    newJob->jobID = malloc(sizeof(char) * 8);
    sprintf(newJob->jobID, "job_%d", p->allJobs + 1);

    pthread_mutex_lock(&p->bufferMutex);
    if (p->currentJobs == p->bufferSize) {
        pthread_mutex_unlock(&p->bufferMutex);
        free(newJob->jobID);
        free(newJob->job);
        free(newJob);
        return -1;
    }
    for (int i = 0; i < p->bufferSize; i++) {
        if (p->jobBuffer[i].job == NULL) {
            p->jobBuffer[i] = *newJob;
            p->currentJobs++;
            p->allJobs++;
            pthread_cond_signal(&p->bufferCond);
            pthread_mutex_unlock(&p->bufferMutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&p->bufferMutex);
    return -1;
}

int removeJob(struct job* jobBuffer, struct job* newJob, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer[i].job == newJob->job) {
            jobBuffer[i].job = NULL;
            return 0;
        }
    }
    return -1;
}

void* workerThread(void* arg) {
    struct CommanderBuffer* p = (struct CommanderBuffer*)arg;
    while (1) {
        struct job* currentJob;
        while (p->currentJobs == 0) {
            pthread_cond_wait(&p->bufferCond, &p->bufferMutex);
        }   
        pthread_mutex_lock(&p->bufferMutex);
        if (p->currentJobs > 0) {
            for (int i = 0; i < p->bufferSize; i++) {
                if (p->jobBuffer[i].job != NULL) {
                    currentJob = &p->jobBuffer[i];
                    p->jobBuffer[i].job = NULL;
                    p->currentJobs--;
                    pthread_mutex_unlock(&p->bufferMutex);
                    break;
                }
            }
        }
        printf("Worker thread processing job ID: %s\n", currentJob->jobID);

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            char* argv[32];
            int argc = 0;
            char* token = strtok(currentJob->job, " ");
            while (token != NULL) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL;
            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }
        // Process the job
        free(currentJob->job);  // Free the job string after processing
        free(currentJob->jobID);  // Free the jobID string
        close(currentJob->socket);  // Close the socket
        free(currentJob);  // Free the job structure
        
    }
    return NULL;
}

void* controllerThread(void* arg) {
    int newsock = *((int*)arg);
    struct CommanderBuffer* CB = *((struct CommanderBuffer**)(arg + sizeof(int)));
    char buf[BUFSIZE];
    while (1) {
        int n = read(newsock, buf, BUFSIZE);
        if (n < 0) {
            perror("read");
            exit(1);
        }
        if (n == 0) {
            break;
        }
        buf[n] = '\0';
        printf("Server received %d bytes: %s\n", n, buf);

        if (strncmp(buf, "issueJob", 8) == 0) {
            char* job = buf + 9;
            printf("Job: %s\n", job);
            addJob(job, CB);
        } else if (strncmp(buf, "setConcurrency", 14) == 0) {
            char* N = buf + 15;
            printf("N: %s\n", N);
        } else if (strncmp(buf, "stop", 4) == 0) {
            char* jobID = buf + 5;
            printf("Job ID: %s\n", jobID);
        } else if (strncmp(buf, "poll", 4) == 0) {
            char* pollState = buf + 5;
            printf("Poll State: %s\n", pollState);
        } else if (strncmp(buf, "exit", 4) == 0) {
            pthread_mutex_lock(&CB->bufferMutex);
            while(CB->currentJobs > 0) {
                pthread_cond_wait(&CB->bufferCond, &CB->bufferMutex);
            }
            pthread_mutex_unlock(&CB->bufferMutex);
            freeBuffer(CB);
            exit(0);
        }
    }
    close(newsock);  // Close the socket when done
    free(arg);  // Free the allocated argument memory
    return NULL;
}

int main(int argc, char** argv) {
    int sock, newsock;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr* serverptr = (struct sockaddr*)&server;
    struct sockaddr* clientptr = (struct sockaddr*)&client;
    struct hostent* rem;

    if (argc < 4) {
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
    Commanderbuffer->allJobs = 0;
    pthread_mutex_init(&Commanderbuffer->bufferMutex, NULL);
    pthread_cond_init(&Commanderbuffer->bufferCond, NULL);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portNum);
    if (bind(sock, serverptr, sizeof(server)) < 0) {
        perror("bind");
        exit(1);
    }
    if (listen(sock, bufferSize) < 0) {
        perror("listen");
        exit(1);
    }
    printf("Listening for connections to port %d\n", portNum);

    for (int i = 0; i < threadPoolSize; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, workerThread, (void*)Commanderbuffer);
        pthread_detach(thread);  // Detach worker threads as they run indefinitely
    }

    while (1) {
        clientlen = sizeof(client);
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0) {
            perror("accept");
            exit(1);
        }
        if ((rem = gethostbyaddr((char*)&client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL) {
            perror("gethostbyaddr");
            exit(1);
        }
        printf("Accepted connection from %s\n", rem->h_name);
        printf("Accepted connection from %s\n", inet_ntoa(client.sin_addr));

        pthread_t controller;
        void* arg = malloc(sizeof(int) + sizeof(struct CommanderBuffer*));
        *((int*)arg) = newsock;
        *((struct CommanderBuffer**)(arg + sizeof(int))) = Commanderbuffer;
        pthread_create(&controller, NULL, controllerThread, arg);
        pthread_detach(controller);  // Detach controller thread
        free(arg);  // Free the allocated argument memory
    }

    close(sock);  // Close the main socket when done
    free(Commanderbuffer->jobBuffer);  // Free the job buffer
    free(Commanderbuffer);  // Free the CommanderBuffer structure
    return 0;
}
