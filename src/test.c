#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <mqueue.h>
#include "prompt.h"

void *thread_routine(void *arg)
{
    char *buf = (char *)malloc(1024);
    int res = mq_receive(*(mqd_t *)arg, buf, 1024, NULL);
    if (res < 0)
        perror("receive");
    else
        safePrintf("success %d\n", res);
    pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
    pthread_t thread;
    struct mq_attr attrs = { .mq_flags = 0, .mq_maxmsg = 32,
                             .mq_msgsize = 1024, .mq_curmsgs = 0 };
    mqd_t mqdes = mq_open("/lepecbeke", O_RDONLY | O_CREAT, 0666, &attrs);
    if (mqdes < 0)
        perror("open");
    initializePrompt();
    pthread_create(&thread, NULL, thread_routine, &mqdes);
    while (1)
    {
        char *str = readline("-> ");
        printf("\nres: %s\n", str);
        if (! str) break;
        free(str);
    }
    printf("%d\n", mq_close(mqdes));
    printf("%d\n", mq_unlink("/lepecbeke"));
    pthread_kill(thread, SIGINT);
    /* exit(0); */
    /* pthread_join(thread, NULL); */
    pthread_exit(NULL);
}
