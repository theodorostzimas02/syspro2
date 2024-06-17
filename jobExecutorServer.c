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
#include <signal.h>

#define BUFSIZE 1024

int concurrency = 1;
int activeWorkers = 0;

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

struct CommanderBuffer* CB = NULL;



int removeJob(struct job* jobBuffer, struct job* newJob, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer[i].job == newJob->job) {
            jobBuffer[i].job = NULL;
            return 0;
        }
    }
    return -1;
}

struct job* getJob(struct job* jobBuffer, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer[i].job != NULL) {
            return &jobBuffer[i];
        }
    }
    return NULL;
}

int searchJob(struct job* jobBuffer, char* jobID, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer[i].jobID != NULL && strcmp(jobBuffer[i].jobID, jobID) == 0) {
            return i;
        }
    }
    return -1;
}

int stopJob(char* jobID) {
    int index = searchJob(CB->jobBuffer, jobID, CB->bufferSize);
    if (index == -1) {
        printf("Job not found frrrr\n");
        return -1;
    }
    struct job* job = &CB->jobBuffer[index];
    if (job->job == NULL) {
        return -1;
    }
    close(job->socket);
    removeJob(CB->jobBuffer, job, CB->bufferSize);
    printf("Job stopped for gooooood\n");
    return 0;
}




void setConcurrencyLevel(int N) {
    pthread_mutex_lock(&CB->bufferMutex);
    concurrency = N;
    pthread_cond_broadcast(&CB->bufferCond);
    pthread_mutex_unlock(&CB->bufferMutex);
}


int freeBuffer() {
    for (int i = 0; i < CB->bufferSize; i++) {
        if (CB->jobBuffer[i].job != NULL) {
            free(CB->jobBuffer[i].job);
            free(CB->jobBuffer[i].jobID);
        }
    }
    free(CB->jobBuffer);
    return 0;
}

int addJob(char* job, struct CommanderBuffer* p, int socket) {
    struct job* newJob = malloc(sizeof(struct job));
    newJob->job = strdup(job); 
    newJob->jobID = malloc(sizeof(char) * 8);
    newJob->socket = socket;
    sprintf(newJob->jobID, "job_%d", p->allJobs + 1);

    pthread_mutex_lock(&p->bufferMutex);
    if (p->currentJobs == p->bufferSize) {
        pthread_mutex_unlock(&p->bufferMutex);
        write(socket, "Server is busy, please try again later\n", 40);
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
            char buffer[BUFSIZE];
            sprintf(buffer, "Job <%s,%s> SUBMITTED\n", newJob->jobID,newJob->job);
            write(socket, buffer, strlen(buffer));
            pthread_cond_signal(&p->bufferCond);
            pthread_mutex_unlock(&p->bufferMutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&p->bufferMutex);
    return -1;
}



void* workerThread(void* arg) {
    printf("Worker thread created\n");
    struct CommanderBuffer* p = CB;
    while (1) {
        struct job* currentJob = NULL;

        pthread_mutex_lock(&p->bufferMutex); // Lock mutex to safely access shared data

        while (p->currentJobs == 0 || activeWorkers >= concurrency) {
            pthread_cond_wait(&p->bufferCond, &p->bufferMutex);
        }

        printf("Worker thread running\n");

        // Find a job to process
        for (int i = 0; i < p->bufferSize; i++) {
            if (p->jobBuffer[i].job != NULL) {
                currentJob = &p->jobBuffer[i];
                p->currentJobs--;
                activeWorkers++;
                break;
            }
        }
        
        pthread_mutex_unlock(&p->bufferMutex); // Unlock mutex after accessing shared data

        if (currentJob != NULL) {
            printf("Worker thread processing job ID: %s\n", currentJob->jobID);
            printf("Worker thread processing job: %s\n", currentJob->job);
            char* job_command = strdup(currentJob->job);
            pthread_mutex_lock(&p->bufferMutex);
            removeJob(p->jobBuffer, currentJob, p->bufferSize);
            pthread_mutex_unlock(&p->bufferMutex);

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(1);
            } else if (pid == 0) {
                // Child process executes the job
                printf("Executing job: %s\n", job_command);
                char* argv[32];
                int argc = 0;
                char* token = strtok(job_command, " ");
                while (token != NULL) {
                    argv[argc++] = token;
                    token = strtok(NULL, " ");
                }
                argv[argc] = NULL;
                execvp(argv[0], argv);
                perror("execvp");
                exit(1);
            }else{
                int status;
                waitpid(pid, &status, 0);
                printf("Job done i guess\n");
                pthread_mutex_lock(&p->bufferMutex);
                activeWorkers--;
                pthread_mutex_unlock(&p->bufferMutex);

                free(currentJob->job);
                free(currentJob->jobID);
                close(currentJob->socket);

            }
            
        }
    }
    return NULL;
}


void* controllerThread(void* arg) {
    int newsock = *(int*)arg;
    free(arg);
    printf("Socket: %d\n", newsock);
    while (1) {
        char buf[BUFSIZE];
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
            addJob(job, CB, newsock);
        } else if (strncmp(buf, "setConcurrency", 14) == 0) {
            char* N = buf + 15;
            printf("N: %s\n", N);
            setConcurrencyLevel(atoi(N));
            char buf[BUFSIZ];
            sprintf(buf, "CONCURRENCY SET AT %s\n", N);
            write(newsock, buf, strlen(buf));
        } else if (strncmp(buf, "stop", 4) == 0) {
            char* jobID = buf + 5;
            printf("Job ID: %s\n", jobID);
            if (stopJob(jobID) == 0) {
                write(newsock, "Job stopped\n", 12);
            } else {
                write(newsock, "Job not found\n", 14);
            }

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
    // close(newsock);  // Close the socket when done
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

    CB = malloc(sizeof(struct CommanderBuffer));
    struct CommanderBuffer* Commanderbuffer = CB;
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
        printf("Socket: %d\n", newsock);

        pthread_t controller;
        int* arg = malloc(sizeof(int));
        *arg = newsock;
        pthread_create(&controller, NULL, controllerThread, arg);
        pthread_detach(controller);  // Detach controller thread
        
    }

    shutdown(sock, SHUT_RDWR);  // Shutdown the socket
    free(Commanderbuffer->jobBuffer);  // Free the job buffer
    free(Commanderbuffer);  // Free the CommanderBuffer structure
    return 0;
}
