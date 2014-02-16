#ifndef __IRC_SERVER_H_INCLUDED_
#define __IRC_SERVER_H_INCLUDED_

typedef struct Client
{
    int id;
    mqd_t mqdes;
    char *name;
    char *channel;
    struct Client *next;
} Client;

typedef struct ClientEntry
{
    int cid;
    struct ClientEntry *next;
} ClientEntry;

typedef struct Channel
{
    char *name;
    ClientEntry *clients;
    char private;
    ClientEntry *invited;
    struct Channel *next;
} Channel;

#endif
