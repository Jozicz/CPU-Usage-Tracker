#ifndef READER_H
#define READER_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>

    extern char* procStatData;

    char* readProcStat(void);
    void* reader(void *arg);

#endif /* READER_H */
