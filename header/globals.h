#ifndef GLOBALS_H
#define GLOBALS_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>

    // extern bool shutdownThread; // for manual thread interruption

    #define NUM_THREADS 3
    // Mutex for reader-analyzer buffer
    extern pthread_mutex_t mutex_R_A_buffer;
    // Conditional variable for reader-analyzer buffer
    extern pthread_cond_t cond_R_A_buffer;
    // Mutex for analyzer-printer buffer
    extern pthread_mutex_t mutex_A_P_buffer;
    // Conditional variable for analyzer-printer buffer
    extern pthread_cond_t cond_A_P_buffer;
    // Mutex for printing on the console
    extern pthread_mutex_t mutex_printing;
    // Conditional variable for reader - printer communication
    extern pthread_cond_t cond_getNewData;
    // Mutex for sending messages to logger
    extern pthread_mutex_t mutex_logging;
    // Conditional variable for message reading
    extern pthread_cond_t cond_messageInQueue;

#endif /* GLOBALS_H */
