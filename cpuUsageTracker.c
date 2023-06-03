#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_t reader_thread, analyzer_thread, printer_thread, watchdog_thread, logger_thread;

void* reader(void* arg){
    return NULL
}

void* analyzer(void* arg){
    return NULL
}

void* printer(void* arg){
    return NULL
}

void* watchdog(void* arg){
    return NULL
}

void* logger(void* arg){
    return NULL
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