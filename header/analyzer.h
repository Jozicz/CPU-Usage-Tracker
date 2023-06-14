#ifndef ANALYZER_H
#define ANALYZER_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <string.h>
    #include <math.h>
    #include <stdbool.h>

    // Printing/calculating flag
    extern bool should_print_data;

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

    extern CPUUsage* cpuUsage;
    // Function reading the number of CPU cores of the system
    short getNumCores(void);
    // Function saving CPU core data into structure
    void getCoreUsage(CPUStats* cpuStats, short numCores);
    // Function saving previous core usage values
    void setPreviousCoreUsage(CPUStats* cpuPreviousStats, CPUStats* cpuStats, short numCores);
    // Function calculating percentage core usage
    void getPercentageCoreUsage(CPUStats* cpuPreviousStats, CPUStats* cpuStats, short numCores);
    void* analyzer(void *arg);

#endif /* ANALYZER_H */
