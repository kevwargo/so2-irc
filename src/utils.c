#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "utils.h"

void Randomize()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand(ts.tv_nsec);
}

void perror_die(char *msg)
{
    perror(msg);
    exit(errno);
}

