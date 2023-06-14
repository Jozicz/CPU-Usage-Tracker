#include "printer.h"
#include "analyzer.h"
#include "watchdog.h"
#include "logger.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

void cpuUsagePrinting(short numCores){
    char line[41];
    size_t roundedUsage;

    system("clear");
    printf("+--------------------------------------+\n");
    for(short i = 0; i < numCores; i++){
        roundedUsage = (size_t)(cpuUsage[i].usage / 5.0f + 0.5f); // Percentage usage rounded in integer range 0-20
        // #-character chain for graphical representation of CPU usage
        char* usageChars = malloc((roundedUsage + 1) * sizeof(char));
        memset(usageChars, '#', roundedUsage);
        usageChars[roundedUsage] = '\0';
        // Graphical representation of each core's percentage usage
        sprintf(line, "| %-5s [%.*s%*s] %6.2f%% |", cpuUsage[i].name, (int) roundedUsage, usageChars, (20 - (int) roundedUsage), "", (double) cpuUsage[i].usage);
        printf("%s\n", line);
        
        free(usageChars);
        usageChars = NULL;
    }
    printf("+--------------------------------------+\n");
}

void* printer(void *arg){
    // Setting threads index
    short threadIndex = *(short*)arg;
    // Getting the number of CPU cores
    short numCores = getNumCores();
    // Setting logger message
    const char* defaultMessage = "Printer: CPU usage percentage data received and printed";
    // Variables for synchronizing 1-second interval between start of each loop's iteration
    clock_t startTime;
    double iterationTime, remainingTime;

    while(!sigterm){
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);

        startTime = clock();
        // Acquiring the data from Analyzer - Printer buffer
        pthread_mutex_lock(&mutex_A_P_buffer);

        while(!should_print_data && !sigterm){
            pthread_cond_wait(&cond_A_P_buffer, &mutex_A_P_buffer);
        }

        if (sigterm){
            // Setting flag for watchdog
            signalThreadActiveState(threadIndex);

            pthread_mutex_unlock(&mutex_A_P_buffer);
            break;
        }
        // Printing data
        pthread_mutex_lock(&mutex_printing);

        cpuUsagePrinting(numCores);

        pthread_mutex_unlock(&mutex_printing);
        // Setting flag for calculating
        should_print_data = 0;

        pthread_mutex_unlock(&mutex_A_P_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage);
        
        iterationTime = (double) (clock() - startTime) / CLOCKS_PER_SEC;
        remainingTime = 1.0 - iterationTime;
        // Sleep for the remainder of 1 second interval
        if(remainingTime > 0){
            usleep((unsigned int)(remainingTime * 1000000));
        }
        // Signal reader to get new data
        pthread_cond_signal(&cond_getNewData);
    }

    free(cpuUsage);
    cpuUsage = NULL;

    pthread_exit(NULL);
}
