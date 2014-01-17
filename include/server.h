#ifndef __IRC_SERVER_H_INCLUDED_
#define __IRC_SERVER_H_INCLUDED_

typedef struct Client
{
    int id;
    int mqdes;
    struct Client *next;
} Client;

#endif
