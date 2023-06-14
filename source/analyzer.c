#include "analyzer.h"
#include "reader.h"
#include "watchdog.h"
#include "logger.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

CPUUsage* cpuUsage;

bool should_print_data = 0;

short getNumCores(void){
    short numCores = 0;
    FILE* fp = popen("nproc", "r");

    if(fp == NULL){
        printErrorMessage("*** Error executing nproc command\n");
        return -1;
    }

    if(fscanf(fp, "%hd", &numCores) != 1){
        printErrorMessage("*** Error reading nproc command output\n");
        return -1;
    }

    pclose(fp);

    return numCores;
}

void getCoreUsage(CPUStats* cpuStats, short numCores){
    // Saving individual lines of /proc/stat
    char* line = strtok(procStatData, "\n");
    // Iterating over each line
    for(short i = 0; i < numCores; i++){
        // Skipping total CPU statistics line
        line = strtok(NULL, "\n");

        if (line == NULL){
            printErrorMessage("*** Analyzer: Insufficient data for all cores\n");
            return;
        }
        // Populating the structures
        sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &cpuStats[i].user, &cpuStats[i].nice, &cpuStats[i].system, &cpuStats[i].idle, &cpuStats[i].iowait, &cpuStats[i].irq, &cpuStats[i].softirq, &cpuStats[i].steal, &cpuStats[i].guest, &cpuStats[i].guest_nice);
    }
}

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

void getPercentageCoreUsage(CPUStats* cpuPreviousStats, CPUStats* cpuStats, short numCores){
    unsigned long prevIdle, idle, prevNonIdle, nonIdle, prevTotal, total, totald, idled, used;
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
        CPU_Percentage = fmaxf(0.0f, fminf(100.0f, ((float) used / (float) totald) * 100.0f));

        cpuUsage[i].usage = CPU_Percentage;
    }
}

void* analyzer(void *arg){
    // Setting threads index
    short threadIndex = *(short*)arg;
    // Getting the number of CPU cores
    short numCores = getNumCores();
    // Setting logger message
    const char* defaultMessage1 = "Analyzer: CPU percentage usage data calculated";
    const char* defaultMessage2 = "Analyzer: CPU percentage usage data sent to printer";

    if (numCores == -1){
        printErrorMessage("*** Analyzer: Error aquiring number of CPU cores\n");
        pthread_exit(NULL);
    }
    // Allocating memory for CPU stats
    CPUStats* cpuStats = (CPUStats*)calloc((size_t)numCores, sizeof(CPUStats));
    if(cpuStats == NULL){
        printErrorMessage("*** Analyzer: Error allocating memory for CPU stats\n");
        pthread_exit(NULL);
    }
    // Allocating memory for previous CPU stats
    CPUStats* cpuPreviousStats = (CPUStats*)calloc((size_t)numCores, sizeof(CPUStats));
    if(cpuPreviousStats == NULL){
        printErrorMessage("*** Analyzer: Error allocating memory for CPU previous stats\n");
        free(cpuStats);
        pthread_exit(NULL);
    }
    // Allocating memory for percentage CPU stats
    cpuUsage = (CPUUsage*)malloc((unsigned long)numCores * sizeof(CPUUsage));
    if(cpuUsage == NULL){
        printErrorMessage("*** Analyzer: Error allocating memory for percentage CPU stats\n");
        free(cpuStats);
        free(cpuPreviousStats);
        pthread_exit(NULL);
    }
    // Setting initial cores' usages
    //setInitialCoreUsage(cpuStats, numCores);

    while(!sigterm){
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);
        // Acquiring the data from Reader - Analyzer buffer
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData == NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer, &mutex_R_A_buffer);
        }

        if (sigterm){
            // Setting flag for watchdog
            signalThreadActiveState(threadIndex);

            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }
        // Collecting core usage data
        setPreviousCoreUsage(cpuPreviousStats, cpuStats, numCores);
        getCoreUsage(cpuStats, numCores);

        free(procStatData);
        procStatData = NULL;

        pthread_mutex_unlock(&mutex_R_A_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage1);
        // Storing the data in Analyzer - Printer buffer
        pthread_mutex_lock(&mutex_A_P_buffer);

        if (sigterm || (cpuUsage == NULL)){
            // Setting flag for watchdog
            signalThreadActiveState(threadIndex);

            pthread_mutex_unlock(&mutex_A_P_buffer);
            break;
        }
        // Calculating percentage usage for each core
        getPercentageCoreUsage(cpuPreviousStats, cpuStats, numCores);
        // Setting flag for printing
        should_print_data = 1;

        pthread_cond_signal(&cond_A_P_buffer);

        pthread_mutex_unlock(&mutex_A_P_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage2);
    }

    if(procStatData != NULL){
        free(procStatData);
        procStatData = NULL;
    }
    free(cpuStats);
    cpuStats = NULL;
    free(cpuPreviousStats);
    cpuPreviousStats = NULL;

    pthread_exit(NULL);
}
