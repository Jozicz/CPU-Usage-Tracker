#ifndef LOGGER_H
#define LOGGER_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <time.h>
    #include <stdbool.h>
    #include <string.h>

    // Variables for logger
    #define MAX_MESSAGE_LENGTH 100
    #define MAX_QUEUE_SIZE 20
    extern volatile bool watchdogDead;

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
    
    extern MessageQueue messageQueue;

    // Function for adding new message to the message queue
    void enqueueMessage(const char* message);
    void* logger(void *args);

#endif /* LOGGER_H */
