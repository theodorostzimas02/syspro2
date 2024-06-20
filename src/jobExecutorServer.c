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
#include <fcntl.h>
#include "../include/types.h"

//Concurrency will be set to 1 by default
int concurrency = 1;


int threadPoolSize = 1;
int activeWorkers = 0;


struct CommanderBuffer* CB = NULL;


//Helper function to print the buffer
int printBuffer(struct job* jobBuffer, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        printf("Job %d: %s with jobID %s \n", i, jobBuffer[i].job, jobBuffer[i].jobID);
    }
    return 0;
}


//Function to remove a job from the buffer
int removeJob(struct job* jobBuffer, struct job* newJob, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {

        //If the job is found, free the memory and move the rest of the jobs to the left
        if (jobBuffer[i].job == newJob->job) {
            free(jobBuffer[i].job);
            free(jobBuffer[i].jobID);

            jobBuffer[i].job = NULL;
            for (int j = i; j < bufferSize - 1; j++) {
                jobBuffer[j] = jobBuffer[j + 1];
            }

            jobBuffer[bufferSize - 1].job = NULL;
            jobBuffer[bufferSize - 1].jobID = NULL;
            
            return 0;
        }
    }
    return -1;
}

//Function to get a job from the buffer. We could also have taken the first job in the buffer
struct job* getJob(struct job* jobBuffer, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer[i].job != NULL) {
            return &jobBuffer[i];
        }
    }
    return NULL;
}

//Function to search for a job in the buffer and return its index
int searchJob(char* jobID, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        //We use strstr to check if the jobID is a substring of the jobID in the buffer cause there might be extra characters like '\n'
        if (CB->jobBuffer[i].jobID != NULL && strstr(CB->jobBuffer[i].jobID, jobID) == 0) {
            return i;
        }
    }
    return -1;
}

//Function to stop a job
int stopJob(char* jobID) {
    printf("Stopping job\n");
    pthread_mutex_lock(&CB->bufferMutex);
    int index = searchJob(jobID, CB->bufferSize);
    if (index == -1) {
        //If the job is not found, return -1
        pthread_mutex_unlock(&CB->bufferMutex);
        return -1;
    }

    struct job* job = &CB->jobBuffer[index];
    if (job->job == NULL) {
        pthread_mutex_unlock(&CB->bufferMutex);
        return -1;
    }

    //If the job is found, we will send a message to the client and remove the job from the buffer
    write(job->socket, "Job stopped\n", 12);
    removeJob(CB->jobBuffer, job, CB->bufferSize);
    CB->currentJobs--;
    pthread_mutex_unlock(&CB->bufferMutex);

    return 0;
}

//Function to set the concurrency level 
void setConcurrencyLevel(int N) {
    pthread_mutex_lock(&CB->bufferMutex);
    concurrency = N;
    //we will signal all the worker threads to check if they can start processing a job since the concurrency level has changed
    pthread_cond_broadcast(&CB->bufferCond);
    pthread_mutex_unlock(&CB->bufferMutex);
}

void pollState(int socket) {

    pthread_mutex_lock(&CB->bufferMutex);
    //We will create a response string to send to the client. Response will include all the jobs in the buffer and will be sent in the format <jobID,job>
    char response[4096]; 
    memset(response, 0, sizeof(response)); 

    for (int i = 0; i < CB->bufferSize; i++) {
        //We will add the job to the response string if it is not NULL
        char doublet[BUFSIZE];
        if (CB->jobBuffer[i].job != NULL) {
            sprintf(doublet, "<%s,%s>\n", CB->jobBuffer[i].jobID, CB->jobBuffer[i].job);
            strcat(response, doublet);
        }
    }

    if (CB->currentJobs == 0) {
        //If there are no jobs in the buffer, we will the according message
        write(socket, "No jobs in queue\n", 17);
        pthread_mutex_unlock(&CB->bufferMutex);
        return;
    }

    write(socket, response, strlen(response));
    pthread_mutex_unlock(&CB->bufferMutex);
    return;
}



int freeBuffer() {
    //We will free all the memory allocated for the jobs in the buffer
    for (int i = 0; i < CB->bufferSize; i++) {
        if (CB->jobBuffer[i].job != NULL) {
            free(CB->jobBuffer[i].job);
            free(CB->jobBuffer[i].jobID);
        }
    }
    free(CB->jobBuffer);
    return 0;
}

void exitServer() {
    //We will free all the memory allocated for the buffer and the buffer itself as well as destroy the mutex and condition variable
    freeBuffer();
    free(CB->jobBuffer);
    free(CB);
    pthread_mutex_destroy(&CB->bufferMutex);
    pthread_cond_destroy(&CB->bufferCond);
}

int addJob(char* job, struct CommanderBuffer* p, int socket) {
    //We will create a new job, allocate memory for it
    struct job* newJob = malloc(sizeof(struct job));
    newJob->job = strdup(job); 
    newJob->jobID = malloc(sizeof(char) * 8);
    newJob->socket = socket;
    sprintf(newJob->jobID, "job_%d", p->allJobs + 1);

    
    
    pthread_mutex_lock(&p->bufferMutex);
    //If the buffer is full, we will send a message to the client and free the memory allocated for the job
    if (p->currentJobs == p->bufferSize) {
        pthread_mutex_unlock(&p->bufferMutex);
        write(socket, "Server is busy, please try again later@\n", 40);
        free(newJob->jobID);
        free(newJob->job);
        free(newJob);
        return -1;
    }

    //We will add the job to the buffer and increase the number of current jobs and all jobs
    p->jobBuffer[p->currentJobs] = *newJob;
    p->currentJobs++;
    p->allJobs++;

    
    //We will send a message to the client that the job has been submitted
    char buffer[strlen(newJob->jobID)+ strlen(newJob->job) + 20]; //20 is just for the other characters so i can be sure that the buffer is big enough
    sprintf(buffer, "Job <%s,%s> SUBMITTED@\n", newJob->jobID,newJob->job);
    write(socket, buffer, strlen(buffer));
    
    pthread_cond_signal(&p->bufferCond);

    pthread_mutex_unlock(&p->bufferMutex);
    return -1;
}


void* workerThread(void* arg) {
    struct CommanderBuffer* p = CB;
    while (1) {
        struct job* currentJob = NULL;

        pthread_mutex_lock(&p->bufferMutex); 

        // Wait for a job to be added to the buffer or for the number of active workers to be less than the concurrency level so the thread can start processing a job
        while (p->currentJobs == 0 || activeWorkers >= concurrency) {
            pthread_cond_wait(&p->bufferCond, &p->bufferMutex);
        }

        // Find a job to process
        for (int i = 0; i < p->bufferSize-1; i++) {
            if (p->jobBuffer[i].job != NULL) {
                currentJob = &p->jobBuffer[i];
                p->currentJobs--;
                activeWorkers++;
                break;
            }
        }
        
        pthread_mutex_unlock(&p->bufferMutex);

        // Execute the job
        if (currentJob != NULL) {
            char* job_command = strdup(currentJob->job);
            char* jobID = strdup(currentJob->jobID);
            int socket = currentJob->socket;

            pthread_mutex_lock(&p->bufferMutex);
            // Remove the job from the buffer cause we currently process it
            removeJob(p->jobBuffer, currentJob, p->bufferSize);
            pthread_mutex_unlock(&p->bufferMutex);

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(1);
            } else if (pid == 0) {
                // Child process executes the job
                char outputFD[BUFSIZE];
                sprintf(outputFD, "%s/%d.output",BUILD_PATH, getpid());

                //create a file to redirect the output of the job
                int fd = open(outputFD, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    perror("open");
                    exit(1);
                }

                // Redirect stdout to a file
                if (dup2(fd, 1) == -1) {   //stdout
                    perror("dup2");
                    exit(1);
                }
                close(fd);


                // Process the arguments of the job so we can pass them to execvp
                char* argv[strlen(job_command) + 1];
                int argc = 0;
                char* token = strtok(job_command, " ");
                while (token != NULL) {
                    argv[argc++] = token;
                    token = strtok(NULL, " ");
                }
                argv[argc] = NULL;
                if (execvp(argv[0], argv) == -1) {
                    write(socket, "Job execution failed\n", 21);
                }
            }else{
                // Wait for the child process to finish
                waitpid(pid, NULL, 0);

                char outputFD[BUFSIZE];
                sprintf(outputFD, "%s/%d.output",BUILD_PATH,pid);
                
                //open the file from before to read the output of the job
                int fd = open(outputFD, O_RDONLY);
                if (fd == -1) {
                    perror("open");
                }

                //buffer to read the output of the job
                char buffer[strlen(job_command)+strlen(jobID)+20];
                int bytes_read;

                //We first send output start
                sprintf(buffer, "-----%s output start-----\n", jobID);
                write(socket, buffer, strlen(buffer));
                while ((bytes_read = read(fd, buffer, 1)) > 0) {
                    write(socket, buffer, 1);
                }

                //at the end we send output end
                sprintf(buffer, "-----%s output end-----\n", jobID);

                write(socket, buffer, strlen(buffer));

                //we send the termination character to the commander
                write(socket, "@", 1);
                shutdown(socket, SHUT_RDWR);

                close(fd);
                remove(outputFD);

                pthread_mutex_lock(&p->bufferMutex);
                activeWorkers--;
                pthread_mutex_unlock(&p->bufferMutex);
                pthread_cond_signal(&p->bufferCond);
                close(socket);

            }
            free(job_command);
            free(jobID);
            
        }
    }

    
    return NULL;
}


void* controllerThread(void* arg) {

    //newsock is the socket that the controller thread will use to communicate with the client
    int newsock = *(int*)arg;
    free(arg);
    while (1) {

        //We will eventually reallocate all of this. At first we allocate memory for 1 character
        char* buf = malloc(sizeof(char) * 1);
        if (buf == NULL) {
            perror("malloc");
            exit(1);
        }

        int total_chunks = 0;
        
        //We will read the response character by character until we find the terminating character '@'
        while (1) {
            int n = read(newsock, buf + total_chunks, 1);
           if (n == 0) {
               break;
           }

            total_chunks++;
            buf = realloc(buf, total_chunks + 1);
            if (buf == NULL) {
                perror("realloc");
                exit(1);
            }

            if (buf[total_chunks - 1] == '@') {
                buf[total_chunks - 1] = ' ';
                break;
            }
        }
    
        
        buf[total_chunks] = '\0';

        if (strncmp(buf, "issueJob", 8) == 0) {
            //if the command is issueJob, we will try to add the job to the buffer
            char* job = buf + 9;


            // if (printBuffer(CB->jobBuffer, CB->bufferSize) == 0) {
            //     printf("Buffer printed\n");
            // }


            addJob(job, CB, newsock);
            break;

        } else if (strncmp(buf, "setConcurrency", 14) == 0) {
            //if the command is setConcurrency, we will change the concurrency level
            char* N = buf + 15;
            printf("N: %s\n", N);
            int conc = atoi(N);
            if (conc > threadPoolSize) {
                write(newsock, "Invalid concurrency level\n", 27);
                shutdown(newsock, SHUT_RDWR);
                close(newsock);
                return NULL;
            }
            setConcurrencyLevel(atoi(N));
            char buf[BUFSIZ];
            sprintf(buf, "CONCURRENCY SET AT %s\n", N);
            write(newsock, buf, strlen(buf));
            shutdown(newsock, SHUT_RDWR);
            close(newsock);
            break;

        } else if (strncmp(buf, "stop", 4) == 0) {
            //if the command is stop, we will try to stop the job
            char* jobID = buf + 5;
            printf("Job ID: %s\n", jobID);
            if (stopJob(jobID) == -1) {
                write(newsock, "Job not found\n", 14);
            } else {
                write(newsock, "Job stopped\n", 12);
            }
            close(newsock);
            break;

        } else if (strncmp(buf, "poll", 4) == 0) {
            //if the command is poll, we will send the state of the buffer to the commander
            pollState(newsock);
            close(newsock);
            break;

        } else if (strncmp(buf, "exit", 4) == 0) {
            //if the command is exit, we will send a message to the client and terminate the server
            pthread_mutex_lock(&CB->bufferMutex);

            // Wait for all the currently processing jobs to finish
            while(CB->currentJobs > 0) {
                pthread_cond_wait(&CB->bufferCond, &CB->bufferMutex);
            }
            pthread_mutex_unlock(&CB->bufferMutex);
            write(newsock, "SERVER TERMINATED", 18);

            // Send a message to all the commanders that are on the buffer currently
            for (int i = 0; i < CB->bufferSize; i++) {
                if (CB->jobBuffer[i].job != NULL) {
                    write(CB->jobBuffer[i].socket, "SERVER TERMINATED BEFORE EXECUTION\n", 36);
                }
            }

            // exitServer();
            close(newsock);
            exit(0);
        }

        
    }
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
    threadPoolSize = atoi(argv[3]);

    CB = malloc(sizeof(struct CommanderBuffer));
    struct CommanderBuffer* Commanderbuffer = CB;

    //Allocate memory for the buffer and initialize the buffer
    Commanderbuffer->jobBuffer = malloc(sizeof(struct job) * bufferSize);
    Commanderbuffer->bufferSize = bufferSize;
    Commanderbuffer->currentJobs = 0;
    Commanderbuffer->allJobs = 0;
    for (int i = 0; i < bufferSize; i++) {
        Commanderbuffer->jobBuffer[i].job = NULL;
        Commanderbuffer->jobBuffer[i].jobID = NULL;
    }
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

    // Create worker threads
    for (int i = 0; i < threadPoolSize; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, workerThread, (void*)Commanderbuffer);
        pthread_detach(thread);  // Detach worker threads as they run indefinitely
    }

    int* arg = malloc(sizeof(int));

    //main loop that will accept connections from the clients
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
        printf("Accepted connection from %s with socket:%d\n", inet_ntoa(client.sin_addr), newsock);

        pthread_t controller;
        //Create a controller thread for the client
        *arg = newsock;
        pthread_create(&controller, NULL, controllerThread, arg);
        //We will wait for the controller thread to finish
        pthread_join(controller, NULL);

    }

    shutdown(sock, SHUT_RDWR);  // Shutdown the socket
    close(sock);  // Close the  socket
    exitServer();
    return 0;
}
