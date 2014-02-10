#ifndef __PROTOCOL_H_DEFINED_
#define __PROTOCOL_H_DEFINED_


#define IRC_MSGSIZE 2048
#define IRC_MAXTEXTSIZE IRC_MSGSIZE - sizeof(int)*2 - sizeof(char)*MAX_USR_NAME
#define MAX_USR_NAME 50
#define COMMON_QUEUE_NAME "/irc-server-queue"
#define CLIENT_QUEUE_NAME_FORMAT "/irc-client-%d-queue"

enum MessageType
{
    LOGIN_MSG = 1, //logowanie
    DISCONNECT_MSG, // wylogowanie
    INFO_MSG, // wyswietlenie dodatkowej funkcjonalnosci serwera
    SRV_MSG, // wyslanie zadania wykoanania dodatkowej funkcjonalnosci
    CONFIRM_MSG, // wiadomosc potwierdzajaca od serwera
    JOIN_CHANNEL_MSG, // dolaczenie do kanalu
    LEAVE_CHANNEL_MSG, // wyjscie z kanalu
    SEND_USER_MSG, // wyslanie wiadomosci do uzytkownika
    SEND_CHANNEL_MSG, // wyslanie wiadomosci na kanal
    SHOW_USERS, // wyswietla liste uzytkownikow
    SHOW_CHANNELS // wyswietla liste kanalow
};

//struktura wiadomosci dla logowania
//cid - identyfikator uzytkownika
//name - nazwa, ktora uzytkownik wpisal
//queueName - nazwa kolejki uzytkownika
typedef struct LoginMessage
{
    int cid;
    char name[MAX_USR_NAME];
    char queueName[NAME_MAX];
} LoginMessage;

//struktura wiadomosci potwierdzajacej od serwera
//cid - identyfikator uzytkownika
//confirmed - czy polecenie zostalo wykonane czy nie
//text - wiadomosc dla uzytkownika, jezeli wystapil blad
typedef struct ConfirmationMessage
{
    int cid;
    int confirmed;
    char text[IRC_MAXTEXTSIZE];
} ConfirmationMessage;

//struktura wiadomosci obslugujacej dodatkowa funkcjonalnosc
//cid - identyfikator uzytkownika
//text - zawiera nazwe polecenia (do serwera) i zwracana wiadomosc (do uzytkownika)
typedef struct ServerMessage
{
    int cid;
    char text[IRC_MAXTEXTSIZE];
} ServerMessage;

//struktura wiadomosci obslugujacej kanal
//dolaczanie/wychodzenie
//cid - identyfikator uzytkownika
//channel_name - nazwa kanalu do ktorego chcemy dolaczyc, badz wyjsc
typedef struct ChannelMessage
{
    int cid;
    char channel_name[MAX_USR_NAME];
} ChannelMessage;

//struktura wiadomosci tekstowej
//cid - identyfikator uzytkownika
//name - nazwa uzytkownika/kanalu - w strone serwera - adresat, w strone klienta - odbiorca
//text - wiadomosc
typedef struct TextMessage
{
    int cid;
    char name[MAX_USR_NAME];
    char text[IRC_MAXTEXTSIZE];
} TextMessage;

//struktura wiadomosci wychodzenia z IRC
//cid  - identyfikator uzytkownika
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
        ServerMessage srvmsg;
        ConfirmationMessage confmsg;
        ChannelMessage chmsg;
    } data;
} Message;

#endif
