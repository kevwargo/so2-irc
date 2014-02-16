#include <mqueue.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/limits.h>
#include "protocol.h"
#include "prompt.h"
#include "server.h"
#include <errno.h>

mqd_t GlobalQueue;
Client *ClientList = NULL;
Channel *ChannelList = NULL;


/* CLIENT LIST SECTION */

static void addClient(Client **clptr, int id, int mqdes, char *name)
{
    if (! clptr)
        return;
    Client **cpp;
    for (cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->id == id)
            return;
    *cpp = (Client *)malloc(sizeof(Client));
    (*cpp)->id = id;
    (*cpp)->mqdes = mqdes;
    (*cpp)->name = name;
    (*cpp)->channel = NULL;
    (*cpp)->next = NULL;
}

static void removeClient(Client **clptr, int id)
{
    if (! clptr)
        return;
    for (Client **cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->id == id)
        {
            printf("removing %d\n", id);
            Client *next = (*cpp)->next;
            mq_close((*cpp)->mqdes);
            free((*cpp)->name);
            if ((*cpp)->channel)
                free((*cpp)->channel);
            free(*cpp);
            *cpp = next;
            return;
        }
}

static void clearClientList(Client **clptr)
{
    if (! clptr)
        return;
    for (Client *cp = *clptr; cp; cp = cp->next)
        removeClient(clptr, cp->id);
}

static Client *getClientById(Client *cl, int cid)
{
    if (! cl)
        return NULL;
    for (Client *cp = cl; cp; cp = cp->next)
        if (cp->id == cid)
            return cp;
    return NULL;
}

/* END CLIENT LIST SECTION */


/* CHANNEL LIST SECTION */

static void addChannel(Channel **chlptr, char *name, char private)
{
    if (! chlptr)
        return;
    Channel **chpp;
    for (chpp = chlptr; *chpp; chpp = &((*chpp)->next))
        if (strcmp((*chpp)->name, name) == 0)
        {
            printf("channel %s already exists\n", name);
            return;
        }
    *chpp = (Channel *)malloc(sizeof(Channel));
    (*chpp)->name = strdup(name);
    (*chpp)->clients = NULL;
    (*chpp)->private = private;
    (*chpp)->invited = NULL;
    (*chpp)->next = NULL;
}

static void clearChannelList(Channel **chlptr)
{
    if (! chlptr)
        return;
    for (Channel **chpp = chlptr; *chpp;)// chpp = &((*chpp)->next))
    {
        Channel **nextptr = &((*chpp)->next);
        free((*chpp)->name);
        for (ClientEntry *cep = (*chpp)->clients; cep; )
        {
            ClientEntry *next = cep->next;
            free(cep);
            cep = next;
        }
        free(*chpp);
        chpp = nextptr;
    }
}

static Channel *getChannelByName(Channel *chl, char *name)
{
    if (! name || ! chl)
        return NULL;
    for (Channel *chp = chl; chp; chp = chp->next)
        if (strcmp(chp->name, name) == 0)
            return chp;
    return NULL;
}

static void addClientToChannel(Channel *chptr, int cid)
{
    ClientEntry **cepp = &(chptr->clients);
    for (; *cepp; cepp = &((*cepp)->next))
        if ((*cepp)->cid == cid)
            return;
    *cepp = (ClientEntry *)malloc(sizeof(ClientEntry));
    (*cepp)->cid = cid;
    (*cepp)->next = NULL;
}

static void removeClientFromChannel(Channel *chptr, int cid)
{
    if (! chptr)
        return;
    for (ClientEntry **cepp = &(chptr->clients); *cepp; cepp = &((*cepp)->next))
        if ((*cepp)->cid == cid)
        {
            ClientEntry *next = (*cepp)->next;
            free(*cepp);
            *cepp = next;
            return;
        }
}

/* END CHANNEL LIST SECTION */


/* MESSAGE HANDLERS SECTION */

static void handleLogin(Message *msgbuf)
{
    int cid = msgbuf->data.loginmsg.cid;
    char *name = strdup(msgbuf->data.loginmsg.name);
    int confirmed = 1;
    char *cqname = msgbuf->data.loginmsg.queueName;
    mqd_t cmqdes = mq_open(cqname, O_WRONLY);
    for (Client *cp = ClientList; cp; cp = cp->next)
        if (strcmp(name, cp->name) == 0)
        {
            printf("user %s is already logged in\n", name);
            confirmed = 0;
            sprintf(msgbuf->data.confmsg.text,
                    "Name %s is already in use, choose other one\n", name);
            break;
        }
    if (cmqdes < 0)
    {
        perror("open client queue");
        return;
    }
    msgbuf->type = CONFIRM_MSG;
    msgbuf->data.confmsg.confirmed = confirmed;
    if (confirmed)
        sprintf(msgbuf->data.confmsg.text, "Successfully logged in as %s\n", name);
    if (mq_send(cmqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send login confirmation");
    else
        printf("login confirmation sent successfully to %d\n", cid);
    if (! confirmed)
        mq_close(cmqdes);
    else
    {
        addClient(&ClientList, cid, cmqdes, name);
        printf("client %d added to list\n", cid);
    }
}

static void handleDisconnect(Message *msgbuf)
{
    int cid = msgbuf->data.discmsg.cid;
    Client *client = getClientById(ClientList, cid);
    if (client)
    {
        Channel *channel = getChannelByName(ChannelList, client->channel);
        removeClientFromChannel(channel, cid);
        removeClient(&ClientList, cid);
        printf("client %d disconnected\n", cid);
    }
}

static void handleInfo(Message *msgbuf)
{
    int cid = msgbuf->data.srvmsg.cid;
    printf("info request from %d\n", cid);
    strcpy(msgbuf->data.srvmsg.text, "List of available server commands:\ninvite <user> <channel>\nbla-bla-bla\n");
    mqd_t cmqd = getClientById(ClientList, cid)->mqdes;
    if (mq_send(cmqd, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send info message to client");
    else
        printf("info msg sent successfully to %d\n", cid);
}

static void handleJoinChannel(Message *msgbuf)
{
    int cid = msgbuf->data.chmsg.cid;
    Client *client = getClientById(ClientList, cid);
    Channel *channel = getChannelByName(ChannelList, msgbuf->data.chmsg.channel_name);
    msgbuf->type = CONFIRM_MSG;
    int confirmed = 1;
    if (! channel)
    {
        addChannel(&ChannelList, msgbuf->data.chmsg.channel_name, 0);
        channel = getChannelByName(ChannelList, msgbuf->data.chmsg.channel_name);
        printf("created new channel %s\n", channel->name);
    }
    if (channel->private)
    {
        ClientEntry *cep = channel->clients;
        for (; cep; cep = cep->next)
            if (cep->cid == cid)
            {
                printf("client %d is gonna be added to private channel %s\n",
                       cid, channel->name);
                /* addClientToChannel(channel, cid); */
                break;
            }
        if (! cep)
        {
            printf("ERROR: client %d isn't invited to private channel %s\n",
                   cid, channel->name);
            sprintf(msgbuf->data.confmsg.text, "You are not invited to the private channel %s\n", channel->name);
            confirmed = 0;
        }
    }
    if (confirmed)
    {
        if (client->channel && strcmp(client->channel, channel->name) != 0)
        {
            printf("client %d removed from channel %s\n", cid, client->channel);
            removeClientFromChannel(channel, cid);
            free(client->channel);
            client->channel = NULL;
            msgbuf->data.confmsg.confirmed = 1;
            sprintf(msgbuf->data.confmsg.text, "Logged out from %s\n", channel->name);
            if (mq_send(client->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
                perror("send leave channel confirmation (before join)");
            else
                printf("leave channel confirmation (before join) sent successfully to %d\n", cid);
        }
        client->channel = strdup(channel->name);
        addClientToChannel(channel, cid);
        printf("added client %d to channel %s\n", cid, channel->name);
        sprintf(msgbuf->data.confmsg.text, "Joined channel %s\n", channel->name);
    }
    msgbuf->data.confmsg.confirmed = confirmed;
    if (mq_send(client->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send join channel confirmation");
    else
        printf("join channel confirmation sent successfully to %d\n", cid);
}

static void handleLeaveChannel(Message *msgbuf)
{
    int cid = msgbuf->data.chmsg.cid;
    Client *client = getClientById(ClientList, cid);
    msgbuf->type = CONFIRM_MSG;
    msgbuf->data.confmsg.confirmed = 1;
    if (client->channel)
    {
        Channel *channel = getChannelByName(ChannelList, client->channel);
        removeClientFromChannel(channel, cid);
        free(client->channel);
        client->channel = NULL;
        printf("client %d left channel %s\n", cid, channel->name);
        sprintf(msgbuf->data.confmsg.text, "Left channel %s\n", channel->name);
    }
    else
    {
        printf("ERROR; client %d isn't connected to any channel\n", cid);
        msgbuf->data.confmsg.confirmed = 0;
        sprintf(msgbuf->data.confmsg.text, "You're not connected to any channel\n");
    }
    if (mq_send(client->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send leave channel confirmation");
    else
        printf("leave channel confirmation sent successfully to %d\n", cid);
}

static void handleSendPrivateMessage(Message *msgbuf)
{
    int cid = msgbuf->data.textmsg.cid;
    Client *sender = getClientById(ClientList, cid);
    char *tgtname = msgbuf->data.textmsg.name;
    Client *target = NULL;
    int confirmed = 1;
    for (Client *cp = ClientList; cp; cp = cp->next)
        if (strcmp(cp->name, tgtname) == 0)
        {
            printf("target client: %s %d\n", cp->name, cp->id);
            target = cp;
            break;
        }
    if (! target)
    {
        printf("ERROR: no such client: %s\n", tgtname);
        confirmed = 0;
        msgbuf->data.confmsg.text[0] = '\0';
        sprintf(msgbuf->data.confmsg.text, "No such useeer: %s\n", tgtname);
        printf(" >> %s", msgbuf->data.confmsg.text);
    }
    else
    {
        strcpy(msgbuf->data.textmsg.name, sender->name);
        if (mq_send(target->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        {
            perror("send priv message");
            confirmed = 0;
            sprintf(msgbuf->data.confmsg.text,
                    "Error on server side during sending priv msg to %s\n", tgtname);
        }
        else
        {
            printf("priv msg succ sent to %d\n", target->id);
            confirmed = 1;
            sprintf(msgbuf->data.confmsg.text,
                    "Successfully sent priv msg to %s\n", tgtname);
        }
    }
    msgbuf->type = CONFIRM_MSG;
    msgbuf->data.confmsg.confirmed = confirmed;
    if (mq_send(sender->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send priv msg confirmation");
    else
        printf("priv msg confirmation sent successfully to %d\n", cid);
}

static void handleSendChannelMessage(Message *msgbuf)
{
    int cid = msgbuf->data.textmsg.cid;
    Client *sender = getClientById(ClientList, cid);
    int confirmed = 1;
    if (! sender->channel)
    {
        printf("ERROR send chmsg: client %d not connected to any channel\n", cid);
        confirmed = 0;
        strcpy(msgbuf->data.confmsg.text,
               "Cannot send message to channel since not connected to any\n");
    }
    else
    {
        Channel *channel = getChannelByName(ChannelList, sender->channel);
        strcpy(msgbuf->data.textmsg.name, sender->name);
        for (ClientEntry *cep = channel->clients; cep; cep = cep->next)
        {
            if (cep->cid == cid)
                continue;
            Client *target = getClientById(ClientList, cep->cid);
            printf("sending chmsg from %d to %d\n", cid, cep->cid);
            if (mq_send(target->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
                perror("send chmsg");
            else
                printf("chmsg sent successfully from %d to %d\n", cid, cep->cid);
        }
        sprintf(msgbuf->data.confmsg.text,
                "message successfully sent to channel %s\n", channel->name);
    }
    msgbuf->type = CONFIRM_MSG;
    msgbuf->data.confmsg.confirmed = confirmed;
    if (mq_send(sender->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send chmsg confirmation");
    else
        printf("chmsg confirmation sent successfully to %d\n", cid);
}

static void handleShowUsers(Message *msgbuf)
{
    int cid = msgbuf->data.srvmsg.cid;
    Client *client = getClientById(ClientList, cid);
    msgbuf->type = SRV_MSG;
    msgbuf->data.srvmsg.text[0] = '\0';
    char *buffer = (char *)malloc(1024);
    for (Client *cp = ClientList; cp; cp = cp->next)
    {
        strcat(msgbuf->data.srvmsg.text, cp->name);
        strcpy(buffer, "\n");
        if (cp->channel)
            sprintf(buffer, " (on channel %s)\n", cp->channel);
        strcat(msgbuf->data.srvmsg.text, buffer);
    }
    free(buffer);
    if (mq_send(client->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send show users");
    else
        printf("users stats successfully sent to %d\n", cid);
}

static void handleShowChannels(Message *msgbuf)
{
    int cid = msgbuf->data.srvmsg.cid;
    Client *client = getClientById(ClientList, cid);
    msgbuf->type = SRV_MSG;
    msgbuf->data.srvmsg.text[0] = '\0';
    char *buffer = (char *)malloc(1024);
    for (Channel *chp = ChannelList; chp; chp = chp->next)
    {
        strcat(msgbuf->data.srvmsg.text, chp->name);
        if (chp->clients)
        {
            strcat(msgbuf->data.srvmsg.text, " (users on channel: ");
            ClientEntry *cep = chp->clients;
            for (; cep->next; cep = cep->next)
            {
                sprintf(buffer, "%s, ", getClientById(ClientList, cep->cid)->name);
                strcat(msgbuf->data.srvmsg.text, buffer);
            }
            sprintf(buffer, "%s)\n", getClientById(ClientList, cep->cid)->name);
            strcat(msgbuf->data.srvmsg.text, buffer);
        }
        else
            strcat(msgbuf->data.srvmsg.text, "\n");
    }
    free(buffer);
    if (mq_send(client->mqdes, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send show channels");
    else
        printf("channels stats successfully sent to %d\n", cid);
}

static void handleServerCommand(Message *msgbuf)
{
    printf("server command request from %d: %s\n", msgbuf->data.srvmsg.cid,
           msgbuf->data.srvmsg.text);
}

/* END MESSAGE HANDLERS SECTION */


/* MAIN SECTION */

static void quit(int signum)
{
    printf("exit\n");
    clearChannelList(&ChannelList);
    printf("ok1\n");
    clearClientList(&ClientList);
    printf("ok2\n");
    if (mq_close(GlobalQueue) < 0)
        perror("close global queue");
    if (mq_unlink(COMMON_QUEUE_NAME) < 0)
        perror("unlink global queue");
    exit(signum);
}

int main(int argc, char **argv)
{
    
    /* return 0; */
    
    struct mq_attr attrs = { .mq_flags = 0, .mq_maxmsg = 10,
                             .mq_msgsize = IRC_MSGSIZE, .mq_curmsgs = 0 };
    GlobalQueue = mq_open(COMMON_QUEUE_NAME, O_RDONLY | O_CREAT | O_EXCL, 0666, &attrs);
    if (GlobalQueue < 0)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "Only one instance of char server can be run at a time.\n");
            return 1;
        }
        perror("global queue open");
        return 2;
    }
    signal(SIGINT, quit);
    while (1)
    {
        Message message;
        if (mq_receive(GlobalQueue, (char *)&message, IRC_MSGSIZE, NULL) < 0)
        {
            perror("receive from global queue");
            quit(3);
        }
        switch (message.type)
        {
            case LOGIN_MSG:
                handleLogin(&message);
                break;
            case DISCONNECT_MSG:
                handleDisconnect(&message);
                break;
            case INFO_MSG:
                handleInfo(&message);
                break;
            case SRV_MSG:
                handleServerCommand(&message);
                break;
            case JOIN_CHANNEL_MSG:
                handleJoinChannel(&message);
                break;
            case LEAVE_CHANNEL_MSG:
                handleLeaveChannel(&message);
                break;
            case SEND_USER_MSG:
                handleSendPrivateMessage(&message);
                break;
            case SEND_CHANNEL_MSG:
                handleSendChannelMessage(&message);
                break;
            case SHOW_USERS:
                handleShowUsers(&message);
                break;
            case SHOW_CHANNELS:
                handleShowChannels(&message);
                break;
            default:
                printf("unknown message type %d\n", message.type);
        }
    }
}

