#include "reader.h"
#include "globals.h"
#include "analyzer.h"
#include "printer.h"
#include "watchdog.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// bool shutdownThread; // for manual thread interruption

static pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread, logger_thread;
// Mutex for printing in the console
pthread_mutex_t mutex_printing;

int main(){
    signal(SIGTERM, handleInterrupt);
    signal(SIGINT, handleInterrupt);

    pthread_mutex_init(&mutex_R_A_buffer, NULL);
    pthread_mutex_init(&mutex_printing, NULL);

    pthread_cond_init(&cond_R_A_buffer, NULL);
    pthread_cond_init(&cond_getNewData, NULL);

    pthread_mutex_init(&mutex_A_P_buffer, NULL);
    pthread_cond_init(&cond_A_P_buffer, NULL);

    pthread_mutex_init(&mutex_logging, NULL);
    pthread_cond_init(&cond_messageInQueue, NULL);
    // Handling thread indexes and flags for watchdog
    short threadIndexes[3];
    for(short i = 0; i < NUM_THREADS; i++){
        threadFlags[i] = 1;
        threadFlagTimes[i] = clock();
        threadIndexes[i] = i;
    }
    // Initializing message queue
    messageQueue.front = 0;
    messageQueue.rear = 0;
    messageQueue.count = 0;

    pthread_create(&reader_thread, NULL, reader, (void*)&threadIndexes[0]);
    pthread_create(&analyzer_thread, NULL, analyzer, (void*)&threadIndexes[1]);
    pthread_create(&printer_thread, NULL, printer, (void*)&threadIndexes[2]);
    pthread_create(&watchdog_thread, NULL, watchdog, NULL);
    pthread_create(&logger_thread, NULL, logger, NULL);

    pthread_join(reader_thread, NULL);
    pthread_mutex_lock(&mutex_printing);
    printf("Killed reader\n");
    pthread_mutex_unlock(&mutex_printing);
    pthread_join(analyzer_thread, NULL);
    pthread_mutex_lock(&mutex_printing);
    printf("Killed analyzer\n");
    pthread_mutex_unlock(&mutex_printing);
    pthread_join(printer_thread, NULL);
    pthread_mutex_lock(&mutex_printing);
    printf("Killed printer\n");
    pthread_mutex_unlock(&mutex_printing);
    allThreadsDead = true;
    pthread_join(watchdog_thread, NULL);
    pthread_mutex_lock(&mutex_printing);
    printf("Killed watchdog\n");
    pthread_mutex_unlock(&mutex_printing);
    watchdogDead = true;
    pthread_join(logger_thread, NULL);
    pthread_mutex_lock(&mutex_printing);
    printf("Killed logger\n");
    pthread_mutex_unlock(&mutex_printing);

    pthread_mutex_destroy(&mutex_R_A_buffer);
    pthread_mutex_destroy(&mutex_printing);

    pthread_cond_destroy(&cond_R_A_buffer);
    pthread_cond_destroy(&cond_getNewData);

    pthread_mutex_destroy(&mutex_A_P_buffer);
    pthread_cond_destroy(&cond_A_P_buffer);

    pthread_mutex_destroy(&mutex_logging);
    pthread_cond_destroy(&cond_messageInQueue);

    return 0;
}
