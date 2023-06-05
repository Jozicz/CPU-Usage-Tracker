#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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

// Flag for catching SIGTERM
volatile sig_atomic_t sigterm = 0;

// SIGTERM handler
void handleInterrupt(){
    sigterm = 1;
    pthread_cond_signal(&cond_R_A_buffer_empty);
    pthread_cond_signal(&cond_R_A_buffer_full);
}

// Function that reads "/proc/stat" data and allocates it to dynamic array
char* readProcStat(){
    FILE* procStatFile = fopen("/proc/stat", "r");
    if(procStatFile == NULL){
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
    while((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStatFile)) != (size_t) -1){
        R_A_totalSize += R_A_bytesRead;
    }

    // Resetting the position indicator to the beginning of the file to copy the data
    fseek(procStatFile, 0, SEEK_SET);

    // Allocating memory for "/proc/stat" output
    procStatData = malloc((R_A_totalSize +1) * sizeof(char));
    if(procStatData == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error allocating memory\n");
        pthread_mutex_unlock(&mutex_printing);
        fclose(procStatFile);
        return NULL;
    }

    // Storing data in array
    size_t R_A_offset = 0;
    while ((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStatFile)) != (size_t) -1) {
        memcpy(procStatData + R_A_offset, R_A_buffer, R_A_bytesRead);
        R_A_offset += R_A_bytesRead;
    }

    procStatData[R_A_totalSize] = '\0'; // Null-terminating the string

    fclose(procStatFile);
    free(R_A_buffer);

    return procStatData;
}

void* reader(){
    while(!sigterm){
        // Reading "/proc/stat" and storing the data in dynamic array
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData != NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer_empty, &mutex_R_A_buffer);
        }

        if (sigterm) {
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }

        procStatData = readProcStat();

        pthread_cond_signal(&cond_R_A_buffer_full);

        pthread_mutex_unlock(&mutex_R_A_buffer);
    }
    free(procStatData);
    pthread_exit(NULL);
}

void* analyzer(){
    while(!sigterm){
        // Acquiring the data from buffer
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData == NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer_full, &mutex_R_A_buffer);
        }

        if (sigterm) {
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
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
    pthread_exit(NULL);
}

// void* printer(){
//     pthread_exit(NULL);
// }

// void* watchdog(){
//     pthread_exit(NULL);
// }

// void* logger(){
//     pthread_exit(NULL);
// }

int main(){
    signal(SIGTERM, handleInterrupt);

    pthread_mutex_init(&mutex_R_A_buffer, NULL);
    pthread_mutex_init(&mutex_printing, NULL);
    pthread_cond_init(&cond_R_A_buffer_full, NULL);
    pthread_cond_init(&cond_R_A_buffer_empty, NULL);

    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&analyzer_thread, NULL, analyzer, NULL);
    //pthread_create(&printer_thread, NULL, printer, NULL);
    //pthread_create(&watchdog_thread, NULL, watchdog, NULL);
    //pthread_create(&logger_thread, NULL, logger, NULL);
    
    pthread_join(reader_thread, NULL);
    printf("Killed reader\n");
    pthread_join(analyzer_thread, NULL);
    printf("Killed analyzer\n");
    //pthread_join(printer_thread, NULL);
    //pthread_join(watchdog_thread, NULL);
    //pthread_join(logger_thread, NULL);

    pthread_mutex_destroy(&mutex_R_A_buffer);
    pthread_mutex_destroy(&mutex_printing);
    pthread_cond_destroy(&cond_R_A_buffer_full);
    pthread_cond_destroy(&cond_R_A_buffer_empty);

    return 0;
}