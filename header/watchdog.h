#ifndef WATCHDOG_H
#define WATCHDOG_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <limits.h>
    #include <stdbool.h>
    #include <stdatomic.h>
    #include <signal.h>
    #include <unistd.h>

    // Conditional variables for watchdog
    extern atomic_bool watchdogPriority;
    extern int flagArrayInUse;
    extern volatile bool allThreadsDead;
    // Flag for catching SIGTERM
    extern volatile sig_atomic_t sigterm;
    // Thread time flags
    #define NUM_THREADS 3
    extern bool threadFlags[NUM_THREADS];
    extern long threadFlagTimes[NUM_THREADS];

    // Function for notifying watchdog that thread is alive
    void signalThreadActiveState(short threadIndex);
    // Function for catching interrupt signal
    void handleInterrupt(int signum);
    void* watchdog(void *args);

#endif /* WATCHDOG_H */
