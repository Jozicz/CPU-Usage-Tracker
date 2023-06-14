#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// bool shutdownThread; // for manual thread interruption

pthread_mutex_t mutex_R_A_buffer;
pthread_cond_t cond_R_A_buffer;

pthread_mutex_t mutex_A_P_buffer;
pthread_cond_t cond_A_P_buffer;

pthread_mutex_t mutex_printing;
pthread_cond_t cond_getNewData;

pthread_mutex_t mutex_logging;
pthread_cond_t cond_messageInQueue;
