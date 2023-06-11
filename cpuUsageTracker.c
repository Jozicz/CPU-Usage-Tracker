#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdatomic.h>
#include <limits.h>

// bool shutdownThread; // for manual thread interruption

pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread, logger_thread;

// Mutex for reader-analyzer buffer
pthread_mutex_t mutex_R_A_buffer;
// Mutex for printing on the console
pthread_mutex_t mutex_printing;

// Conditional variable for reader-analyzer buffer
pthread_cond_t cond_R_A_buffer;

// Global pointer to buffer
char* procStatData = NULL;
// printing/calculating flag
bool should_print_data = 0;

// Mutex for analyzer-printer buffer
pthread_mutex_t mutex_A_P_buffer;
// Conditional variable for analyzer-printer buffer
pthread_cond_t cond_A_P_buffer;

// Mutex for sending messages to logger
pthread_mutex_t mutex_logging;
// Conditional variable for message reading
pthread_cond_t cond_messageInQueue;

// Flag for catching SIGTERM
volatile sig_atomic_t sigterm = 0;

// SIGTERM handler
void handleInterrupt(int signum){
    (void)signum;
    sigterm = 1;
    pthread_cond_signal(&cond_R_A_buffer);
    pthread_cond_signal(&cond_A_P_buffer);
    pthread_cond_signal(&cond_messageInQueue);
}

// Thread indexes and flags for watchdog
#define NUM_THREADS 3

bool threadFlags[NUM_THREADS];
long threadFlagTimes[NUM_THREADS];

// Conditional variables for watchdog
atomic_bool watchdogPriority = false;
int flagArrayInUse = 0;

// Variables for logger
#define MAX_MESSAGE_LENGTH 100
#define MAX_QUEUE_SIZE 20

typedef struct{
    char message[MAX_MESSAGE_LENGTH];
    char padding[4];
    struct timespec timestamp;
} Message;

typedef struct{
    Message messages[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    char padding[4];
} MessageQueue;

// Message queue initialization
MessageQueue messageQueue;

void enqueueMessage(const char* message){
    pthread_mutex_lock(&mutex_logging);

    if(messageQueue.count == MAX_QUEUE_SIZE){
        pthread_mutex_unlock(&mutex_logging);
        return;
    }

    Message newMessage;
    // Enqueueing the message
    strncpy(newMessage.message, message, MAX_MESSAGE_LENGTH - 1);
    newMessage.message[MAX_MESSAGE_LENGTH - 1] = '\0'; // Null termination

    clock_gettime(CLOCK_REALTIME, &(newMessage.timestamp)); // Time of creation

    messageQueue.messages[messageQueue.rear] = newMessage;
    messageQueue.rear = (messageQueue.rear + 1) % MAX_QUEUE_SIZE;
    messageQueue.count++;
    // Signal the logger that new message is available
    pthread_cond_signal(&cond_messageInQueue);

    pthread_mutex_unlock(&mutex_logging);
}

void signalThreadActiveState(short threadIndex){
    clock_t time;

    while(atomic_load(&watchdogPriority)){
    }

    __atomic_fetch_add(&flagArrayInUse, 1, __ATOMIC_RELAXED);
    // Set flag and last notification time for thread
    threadFlags[threadIndex] = 1;
    time = clock();
    threadFlagTimes[threadIndex] = time;

    /*pthread_mutex_lock(&mutex_printing);
    printf("Thread %hi notifies!\n", threadIndex);
    pthread_mutex_unlock(&mutex_printing);*/

    __atomic_fetch_sub(&flagArrayInUse, 1, __ATOMIC_RELAXED);
}

// Function that reads "/proc/stat" data and allocates it to dynamic array
char* readProcStat(void){
    FILE* procStatFile = fopen("/proc/stat", "r");
    if(procStatFile == NULL){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Error opening /proc/stat";
        printf("%s\n", message);
        enqueueMessage(message);

        pthread_mutex_unlock(&mutex_printing);
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
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Error allocating memory for /proc/stat";
        printf("%s\n", message);
        enqueueMessage(message);

        pthread_mutex_unlock(&mutex_printing);
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
        // Reading "/proc/stat" and storing the data in dynamic array
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData != NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer, &mutex_R_A_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);
        
        procStatData = readProcStat();

        pthread_cond_signal(&cond_R_A_buffer);

        pthread_mutex_unlock(&mutex_R_A_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage);
    }

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
     char padding[2];
     float usage;
} CPUUsage;

// Function reading the number of CPU cores of the system
short getNumCores(void){
    short numCores = 0;
    FILE* fp = popen("nproc", "r");

    if(fp == NULL){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Error executing nproc command";
        printf("%s\n", message);
        enqueueMessage(message);

        pthread_mutex_unlock(&mutex_printing);
        return -1;
    }

    if(fscanf(fp, "%hd", &numCores) != 1){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Error reading nproc command output";
        printf("%s\n", message);
        enqueueMessage(message);
        
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

            const char* message = "Analyzer: Insufficient data for all cores";
            printf("%s\n", message);
            enqueueMessage(message);
            
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
        CPU_Percentage = fmaxf(0.0f, fminf(100.0f, ((float) used / (float) totald) * 100.0f));

        cpuUsage[i].usage = CPU_Percentage;
    }
}

CPUUsage* cpuUsage;

void* analyzer(void *arg){
    // Setting threads index
    short threadIndex = *(short*)arg;
    // Getting the number of CPU cores
    short numCores = getNumCores();
    // Setting logger message
    const char* defaultMessage = "Analyzer: CPU percentage usage data calculated and sent";

    if (numCores == -1){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Analyzer: Error aquiring number of CPU cores";
        printf("%s\n", message);
        enqueueMessage(message);
        
        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for CPU stats
    CPUStats* cpuStats = (CPUStats*)malloc((unsigned long) numCores * sizeof(CPUStats));
    if(cpuStats == NULL){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Analyzer: Error allocating memory for CPU stats";
        printf("%s\n", message);
        enqueueMessage(message);
        
        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for previous CPU stats
    CPUStats* cpuPreviousStats = (CPUStats*)malloc((unsigned long) numCores * sizeof(CPUStats));
    if(cpuPreviousStats == NULL){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Analyzer: Error allocating memory for CPU previous stats";
        printf("%s\n", message);
        enqueueMessage(message);

        pthread_mutex_unlock(&mutex_printing);
    }
    // Allocating memory for percentage CPU stats
    cpuUsage = (CPUUsage*)malloc((unsigned long) numCores * sizeof(CPUUsage));
    if(cpuUsage == NULL){
        pthread_mutex_lock(&mutex_printing);

        const char* message = "Analyzer: Error allocating memory for percentage CPU stats";
        printf("%s\n", message);
        enqueueMessage(message);
        
        pthread_mutex_unlock(&mutex_printing);
    }
    // Setting initial cores' usages
    setInitialCoreUsage(cpuStats, numCores);

    while(!sigterm){
        // Acquiring the data from Reader - Analyzer buffer
        pthread_mutex_lock(&mutex_R_A_buffer);

        while(procStatData == NULL && !sigterm){
            pthread_cond_wait(&cond_R_A_buffer, &mutex_R_A_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_R_A_buffer);
            break;
        }
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);
        // Collecting core usage data
        setPreviousCoreUsage(cpuPreviousStats, cpuStats, numCores);
        getCoreUsage(cpuStats, numCores);

        free(procStatData);
        procStatData = NULL;

        pthread_cond_signal(&cond_R_A_buffer);

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
        // Setting flag for printing
        should_print_data = 1;

        pthread_cond_signal(&cond_A_P_buffer);

        pthread_mutex_unlock(&mutex_A_P_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage);
    }
    
    free(cpuStats);
    cpuStats = NULL;
    free(cpuPreviousStats);
    cpuPreviousStats = NULL;

    pthread_exit(NULL);
}

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
        startTime = clock();
        // Acquiring the data from Analyzer - Printer buffer
        pthread_mutex_lock(&mutex_A_P_buffer);

        while(!should_print_data && !sigterm){
            pthread_cond_wait(&cond_A_P_buffer, &mutex_A_P_buffer);
        }

        if (sigterm){
            pthread_mutex_unlock(&mutex_A_P_buffer);
            break;
        }
        // Setting flag for watchdog
        signalThreadActiveState(threadIndex);
        // Printing data
        pthread_mutex_lock(&mutex_printing);

        cpuUsagePrinting(numCores);

        pthread_mutex_unlock(&mutex_printing);
        // Setting flag for calculating
        should_print_data = 0;

        pthread_cond_signal(&cond_A_P_buffer);

        pthread_mutex_unlock(&mutex_A_P_buffer);
        // Sending message to logger
        enqueueMessage(defaultMessage);
        
        iterationTime = (double) (clock() - startTime) / CLOCKS_PER_SEC;
        remainingTime = 1.0 - iterationTime;
        // Sleep for the remainder of 1 second interval
        if(remainingTime > 0){
            usleep((unsigned int)(remainingTime * 1000000));
        }
    }

    free(cpuUsage);
    cpuUsage = NULL;

    pthread_exit(NULL);
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
            handleInterrupt(0); // Shutdown the process
            char message[MAX_MESSAGE_LENGTH];

            pthread_mutex_lock(&mutex_printing);

            for(short i = 0; i < NUM_THREADS; i++){
                //printf("Thread %hi's last notification time: %ld\n", i, threadFlagTimes[i]);
                if(threadFlags[i] == 0){
                    sprintf(message, "Thread %hi stopped working", i);
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
        for(short i = 0; i < NUM_THREADS; i++){
            threadFlags[i] = 0;
        }

        atomic_store(&watchdogPriority, false);

        iterationTime = (double) (clock() - oldestNotificationSleep) / CLOCKS_PER_SEC;
        remainingTime = 2.0 - iterationTime;
        // Sleep for the remainder of 2 second interval
        pthread_mutex_lock(&mutex_printing);
        //printf("Sleeping for: %lf + %lf\n", remainingTime, iterationTime);
        pthread_mutex_unlock(&mutex_printing);

        if(remainingTime > 0){
            usleep((unsigned int)(remainingTime * 1000000));
        }
    }

    pthread_exit(NULL);
}

void* logger(void *args __attribute__((unused))){
    // Getting current time for filename
    time_t currentTime = time(NULL);
    struct tm* localTime = localtime(&currentTime);
    // Creating log file
    char logFilename[100];
    strftime(logFilename, sizeof(logFilename), "logs/log_%Y-%m-%d_%H-%M-%S.txt", localTime);

    FILE* logFile = fopen(logFilename, "w");
    
    if(logFile == NULL){
        handleInterrupt(0);

        pthread_mutex_lock(&mutex_printing);
        printf("Error creating log file\n");
        pthread_mutex_unlock(&mutex_printing);

        pthread_exit(NULL);
    }

    while(true){
        pthread_mutex_lock(&mutex_logging);
        // Waiting until there's a message in the queue
        while(messageQueue.count == 0 && !sigterm){
            pthread_cond_wait(&cond_messageInQueue, &mutex_logging);
        }

        if(messageQueue.count == 0 && sigterm){
            break;
        }
        // Dequeue message from queue
        Message message = messageQueue.messages[messageQueue.front];
        messageQueue.front = (messageQueue.front + 1) % MAX_QUEUE_SIZE;
        messageQueue.count--;

        pthread_mutex_unlock(&mutex_logging);
        // Logging message
        time_t timestampSec = message.timestamp.tv_sec;
        struct tm* messageTimestamp = localtime(&timestampSec);
        char timestamp[9];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", messageTimestamp);

        fprintf(logFile, "%s\t%s\n", timestamp, message.message);
    }
    fclose(logFile);

    pthread_exit(NULL);
}

int main(){
    signal(SIGTERM, handleInterrupt);
    signal(SIGINT, handleInterrupt);

    pthread_mutex_init(&mutex_R_A_buffer, NULL);
    pthread_mutex_init(&mutex_printing, NULL);

    pthread_cond_init(&cond_R_A_buffer, NULL);

    pthread_mutex_init(&mutex_A_P_buffer, NULL);
    pthread_cond_init(&cond_A_P_buffer, NULL);

    pthread_mutex_init(&mutex_logging, NULL);
    pthread_cond_init(&cond_messageInQueue, NULL);
    // Handling thread indexes and flags for watchdog
    short threadIndexes[3];
    for(short i = 0; i < NUM_THREADS; i++){
        threadFlags[i] = 1;
        threadFlagTimes[i] = clock();
        threadIndexes[i] = i;
    }
    // Initializing message queue
    messageQueue.front = 0;
    messageQueue.rear = 0;
    messageQueue.count = 0;

    pthread_create(&reader_thread, NULL, reader, (void*)&threadIndexes[0]);
    pthread_create(&analyzer_thread, NULL, analyzer, (void*)&threadIndexes[1]);
    pthread_create(&printer_thread, NULL, printer, (void*)&threadIndexes[2]);
    pthread_create(&watchdog_thread, NULL, watchdog, NULL);
    pthread_create(&logger_thread, NULL, logger, NULL);
    
    pthread_join(reader_thread, NULL);
    printf("Killed reader\n");
    pthread_join(analyzer_thread, NULL);
    printf("Killed analyzer\n");
    pthread_join(printer_thread, NULL);
    printf("Killed printer\n");
    pthread_join(watchdog_thread, NULL);
    printf("Killed watchdog\n");
    pthread_join(logger_thread, NULL);
    printf("Killed logger\n");

    pthread_mutex_destroy(&mutex_R_A_buffer);
    pthread_mutex_destroy(&mutex_printing);

    pthread_cond_destroy(&cond_R_A_buffer);

    pthread_mutex_destroy(&mutex_A_P_buffer);
    pthread_cond_destroy(&cond_A_P_buffer);

    return 0;
}
