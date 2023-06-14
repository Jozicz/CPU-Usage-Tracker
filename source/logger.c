#include "logger.h"
#include "watchdog.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

volatile bool watchdogDead = false;
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

void printErrorMessage(const char* errorMessage){
    pthread_mutex_lock(&mutex_printing);

    printf("%s\n", errorMessage);
    enqueueMessage(errorMessage);

    pthread_mutex_unlock(&mutex_printing);
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
            if(watchdogDead){
                break;
            }
            else{
                pthread_mutex_unlock(&mutex_logging);
                usleep(200000);
                continue;
            }
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
