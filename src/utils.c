#include <stdio.h>
#include "utils.h"

void Randomize()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand(ts.tv_nsec);
}

