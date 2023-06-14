#include "reader.h"
#include "globals.h"
#include "analyzer.h"
#include "printer.h"
#include "watchdog.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_t threads[NUM_THREADS + 2];

static short threadIndexes[NUM_THREADS];

static void initializeMutexesAndConditions() {
    pthread_mutex_init(&mutex_R_A_buffer, NULL);
    pthread_mutex_init(&mutex_printing, NULL);
    pthread_mutex_init(&mutex_A_P_buffer, NULL);
    pthread_mutex_init(&mutex_logging, NULL);

    pthread_cond_init(&cond_R_A_buffer, NULL);
    pthread_cond_init(&cond_getNewData, NULL);
    pthread_cond_init(&cond_A_P_buffer, NULL);
    pthread_cond_init(&cond_messageInQueue, NULL);
}

static void createThreads() {
    pthread_create(&threads[0], NULL, reader, (void*)&threadIndexes[0]);
    pthread_create(&threads[1], NULL, analyzer, (void*)&threadIndexes[1]);
    pthread_create(&threads[2], NULL, printer, (void*)&threadIndexes[2]);
    pthread_create(&threads[3], NULL, watchdog, NULL);
    pthread_create(&threads[4], NULL, logger, NULL);
}

static void joinThreads() {
    for(int i = 0; i < NUM_THREADS + 2; i++){
        pthread_join(threads[i], NULL);
        pthread_mutex_lock(&mutex_printing);
        printf("Killed thread %d\n", i);
        pthread_mutex_unlock(&mutex_printing);

        if(i == 2){
            allThreadsDead = true;
        }
        else if(i == 3){
            watchdogDead = true;
        }
    }
}

static void destroyMutexesAndConditions() {
    pthread_mutex_destroy(&mutex_R_A_buffer);
    pthread_mutex_destroy(&mutex_printing);
    pthread_mutex_destroy(&mutex_A_P_buffer);
    pthread_mutex_destroy(&mutex_logging);

    pthread_cond_destroy(&cond_R_A_buffer);
    pthread_cond_destroy(&cond_getNewData);
    pthread_cond_destroy(&cond_A_P_buffer);
    pthread_cond_destroy(&cond_messageInQueue);
}

int main(){
    signal(SIGTERM, handleInterrupt);
    signal(SIGINT, handleInterrupt);

    initializeMutexesAndConditions();
    // Handling thread indexes and flags for watchdog
    for(short i = 0; i < NUM_THREADS; i++){
        threadFlags[i] = 1;
        threadFlagTimes[i] = clock();
        threadIndexes[i] = i;
    }
    // Initializing message queue
    messageQueue.front = 0;
    messageQueue.rear = 0;
    messageQueue.count = 0;

    createThreads();

    joinThreads();

    destroyMutexesAndConditions();

    return 0;
}
