#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread, logger_thread;

// Mutex for reader-analyzer buffer
pthread_mutex_t mutex_R_A_buffer;
// Mutex for printing on the console
pthread_mutex_t mutex_printing;

// Conditional variables for reader-analyzer buffer
pthread_cond_t cond_R_A_buffer_full;
pthread_cond_t cond_R_A_buffer_empty;

// Global pointer to buffer
char* procStatData = NULL;

bool should_print_data = 0;

// Mutex for analyzer-printer buffer
pthread_mutex_t mutex_A_P_buffer;
// Conditional variable for analyzer-printer buffer
pthread_cond_t cond_A_P_buffer;

// Flag for catching SIGTERM
volatile sig_atomic_t sigterm = 0;

// SIGTERM handler
void handleInterrupt(){
    sigterm = 1;
    pthread_cond_signal(&cond_R_A_buffer_empty);
    pthread_cond_signal(&cond_R_A_buffer_full);
    pthread_cond_signal(&cond_A_P_buffer);
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

    // Variables for storing Reader - Analyzer data
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
        printf("Error allocating memory for /proc/stat\n");
        pthread_mutex_unlock(&mutex_printing);
        fclose(procStatFile);
        return NULL;
    }

    // Storing data in array
    size_t R_A_offset = 0;
    while ((R_A_bytesRead = getline(&R_A_buffer, &R_A_bufferSize, procStatFile)) != (size_t) -1){
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

        if (sigterm){
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

typedef struct{
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;
    unsigned long guest;
    unsigned long guest_nice;
} CPUStats;

typedef struct{
     char name[6];
     float usage;
} CPUUsage;

// Function reading the number of CPU cores of the system
short getNumCores(){
    short numCores = 0;
    FILE* fp = popen("nproc", "r");

    if(fp == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error executing nproc command\n");
        pthread_mutex_unlock(&mutex_printing);
        return -1;
    }

    if(fscanf(fp, "%hd", &numCores) != 1){
        pthread_mutex_lock(&mutex_printing);
        printf("Error reading nproc command output\n");
        pthread_mutex_unlock(&mutex_printing);
        return -1;
    }

    pclose(fp);

    return numCores;
}

// Function saving CPU core data into structure
void getCoreUsage(CPUStats* cpuStats, short numCores){
    // Saving individual lines of /proc/stat
    char* line = strtok(procStatData, "\n");
    // Iterating over each line
    for(short i = 0; i < numCores; i++){
        // Skipping total CPU statistics line
        line = strtok(NULL, "\n");

        if (line == NULL) {
            pthread_mutex_lock(&mutex_printing);
            printf("Insufficient data for all cores\n");
            pthread_mutex_unlock(&mutex_printing);
            return;
        }
        // Populating the structures
        sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &cpuStats[i].user, &cpuStats[i].nice, &cpuStats[i].system, &cpuStats[i].idle, &cpuStats[i].iowait, &cpuStats[i].irq, &cpuStats[i].softirq, &cpuStats[i].steal, &cpuStats[i].guest, &cpuStats[i].guest_nice);
    }
}

// Function setting initial values of structure elements to 0
void setInitialCoreUsage(CPUStats* cpuStats, short numCores){
    for(short i = 0; i < numCores; i++){
        cpuStats[i].user = 0;
        cpuStats[i].nice = 0;
        cpuStats[i].system = 0;
        cpuStats[i].idle = 0;
        cpuStats[i].iowait = 0;
        cpuStats[i].irq = 0;
        cpuStats[i].softirq = 0;
        cpuStats[i].steal = 0;
        cpuStats[i].guest = 0;
        cpuStats[i].guest_nice = 0;
    }
}

// Function saving previous core usage values
void setPreviousCoreUsage(CPUStats* cpuPreviousStats, CPUStats* cpuStats, short numCores){
    for(short i = 0; i < numCores; i++){
        cpuPreviousStats[i].user = cpuStats[i].user;
        cpuPreviousStats[i].nice = cpuStats[i].nice;
        cpuPreviousStats[i].system = cpuStats[i].system;
        cpuPreviousStats[i].idle = cpuStats[i].idle;
        cpuPreviousStats[i].iowait = cpuStats[i].iowait;
        cpuPreviousStats[i].irq = cpuStats[i].irq;
        cpuPreviousStats[i].softirq = cpuStats[i].softirq;
        cpuPreviousStats[i].steal = cpuStats[i].steal;
        cpuPreviousStats[i].guest = cpuStats[i].guest;
        cpuPreviousStats[i].guest_nice = cpuStats[i].guest_nice;
    }
}

// Function calculating percentage core usage
void getPercentageCoreUsage(CPUStats* cpuPreviousStats, CPUStats* cpuStats, CPUUsage* cpuUsage, short numCores){
    unsigned long prevIdle;
    unsigned long idle;
    unsigned long prevNonIdle;
    unsigned long nonIdle;
    unsigned long prevTotal;
    unsigned long total;
    unsigned long totald;
    unsigned long idled;
    unsigned long used;
    float CPU_Percentage;

    for(short i = 0; i < numCores; i++){
        sprintf(cpuUsage[i].name, "cpu%d", i);

        prevIdle = cpuPreviousStats[i].idle + cpuPreviousStats[i].iowait;
        idle = cpuStats[i].idle + cpuStats[i].iowait;
        
        prevNonIdle = cpuPreviousStats[i].user + cpuPreviousStats[i].nice + cpuPreviousStats[i].system + cpuPreviousStats[i].irq + cpuPreviousStats[i].steal;
        nonIdle = cpuStats[i].user + cpuStats[i].nice + cpuStats[i].system + cpuStats[i].irq + cpuStats[i].steal;

        prevTotal = prevIdle + prevNonIdle;
        total = idle + nonIdle;

        totald = total - prevTotal;
        idled = idle - prevIdle;

        used = totald - idled;
        CPU_Percentage = ((float) used/totald) * 100.0;

        cpuUsage[i].usage = CPU_Percentage;
    }
}

//int numCores;
CPUUsage* cpuUsage;

void* analyzer(){
    // Getting the number of CPU cores
    short numCores = getNumCores();

    if (numCores == -1){
        pthread_mutex_lock(&mutex_printing);
        printf("Error aquiring number of CPU cores\n");
        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for CPU stats
    CPUStats* cpuStats = malloc(numCores * sizeof(CPUStats));
    if(cpuStats == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error allocating memory for CPU stats\n");
        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for previous CPU stats
    CPUStats* cpuPreviousStats = malloc(numCores * sizeof(CPUStats));
    if(cpuPreviousStats == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error allocating memory for CPU previous stats\n");
        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for percentage CPU stats
    cpuUsage = malloc(numCores * sizeof(CPUUsage));
    if(cpuUsage == NULL){
        pthread_mutex_lock(&mutex_printing);
        printf("Error allocating memory for percentage CPU stats\n");
        pthread_mutex_unlock(&mutex_printing);
    }
    // Setting initial cores' usages to 0
    setInitialCoreUsage(cpuStats, numCores);

    while(!sigterm){
        // Acquiring the data from Reader - Analyzer buffer
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData == NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer_full, &mutex_R_A_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }

        // Collecting core usage data
        setPreviousCoreUsage(cpuPreviousStats, cpuStats, numCores);
        getCoreUsage(cpuStats, numCores);

        free(procStatData);
        procStatData = NULL;

        pthread_cond_signal(&cond_R_A_buffer_empty);

        pthread_mutex_unlock(&mutex_R_A_buffer);

        // Storing the data in Analyzer - Printer buffer
        pthread_mutex_lock(&mutex_A_P_buffer);

        while(should_print_data && !sigterm){
            pthread_cond_wait(&cond_A_P_buffer, &mutex_A_P_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_A_P_buffer);
            break;
        }

        // Calculating percentage usage for each core
        getPercentageCoreUsage(cpuPreviousStats, cpuStats, cpuUsage, numCores);
        // Switching flag for printing
        should_print_data = 1;

        pthread_cond_signal(&cond_A_P_buffer);

        pthread_mutex_unlock(&mutex_A_P_buffer);
    }

    free(cpuStats);
    cpuStats = NULL;
    free(cpuPreviousStats);
    cpuPreviousStats = NULL;
    free(cpuUsage);
    cpuUsage = NULL;

    pthread_exit(NULL);
}

void* printer(){
    // Getting the number of CPU cores
    short numCores = getNumCores();

    while(!sigterm){
        // Acquiring the data from Analyzer - Printer buffer
        pthread_mutex_lock(&mutex_A_P_buffer);

        while(!should_print_data && !sigterm){
            pthread_cond_wait(&cond_A_P_buffer, &mutex_A_P_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_A_P_buffer);
            break;
        }

        // Printing data
        pthread_mutex_lock(&mutex_printing);
        system("clear");
        for (short i = 0; i < numCores; i++) {
            printf("%s:\n", cpuUsage[i].name);
            printf("\tCPU Percentage: %.2f%%\n\n", cpuUsage[i].usage);
        }
        pthread_mutex_unlock(&mutex_printing);
        // Switching flag for calculating
        should_print_data = 0;

        pthread_cond_signal(&cond_A_P_buffer);

        pthread_mutex_unlock(&mutex_A_P_buffer);
        
        sleep(1);
    }
    
    pthread_exit(NULL);
}

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

    pthread_mutex_init(&mutex_A_P_buffer, NULL);
    pthread_cond_init(&cond_A_P_buffer, NULL);

    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&analyzer_thread, NULL, analyzer, NULL);
    pthread_create(&printer_thread, NULL, printer, NULL);
    //pthread_create(&watchdog_thread, NULL, watchdog, NULL);
    //pthread_create(&logger_thread, NULL, logger, NULL);
    
    pthread_join(reader_thread, NULL);
    printf("Killed reader\n");
    pthread_join(analyzer_thread, NULL);
    printf("Killed analyzer\n");
    pthread_join(printer_thread, NULL);
    printf("Killed printer\n");
    //pthread_join(watchdog_thread, NULL);
    //pthread_join(logger_thread, NULL);

    pthread_mutex_destroy(&mutex_R_A_buffer);
    pthread_mutex_destroy(&mutex_printing);
    pthread_cond_destroy(&cond_R_A_buffer_full);
    pthread_cond_destroy(&cond_R_A_buffer_empty);

    pthread_mutex_destroy(&mutex_A_P_buffer);
    pthread_cond_destroy(&cond_A_P_buffer);

    return 0;
}