#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/limits.h>
#include "chat_common.h"
#include "prompt.h"
#include "utils.h"
#include "server.h"

mqd_t GlobalQueue;

static void addClient(Client **clientList, int id, int mqdes)
{
    if (! clientList)
        return;
    Client **cc;
    for (cc = clientList; *cc; cc = &((*cc)->next))
    {
        if ((*cc)->id == id)
            break;
        /* printf("%lx") */
    }
    if (! *cc)
    {
        *cc = (Client *)malloc(sizeof(Client));
        (*cc)->id = id;
        (*cc)->mqdes = mqdes;
        (*cc)->next = NULL;
    }
}

static void removeClient(Client **clientList, int id)
{
    if (! clientList)
        return;
    for (Client **cc = clientList; *cc; *cc = (*cc)->next)
        if ((*cc)->id == id)
        {
            Client *next = (*cc)->next;
            free(*cc);
            *cc = next;
            return;
        }
}

static void SignalHandler(int signum)
{
    if (mq_close(GlobalQueue) < 0)
        perror("close global queue");
    else
        if (mq_unlink(COMMON_QUEUE_NAME) < 0)
            perror("unlink global queue");
    putchar('\n');
    exit(0);
}

int main(int argc, char **argv)
{
    Client *clients = NULL;
    struct mq_attr attrs = { .mq_flags = 0, .mq_maxmsg = 32,
                             .mq_msgsize = IRC_MSGSIZE, .mq_curmsgs = 0 };
    GlobalQueue = mq_open(COMMON_QUEUE_NAME, O_RDONLY | O_CREAT, 0666, &attrs);
    if (GlobalQueue < 0)
        perror("global queue open");
    else
    {
        signal(SIGINT, SignalHandler);
        while (1)
        {
            Message msgbuf;
            if (mq_receive(GlobalQueue, (char *)&msgbuf, IRC_MSGSIZE, NULL) < 0)
                perror("receive from global queue");
            else
            {
                int cid;
                char *clientQueueName;
                mqd_t cmqd;
                switch (msgbuf.type)
                {
                    case LOGIN_MSG:
                        cid = msgbuf.data.loginmsg.cid;
                        clientQueueName = msgbuf.data.loginmsg.queueName;
                        printf("new client: %d %s\n", cid, clientQueueName);
                        cmqd = mq_open(clientQueueName, O_WRONLY);
                        if (cmqd < 0)
                        {
                            printf("%s: ", clientQueueName);
                            perror("open client's queue");
                        }
                        else
                            addClient(&clients, cid, cmqd);
                        break;
                    case TEXT_MSG:
                        cid = msgbuf.data.textmsg.cid;
                        printf("message from %d\n", cid);
                        for (Client *c = clients; c; c = c->next)
                        {
                            if (c->id != cid)
                            {
                                if (mq_send(c->mqdes, (char *)&msgbuf,
                                            sizeof(Message), 0) < 0)
                                {
                                    printf("cid: %d: ", c->id);
                                    perror("send message to client");
                                }
                                else
                                    printf("sent successfully to %d\n", c->id);
                            }
                        }
                        break;
                    case DISCONNECT_MSG:
                        cid = msgbuf.data.discmsg.cid;
                        printf("disconnecting %d...\n", cid);
                        Client *c;
                        for (c = clients; c; c = c->next)
                            if (c->id == cid)
                                break;
                        if (c)
                            if (mq_close(c->mqdes) < 0)
                                perror("mq_close");
                            else
                                removeClient(&clients, cid);
                }
            }
        }
    }
}
