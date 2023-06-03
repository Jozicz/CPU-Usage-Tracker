#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread, logger_thread;

// Mutex for reader-analyzer buffer
pthread_mutex_t mutex_R_A_buffer;
// Mutex for printing on the console
pthread_mutex_t mutex_printing;

// Condition variables for full/empty state of the buffer
pthread_cond_t cond_R_A_buffer_full;
pthread_cond_t cond_R_A_buffer_empty;

// Global pointer for buffer
char* procStatData = NULL;

// Function that reads "/proc/stat" data and allocates it to dynamic array
char* readProcStat(){
    FILE* procStat = fopen("/proc/stat", "r");
    if(procStat == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error opening /proc/stat\n");
        pthread_mutex_unlock(&mutex_printing);
        return NULL;
    }

    // Variables for storing data
    char* R_A_buffer = NULL;
    size_t R_A_bufferSize = 0;
    size_t R_A_bytesRead;
    size_t R_A_totalSize = 0;

    // Calculating memory needed for the buffer
    while((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStat)) != -1){
        R_A_totalSize += R_A_bytesRead;
    }

    // Resetting the position indicator to the beginning of the file
    fseek(procStat, 0, SEEK_SET);

    // Allocating memory for "/proc/stat" output
    char* procStatData = malloc((R_A_totalSize +1) * sizeof(char));
    if(procStatData == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error allocating memory\n");
        pthread_mutex_unlock(&mutex_printing);
        fclose(procStat);
        return NULL;
    }

    // Storing data in array
    size_t R_A_offset = 0;
    while ((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStat)) != -1) {
        memcpy(procStatData + R_A_offset, R_A_buffer, R_A_bytesRead);
        R_A_offset += R_A_bytesRead;
    }

    procStatData[R_A_totalSize] = '\0'; // Null-terminating the string

    fclose(procStat);
    free(R_A_buffer);

    return procStatData;
}

void* reader(void* arg){
    while(1){
        // Reading "/proc/stat" and storing the data in dynamic array
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData != NULL){
            pthread_cond_wait(&cond_R_A_buffer_empty, &mutex_R_A_buffer);
        }
        procStatData = readProcStat();

        pthread_cond_signal(&cond_R_A_buffer_full);

        pthread_mutex_unlock(&mutex_R_A_buffer);
    }

    return NULL;
}

void* analyzer(void* arg){
    while(1){
        // Acquiring the data from buffer
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData == NULL){
            pthread_cond_wait(&cond_R_A_buffer_full, &mutex_R_A_buffer);
        }

        // PLACEHOLDER - printing data in console
        pthread_mutex_lock(&mutex_printing);
        system("clear");
        printf("Data from /proc/stat:\n%s", procStatData);
        pthread_mutex_unlock(&mutex_printing);

        free(procStatData);
        procStatData = NULL;

        pthread_cond_signal(&cond_R_A_buffer_empty);

        pthread_mutex_unlock(&mutex_R_A_buffer);

        sleep(1);
    }

    return NULL;
}

void* printer(void* arg){
    return NULL;
}

void* watchdog(void* arg){
    return NULL;
}

void* logger(void* arg){
    return NULL;
}

int main(){
    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&analyzer_thread, NULL, analyzer, NULL);
    pthread_create(&printer_thread, NULL, printer, NULL);
    pthread_create(&watchdog_thread, NULL, watchdog, NULL);
    pthread_create(&logger_thread, NULL, logger, NULL);

    pthread_join(reader_thread, NULL);
    pthread_join(analyzer_thread, NULL);
    pthread_join(printer_thread, NULL);
    pthread_join(watchdog_thread, NULL);
    pthread_join(logger_thread, NULL);

    return 0;
}