#ifndef __CHAT_COMMON_H_DEFINED_
#define __CHAT_COMMON_H_DEFINED_


#define IRC_MSGSIZE 2048
#define IRC_MAXTEXTSIZE IRC_MSGSIZE - sizeof(int)*2
#define COMMON_QUEUE_NAME "/irc-commmon-queue"
#define CLIENT_QUEUE_NAME_FORMAT "/irc-client-%d"

enum MessageType
{
    LOGIN_MSG = 1,
    TEXT_MSG,
    DISCONNECT_MSG
};

typedef struct LoginMessage
{
    int cid;
    char queueName[NAME_MAX];
} LoginMessage;

typedef struct TextMessage
{
    int cid;
    char text[IRC_MAXTEXTSIZE];
} TextMessage;

typedef struct DisconnectMessage
{
    int cid;
} DisconnectMessage;

typedef struct Message
{
    enum MessageType type;
    union {
        LoginMessage loginmsg;
        TextMessage textmsg;
        DisconnectMessage discmsg;
    } data;
} Message;

#endif
