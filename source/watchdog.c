#include "watchdog.h"
#include "logger.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <signal.h>
#include <unistd.h>

atomic_bool watchdogPriority = false;
int flagArrayInUse = 0;
volatile bool allThreadsDead = false;

volatile sig_atomic_t sigterm = 0;

bool threadFlags[NUM_THREADS];
long threadFlagTimes[NUM_THREADS];

void signalThreadActiveState(short threadIndex){
    clock_t time;

    while(atomic_load(&watchdogPriority)){
    }

    __atomic_fetch_add(&flagArrayInUse, 1, __ATOMIC_RELAXED);
    // Set flag and last notification time for thread
    threadFlags[threadIndex] = 1;
    time = clock();
    threadFlagTimes[threadIndex] = time;

    __atomic_fetch_sub(&flagArrayInUse, 1, __ATOMIC_RELAXED);
}

void handleInterrupt(int signum){
    (void)signum;
    sigterm = 1;

    const char* message = "*** SIGTERM signal caught - killing all threads\n";
    // Sending message to logger
    enqueueMessage(message);

    pthread_cond_signal(&cond_getNewData);
    pthread_cond_signal(&cond_R_A_buffer);
    pthread_cond_signal(&cond_A_P_buffer);
    pthread_cond_signal(&cond_messageInQueue);
}

void* watchdog(void *args __attribute__((unused))){
    short interruptFlag = -1;
    int canReadFlagArray = 1;

    double iterationTime, remainingTime;

    long oldestNotification = LONG_MAX;
    long oldestNotificationSleep;

    while(!sigterm){
        // Set priority for reading thread flags
        atomic_store(&watchdogPriority, true);

        oldestNotificationSleep = LONG_MAX;
        
        // Checking if any thread is now trying to signal its active state
        while(canReadFlagArray && !sigterm){
            __atomic_load(&flagArrayInUse, &canReadFlagArray, __ATOMIC_RELAXED);
        }

        if(sigterm){
            break;
        }

        canReadFlagArray = 1;
        // Check thread flags
        for(short i = 0; i < NUM_THREADS; i++){
            if((oldestNotificationSleep - threadFlagTimes[i]) > 0){
                oldestNotificationSleep = threadFlagTimes[i];
            }
            if(threadFlags[i] != 1){
                interruptFlag = i; // Assign the index of thread that stopped working
                break;
            }
        }

        if(interruptFlag != -1){
            atomic_store(&watchdogPriority, false);

            handleInterrupt(0); // Shutdown the process
            // Wait until all threads are dead
            do{
                usleep(100000);
            } while(!allThreadsDead);
            char message[MAX_MESSAGE_LENGTH];

            pthread_mutex_lock(&mutex_printing);

            for(short i = 0; i < NUM_THREADS; i++){
                if(threadFlags[i] == 0){
                    sprintf(message, "*** Thread %hi stopped working", i);
                    printf("%s\n", message);
                    enqueueMessage(message);

                    if((oldestNotification - threadFlagTimes[i]) > 0){
                        oldestNotification = threadFlagTimes[i];
                        interruptFlag = i;
                    }
                }
            }
            sprintf(message, "Most probable culprit: Thread %hi", interruptFlag);
            printf("%s\n", message);
            enqueueMessage(message);

            pthread_mutex_unlock(&mutex_printing);

            break;
        }
        // Reset thread flags
        memset(threadFlags, 0, (NUM_THREADS) * sizeof(bool));

        atomic_store(&watchdogPriority, false);

        iterationTime = (double) (clock() - oldestNotificationSleep) / CLOCKS_PER_SEC;
        remainingTime = 2.0 - iterationTime;
        // Sleep for the remainder of 2 second interval
        if(remainingTime > 0){
            usleep((unsigned int)(remainingTime * 1000000));
        }
    }

    pthread_exit(NULL);
}
