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
#include <errno.h>
#include "protocol.h"
#include "utils.h"
#include "prompt.h"

mqd_t GlobalQueue;
mqd_t ClientQueue;
char ClientQueueName[NAME_MAX];

static void quit(int signum)
{
    printf("quiting...\n");
    Message msg;
    msg.type = DISCONNECT_MSG;
    msg.data.discmsg.cid = getpid();
    if (mq_send(GlobalQueue, (char *)&msg, sizeof(Message), 0) < 0)
        perror("send disconnect message");
    if (mq_close(ClientQueue) < 0)
        perror("close client queue");
    if (mq_unlink(ClientQueueName) < 0)
        perror("unlink client queue");
    if (mq_close(GlobalQueue) < 0)
        perror("close global queue");
    exit(signum);
}

static void *queueListen(void *arg)
{
    while (1)
    {
        Message message;
        if (mq_receive(ClientQueue, (char *)&message, IRC_MSGSIZE, NULL) < 0)
        {
            safePrintf("error receving from client queue: %s\n", strerror(errno));
            return NULL;
        }
        switch (message.type)
        {
            case CONFIRM_MSG:
                safePrintf("confirm message (%s): %s\n",
                           message.data.confmsg.confirmed ? "success" : "fail",
                           message.data.confmsg.text);
                break;
            case SRV_MSG:
                safePrintf("server message: %s\n", message.data.srvmsg.text);
                break;
            case SHOW_USERS:
                safePrintf("on-line users:\n%s\n", message.data.srvmsg.text);
                break;
            case SHOW_CHANNELS:
                safePrintf("all channels:\n%s\n", message.data.srvmsg.text);
                break;
            case INFO_MSG:
                safePrintf("info: %s\n", message.data.srvmsg.text);
                break;
            case SEND_CHANNEL_MSG:
                safePrintf("channel msg from %s: %s\n", message.data.textmsg.name,
                           message.data.textmsg.text);
                break;
            case SEND_USER_MSG:
                safePrintf("private msg from %s: %s\n", message.data.textmsg.name,
                           message.data.textmsg.text);
        }
    }
}

static void sendChannelMessage(Message *msgbuf, char *msg)
{
    msgbuf->type = SEND_CHANNEL_MSG;
    msgbuf->data.textmsg.cid = getpid();
    char *text = strtok(msg, " 	");
    if (! text)
    {
        safePrintf("no message to send to channel\n");
        return;
        /* return 0; */
    }
    if (strlen(text) >= IRC_MAXTEXTSIZE)
    {
        safePrintf("message is too long, cutting...\n");
        strncpy(msgbuf->data.textmsg.text, text, IRC_MAXTEXTSIZE - 1);
        msgbuf->data.textmsg.text[IRC_MAXTEXTSIZE - 1] = '\0';
    }
    else
        strcpy(msgbuf->data.textmsg.text, text);
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send channel message");
        return;
        /* return 0; */
    }
    safePrintf("channel msg sent successfully\n");
    /* return 1; */
}

static void sendPrivateMessage(Message *msgbuf, char *msg)
{
    msgbuf->type = SEND_USER_MSG;
    msgbuf->data.textmsg.cid = getpid();
    char *username = strtok(msg, " 	");
    if (! username)
    {
        safePrintf("no user name\n");
        return;
        /* return 0; */
    }
    char *text = strtok(NULL, " 	");
    if (! text)
    {
        safePrintf("no text to user\n");
        return;
        /* return 0; */
    }
    safePrintf("args to sendusr: \"%s\" \"%s\"\n", username, text);
    strcpy(msgbuf->data.textmsg.name, username);
    if (strlen(text) >= IRC_MAXTEXTSIZE)
    {
        safePrintf("message is too long, cutting...\n");
        strncpy(msgbuf->data.textmsg.text, text, IRC_MAXTEXTSIZE - 1);
        msgbuf->data.textmsg.text[IRC_MAXTEXTSIZE - 1] = '\0';
    }
    else
        strcpy(msgbuf->data.textmsg.text, text);
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send private message");
        return;
        /* return 0; */
    }
    safePrintf("private msg sent successfully\n");
    /* return 1; */
}

static void joinChannel(Message *msgbuf, char *msg)
{
    msgbuf->type = JOIN_CHANNEL_MSG;
    msgbuf->data.chmsg.cid = getpid();
    char *channel = strtok(msg, " 	");
    if (! channel)
    {
        safePrintf("no channel name\n");
        return;
        /* return 0; */
    }
    if (strlen(channel) >= MAX_USR_NAME)
    {
        safePrintf("channel name is too long, cutting...\n");
        strncpy(msgbuf->data.chmsg.channel_name, channel, MAX_USR_NAME - 1);
        msgbuf->data.chmsg.channel_name[MAX_USR_NAME - 1] = '\0';
    }
    else
        strcpy(msgbuf->data.chmsg.channel_name, channel);
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("join channel");
        return;
        /* return 0; */
    }
    safePrintf("successfully joined channel %s\n", channel);
    /* return 1; */
}

static void leaveChannel(Message *msgbuf)
{
    msgbuf->type = LEAVE_CHANNEL_MSG;
    msgbuf->data.chmsg.cid = getpid();
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
        perror("send leave channel message");
    else
        printf("leave channel message sent successfully\n");
}

static void showUsers(Message *msgbuf)
{
    msgbuf->type = SHOW_USERS;
    msgbuf->data.srvmsg.cid = getpid();
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send show users");
        return;
        /* return 0; */
    }
    safePrintf("successfully sent show users request\n");
    /* return 1; */
}

static void showChannels(Message *msgbuf)
{
    msgbuf->type = SHOW_CHANNELS;
    msgbuf->data.srvmsg.cid = getpid();
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send show channels");
        return;
        /* return 0; */
    }
    safePrintf("successfully sent show channels request\n");
    /* return 1; */
}

static void requestInfo(Message *msgbuf)
{
    msgbuf->type = INFO_MSG;
    msgbuf->data.srvmsg.cid = getpid();
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send info request");
        return;
        /* return 0; */
    }
    safePrintf("successfully sent info request\n");
    /* return 1; */
}

static void serverMessage(Message *msgbuf, char *command)
{
    msgbuf->type = SRV_MSG;
    msgbuf->data.srvmsg.cid = getpid();
    if (strlen(command) >= IRC_MAXTEXTSIZE)
    {
        safePrintf("server command is too long, cutting...\n");
        strncpy(msgbuf->data.srvmsg.text, command, IRC_MAXTEXTSIZE - 1);
        msgbuf->data.srvmsg.text[IRC_MAXTEXTSIZE - 1] = '\0';
    }
    else
        strcpy(msgbuf->data.srvmsg.text, command);
    if (mq_send(GlobalQueue, (char *)msgbuf, sizeof(Message), 0) < 0)
    {
        perror("send server command");
        return;
        /* return 0; */
    }
    safePrintf("successfully sent server command\n");
    /* return 1; */
}

static void showHelp()
{
    printf("quit, exit\n");
    printf("chmsg <msg> - send msg to channel\n");
    printf("privmsg <user> <msg> - send private msg to <user>\n");
    printf("join <channel>\n");
    printf("leave - leave current channel\n");
    printf("users - show users currently logged in\n");
    printf("channels - show all channels\n");
    printf("info - show additional commands which server can handle\n");
    printf("help - shows this help\n");
}

static void parseCommand(char *cmdstr, Message *msgbuf)
{
    if (! cmdstr || // EOF
        strcmp(cmdstr, "quit") == 0 || strcmp(cmdstr, "exit") == 0)
    {
        if (cmdstr)
            free(cmdstr);
        quit(0);
    }
    // here comes actual command parsing
    if (*cmdstr == '\0') // empty string, do nothing
        return;
    if (strncmp(cmdstr, "chmsg ", 6) == 0)
        sendChannelMessage(msgbuf, cmdstr + 6);
    else if (strncmp(cmdstr, "privmsg ", 8) == 0)
        sendPrivateMessage(msgbuf, cmdstr + 8);
    else if (strncmp(cmdstr, "join ", 5) == 0)
        joinChannel(msgbuf, cmdstr + 5);
    else if (strcmp(cmdstr, "leave") == 0)
        leaveChannel(msgbuf);
    else if (strcmp(cmdstr, "users") == 0)
        showUsers(msgbuf);
    else if (strcmp(cmdstr, "channels") == 0)
        showChannels(msgbuf);
    else if (strcmp(cmdstr, "info") == 0)
        requestInfo(msgbuf);
    else if (strcmp(cmdstr, "help") == 0)
        showHelp();
    else
        serverMessage(msgbuf, cmdstr);
    free(cmdstr);
}

int main(int argc, char **argv)
{
    /* GlobalQueue = mq_open("/irc-commmon-queue", O_WRONLY); */
    GlobalQueue = mq_open(COMMON_QUEUE_NAME, O_WRONLY);
    if (GlobalQueue < 0)
    {        
        perror("open global queue");
        quit(0);
    }
    int pid = getpid();
    sprintf(ClientQueueName, CLIENT_QUEUE_NAME_FORMAT, pid);
    /* sprintf(ClientQueueName, "/irc-client-%d", pid); */
    struct mq_attr attrs = { .mq_flags = 0, .mq_maxmsg = 10,
                             .mq_msgsize = IRC_MSGSIZE, .mq_curmsgs = 0 };
    ClientQueue = mq_open(ClientQueueName, O_RDONLY | O_CREAT, 0666, &attrs);
    if (ClientQueue < 0)
    {
        printf("name - %s\n", ClientQueueName);
        perror("open client queue");
        quit(0);
    }
    signal(SIGINT, quit);
    if (argc < 2)
    {
        printf("Usage: %s <login>\n", argv[0]);
        quit(0);
    }
    Message message;
    char *username = argv[1];
    initializePrompt();
    // sending login request
    strcpy(message.data.loginmsg.name, username);
    while (1) // checking if server has accepted the username we gave it
    {
        message.type = LOGIN_MSG;
        message.data.loginmsg.cid = pid;
        strcpy(message.data.loginmsg.queueName, ClientQueueName);
        if (mq_send(GlobalQueue, (char *)&message, sizeof(Message), 0) < 0)
        {
            perror("send login msg");
            quit(0);
        }
        if (mq_receive(ClientQueue, (char *)&message, sizeof(Message), NULL) < 0)
            perror("receiving login confirmation");
        else
            if (message.type == CONFIRM_MSG)
            {
                puts(message.data.confmsg.text);
                if (message.data.confmsg.confirmed)
                    break;
                username = readline("Choose other name: ");
                if (! username)
                    quit(0);
                strcpy(message.data.loginmsg.name, username);
                free(username);
            }
    }
    // login confirmed

    pthread_t queueListenerThread;
    pthread_create(&queueListenerThread, NULL, queueListen, NULL);

    // main command loop
    while (1)
    {
        char *input = readline("-> ");
        parseCommand(input, &message);
    }
}
