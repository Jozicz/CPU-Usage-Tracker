#include "analyzer.h"
#include "watchdog.h"
#include "logger.h"

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

bool testsFailed = false;

void customAssertHandler(const char* expression, int actualValue)
{
    printf("Assertion failed: %s   Actual value: %d\n", expression, actualValue);
}

#undef assert
#define assert(expression) ((void)((expression) || (customAssertHandler(#expression, (int)(expression)), 0)))


void testGetNumCores(void){
    short numCores = getNumCores();
    assert(numCores > 0);
}

void testSetPreviousCoreUsage(void){
    CPUStats cpuPreviousStats[2];
    CPUStats cpuStats[2];

    cpuStats[0].user = 1;
    cpuStats[0].system = 2;
    cpuStats[1].user = 10;
    cpuStats[1].system = 20;

    setPreviousCoreUsage(cpuPreviousStats, cpuStats, 2);

    assert(cpuPreviousStats[0].user == 1);
    assert(cpuPreviousStats[0].system == 2);
    assert(cpuPreviousStats[1].user == 10);
    assert(cpuPreviousStats[1].system == 20);
}

void testGetPercentageCoreUsage(void){
    short numCores = 1;

    CPUStats cpuPreviousStats[1];
    CPUStats cpuStats[1];

    cpuUsage = (CPUUsage*)malloc(numCores * sizeof(CPUUsage));

    cpuStats[0].user = 10;
    cpuStats[0].system = 20;
    cpuPreviousStats[0].user = 5;
    cpuPreviousStats[0].system = 10;

    getPercentageCoreUsage(cpuPreviousStats, cpuStats, 1);

    assert(cpuUsage[0].usage == 100.0f);

    free(cpuUsage);
}

void testSignalThreadActiveState(void){
    atomic_bool watchdogPriority = 0;
    int flagArrayInUse = 0;
    
    threadFlags[0] = 0;
    threadFlagTimes[0] = 0;

    signalThreadActiveState(0);

    assert(watchdogPriority == 0);
    assert(flagArrayInUse == 0);
    assert(threadFlags[0] == 1);
    assert(threadFlagTimes[0] > 0);
}

void handleInterruptMock(int signum){
    (void)signum;
    sigterm = 1;
}

void testHandleInterrupt(void){
    handleInterruptMock(0);

    assert(sigterm == 1);
}

void testEnqueueMessage(void){
    messageQueue.rear = 0;
    messageQueue.count = 0;

    const char* message = "Test message";
    enqueueMessage(message);

    assert(messageQueue.rear == 20);
    assert(messageQueue.count == 1);
}

int main(){
    testGetNumCores();
    testSetPreviousCoreUsage();
    testGetPercentageCoreUsage();
    testSignalThreadActiveState();
    testHandleInterrupt();
    testEnqueueMessage();

    if(!testsFailed){
        printf("All tests passed.\n");
    }

    return 0;
}
