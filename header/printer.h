#ifndef PRINTER_H
#define PRINTER_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <string.h>
    #include <time.h>
    #include <unistd.h>
    #include <stdbool.h>

    // Function printing out percentage core usage in the console
    void cpuUsagePrinting(short numCores);
    void* printer(void *arg);

#endif /* PRINTER_H */
