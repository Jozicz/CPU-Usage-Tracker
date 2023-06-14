#include "reader.h"
#include "watchdog.h"
#include "logger.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Global pointer to buffer
char* procStatData = NULL;

// Function that reads "/proc/stat" data and allocates it to dynamic array
char* readProcStat(void){
    FILE* procStatFile = fopen("/proc/stat", "r");
    if(procStatFile == NULL){
        printErrorMessage("*** Error opening /proc/stat\n");
        return NULL;
    }

    // Variables for storing Reader - Analyzer data
    char* R_A_buffer = NULL;
    size_t R_A_bufferSize = 0;
    ssize_t R_A_bytesRead;
    size_t R_A_totalSize = 0;

    // Calculating memory needed for the buffer
    while((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStatFile)) > 0){
        R_A_totalSize += (size_t) R_A_bytesRead;
    }
    // Resetting the position indicator to the beginning of the file to copy the data
    fseek(procStatFile, 0, SEEK_SET);
    // Allocating memory for "/proc/stat" output
    procStatData = malloc((R_A_totalSize +1) * sizeof(char));
    if(procStatData == NULL){
        printErrorMessage("*** Error allocating memory for /proc/stat\n");
        fclose(procStatFile);
        return NULL;
    }

    // Storing data in array
    size_t R_A_offset = 0;
    while((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStatFile)) > 0){
        memcpy(procStatData + R_A_offset, R_A_buffer, (size_t) R_A_bytesRead);
        R_A_offset += (size_t) R_A_bytesRead;
    }

    procStatData[R_A_totalSize] = '\0'; // Null-terminating the string

    fclose(procStatFile);
    free(R_A_buffer);

    return procStatData;
}

void* reader(void *arg){
    // Setting threads index
    short threadIndex = *(short*)arg;
    // Setting logger message
    const char* defaultMessage = "Reader: /proc/stat raw data read and sent";

    while(!sigterm){
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);
        // Reading "/proc/stat" and storing the data in dynamic array
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData != NULL && !sigterm){
            pthread_cond_wait(&cond_getNewData, &mutex_R_A_buffer);
        }

        if (sigterm){
            // Setting flag for watchdog
            signalThreadActiveState(threadIndex);
            
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }
        
        procStatData = readProcStat();

        pthread_cond_signal(&cond_R_A_buffer);

        pthread_mutex_unlock(&mutex_R_A_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage);
    }

    if(procStatData != NULL){
        free(procStatData);
        procStatData = NULL;
    }

    pthread_exit(NULL);
}
