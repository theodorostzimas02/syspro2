#include <pthread.h>

#define BUFSIZE 1024
#define BUILD_PATH "build"

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