#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include "chat_common.h"
#include "utils.h"
#include "prompt.h"

mqd_t GlobalQueue;
mqd_t ClientQueue;
char ClientQueueName[NAME_MAX];

static void QuitHandle(int signum)
{
    printf("quiting...\n");
    if (mq_close(GlobalQueue) < 0)
        perror_die("close global queue");
    else
        if (mq_close(ClientQueue) < 0)
            perror_die("close client queue");
        else
            if (mq_unlink(ClientQueueName) < 0)
                perror_die("unlink client queue");
    exit(0);
}

static void *queueListen(msgbuf)
    Message *msgbuf;
{
    while (1)
    {
        if (mq_receive(ClientQueue, (char *)msgbuf, IRC_MSGSIZE, NULL) < 0)
            perror_die("receive text message");
        else
        {
            safePrintf("message from %d: %s\n", msgbuf->data.textmsg.cid,
                       msgbuf->data.textmsg.text);
        }
    }
}

int main(int argc, char **argv)
{
    GlobalQueue = mq_open(COMMON_QUEUE_NAME, O_WRONLY);
    if (GlobalQueue < 0)
        perror_die("open global queue");
    else
    {
        int pid = getpid();
        sprintf(ClientQueueName, CLIENT_QUEUE_NAME_FORMAT, pid);
        struct mq_attr attrs = { .mq_flags = 0, .mq_maxmsg = 32,
                                 .mq_msgsize = IRC_MSGSIZE, .mq_curmsgs = 0 };
        ClientQueue = mq_open(ClientQueueName, O_RDONLY | O_CREAT, 0666, &attrs);
        if (ClientQueue < 0)
            perror_die("open client queue");
        else
        {
            Message msgbuf;
            msgbuf.type = LOGIN_MSG;
            msgbuf.data.loginmsg.cid = pid;
            strcpy(msgbuf.data.loginmsg.queueName, ClientQueueName);
            if (mq_send(GlobalQueue, (char *)&msgbuf, sizeof(Message), 0) < 0)
                perror_die("send client's queuename");
            else
            {
                initializePrompt();
                signal(SIGINT, QuitHandle);
                /* signal(SIGSEGV, QuitHandle); */
                pthread_t queueListenerThread;
                /* odpala wątek, który czeka na wiadomości z kolejki */
                pthread_create(&queueListenerThread, NULL, queueListen, &msgbuf);
                while (1)
                {
                    /* readline (w odróżnieniu od scanf-a) umożliwia
                       edytowanie wprowadzanej linii:
                       można używać strzałek itd.
                    */
                    char *string = readline("-> ");
                    if (! string)
                        break;
                    if (strlen(string) >= IRC_MAXTEXTSIZE)
                    {
                        safePrintf("Text message is too long, cutting to %d bytes.\n",
                                   IRC_MAXTEXTSIZE - 1);
                        string[IRC_MAXTEXTSIZE - 1] = '\0';
                    }
                    if (strncmp(string, "exit", 4) == 0)
                        break;
                    msgbuf.type = TEXT_MSG;
                    msgbuf.data.textmsg.cid = pid;
                    strcpy(msgbuf.data.textmsg.text, string);
                    if (mq_send(GlobalQueue, (char *)&msgbuf, sizeof(Message), 0) < 0)
                        perror_die("send text message");
                    free(string);
                }
                QuitHandle(0);
                pthread_exit(NULL);
            }
        }
    }
}
