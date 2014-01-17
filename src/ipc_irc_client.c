#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>


typedef struct msgbuf{
	long id;
	char usr_id[16];
	char rec_id[16];
	int ack;
	time_t time;
	char tekst[128];
} msg;

enum msg_types { MSG_LOGOUT = 1,		//wylogowanie
						MSG_CHNAME,			//zmiana nazwy
						MSG_CHPASS,			//zmiana hasla
						MSG_DISPALL,		//pokazanie wszystkich
						MSG_DISPACTIVE,	//pokazanie aktywnych
						MSG_DISPMEMBERS,	//pokazanie czlonkow grupy
						MSG_DISPGROUPS,	//pokazania grup
						MSG_JOINGROUP,		//przylaczenie sie do grupy
						MSG_LEAVEGROUP,	//wypisanie sie z grupy
						MSG_SNDUSR,			//wiadomosc do innego usera
						MSG_SNDGROUP,		//wiadomosc do grupy
						MSG_SERV,			//serwer -> klient
						MSG_ACK,				//potwierdzenie
						MSG_LOGIN };		//logowanie

enum bool { NO, YES };


char* quit;
char* proc;
char logged = NO;
int* client_th;
int* listen_th;
char nick[16];


void set_buf(msg* buf, long id, char rec_id[16], int ack, char tekst[128]){
	buf->id = id;
	strcpy(buf->usr_id, nick);
	strcpy(buf->rec_id, rec_id);
	buf->ack = ack;
	time(&(buf->time));
	strcpy(buf->tekst, tekst);
}

void clear_buf(msg* buf){
	buf->id = 0;
	strcpy(buf->usr_id, "");
	strcpy(buf->rec_id, "");
	buf->ack = 0;
	time(&(buf->time));
	strcpy(buf->tekst, "");
}

int strnpcpy(char* to, char* from, int start, int n){
	int i;
	int k = NO;
	
	for(i = 0; i < n; i++){
		if((to[i] = from[i + start]) == '\0')
			k = YES;
	}
	
	to[n] = '\0';
	
	return k;
}

void process(void){
	char command[16];
	char param1[16];
	char param2[16];
	char part[128];
	char tekst[2048];
	
	msg buf;
	
	
	scanf("%s", command);
	*proc = 'A';
	
	if(strcmp(command, "logout") == 0){
		
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			//wyloguj	
			set_buf(&buf, MSG_LOGOUT, "", 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			printf(":: Zostales wylogowany.\n");
			logged = NO;
		}
		
	}else if(strcmp(command, "chname") == 0){
		
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			printf(":: Podaj nowa nazwe uzytkownika: ");
			scanf("%s", param1);
			
			//zmiana nazwy
			set_buf(&buf, MSG_CHNAME, param1, 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Nazwa zostala zmieniona.\n");
				strcpy(nick, param1);
			}
			else{
				//blad
				printf(":: Nie udalo sie zmienic nazwy!\n");
			}
		}
					
	}else if(strcmp(command, "chpass") == 0){
		
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			printf(":: Podaj nowe haslo: ");
			scanf("%s", param1);
			
			//zmiana hasla
			set_buf(&buf, MSG_CHPASS, param1, 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Haslo zostalo zmienione.\n");
			}
			else{
				//blad
				printf(":: Nie udalo sie zmienic hasla!\n");
			}
		}
		
	}else if(strcmp(command, "dispall") == 0){
		
		
		
		//wyswietlenie uzytkownikow
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i;
			int licz = 0;

			set_buf(&buf, MSG_DISPALL, "", 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);

			do{
				msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_SERV, 0);
				
				for(i = 0; i < 127; i++){
					tekst[15*licz+i] = buf.tekst[i];
				}
				
				licz++;
			}while(buf.ack != YES);
			
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
			
			if(buf.ack != 0){
				printf(":: Lista wszystkich uzytkownikow:\n");
				printf("%s", tekst);
			}
			else{
				//blad
				printf(":: Nie mozna wyswietlic listy uzytkownikow!\n");
			}
		}
		
	}else if(strcmp(command, "dispactive") == 0){
		
		//wyswietlenie aktywnych
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i;
			int licz = 0;
			
			set_buf(&buf, MSG_DISPACTIVE, "", 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			
			do{
				msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_SERV, 0);
				
				for(i = 0; i < 127; i++){
					tekst[15*licz+i] = buf.tekst[i];
				}
				
				licz++;
			}while(buf.ack != YES);
			
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Lista aktywnych (dostepnych) uzytkownikow:\n");
				printf("%s", tekst);
			}
			else{
				//blad
				printf(":: Nie mozna wyswietlic listy aktywnych uzytkownikow!\n");
			}
		}
		
	}else if(strcmp(command, "dispmembers") == 0){
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i;
			int licz = 0;
			char jest = NO;
			
			printf(":: Podaj nazwe grupy: ");
			scanf("%s", param1);
			
			//wyswietlenie czlonkow grupy
			set_buf(&buf, MSG_DISPMEMBERS, param1, 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			
			do{
				msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), 0, 0);
				
				if(buf.id == MSG_ACK){
					buf.ack = 0;
					jest = YES;
					break;	
				}
				
				for(i = 0; i < 127; i++){
					tekst[15*licz+i] = buf.tekst[i];
				}
				
				licz++;
			}while(buf.ack != YES);
			
			if(!jest)
				msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Lista czlonkow grupy %s:\n", param1);
				printf("%s", tekst);
			}
			else{
				//blad
				printf(":: Nie mozna wyswietlic listy czlonkow grupy %s!\n", param1);
			}
		}
		
	}else if(strcmp(command, "dispgroups") == 0){
		
		//wyswietlenie grup
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i;
			int licz = 0;
			
			set_buf(&buf, MSG_DISPGROUPS, "", 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			
			do{
				msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_SERV, 0);
				
				for(i = 0; i < 127; i++){
					tekst[15*licz+i] = buf.tekst[i];
				}
				
				licz++;
			}while(buf.ack != YES);
			
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Lista grup:\n");
				printf("%s", tekst);
			}
			else{
				//blad
				printf(":: Nie mozna wyswietlic listy grup!\n");
			}
		}
		
	}else if(strcmp(command, "joingroup") == 0){
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			printf(":: Podaj nazwe grupy: ");
			scanf("%s", param1);
			
			//przylaczenie do grupy
			set_buf(&buf, MSG_JOINGROUP, param1, 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Zostales zapisany do grupy %s:\n", param1);
			}
			else{
				//blad
				printf(":: Nie udalo sie zapisac uzytkownika do grupy %s!\n", param1);
			}
		}
		
	}else if(strcmp(command, "leavegroup") == 0){
		
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			printf(":: Podaj nazwe grupy: ");
			scanf("%s", param1);
			
			//wypisanie z grupy
			set_buf(&buf, MSG_LEAVEGROUP, param1, 0, "");
			msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Zostales wypisany z grupy %s:\n", param1);
			}
			else{
				//blad
				printf(":: Nie udalo sie wypisac uzytkownika z grupy %s!\n", param1);
			}
		}
		
	}else if(strcmp(command, "senduser") == 0){
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i, j;
			
			printf(":: Podaj nazwe uzytkownika: ");
			scanf("%s", param1);
			printf(":: Wpisz tresc wiadomosci:\n");
			gets(tekst);
			
			//wyslij
			//printf("\n>>%s<<\n", tekst);
			
			for(i = 0; i < 16; i++){
				if((j = strnpcpy(part, tekst, i*127, 127)) != YES){
					set_buf(&buf, MSG_SNDUSR, param1, NO, part);
					
					//printf("[%d]:%s\n", i, part);
								
					msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
				}else{
					set_buf(&buf, MSG_SNDUSR, param1, YES, part);
								
					//printf("[%d]:%s\n", i, part);
					
					msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
								
					break;
				}
			}
			
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Wiadomosc zostala wyslana\n");
			}
			else{
				//blad
				printf(":: Nie udalo sie wyslac wiadomosci do %s!\n", param1);
			}
		}
		
	}else if(strcmp(command, "sendgroup") == 0){
		
		if(!logged){
			printf(":: Nie jestes zalogowany.\n");
		}else{
			int i, j;
			
			printf(":: Podaj nazwe grupy: ");
			scanf("%s", param1);
			printf(":: Wpisz tresc wiadomosci:\n");
			
			//wczytaj + wyslij
			gets(tekst);
			
			//wyslij
			//printf("\n>>%s<<\n", tekst);
			
			for(i = 0; i < 16; i++){
				if((j = strnpcpy(part, tekst, i*127, 127)) != YES){
					set_buf(&buf, MSG_SNDGROUP, param1, NO, part);
					
					//printf("[%d]:%s\n", i, part);
								
					msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
				}else{
					set_buf(&buf, MSG_SNDGROUP, param1, YES, part);
								
					//printf("[%d]:%s\n", i, part);
					
					msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
								
					break;
				}
			}
			
			msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), MSG_ACK, 0);
				
			if(buf.ack != 0){
				printf(":: Wiadomosc zostala wyslana\n");
			}
			else{
				//blad
				printf(":: Nie udalo sie wyslac wiadomosci do grupy %s!\n", param1);
			}
		}
		
	}else if(strcmp(command, "login") == 0){
		  
		
		if(logged){
			printf(":: Juz jestes zalogowany.\n");
		}else{
			//logowanie	
			printf(":: Podaj nazwe uzytkownika: ");
			scanf("%s", param1);
			printf(":: Podaj haslo: ");
			scanf("%s", param2);
		
			*listen_th = msgget(1024, IPC_PRIVATE | IPC_EXCL);
			//printf("[%d]\n", listen_th);
			
			if(*listen_th != -1){
				printf(":: Polaczono z serwerem. Uwierzytelnianie...\n");
				int tmp_th = msgget(IPC_PRIVATE, IPC_CREAT | 0644);
				
				//printf("\n<tmp_th: %d>\n", tmp_th);
				
				strcpy(nick, param1);
				set_buf(&buf, MSG_LOGIN, param2, tmp_th, "");
				msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
				
				while(msgrcv(tmp_th, &buf, sizeof(msg)-sizeof(long), 0, 0) == -1);
			
				if(buf.ack != 0){
					printf(":: Zostales zalogowany.\n");
					logged = YES;
					*client_th = buf.ack;
					
					//printf("\n<client_th: %d>\n", client_th);
				}
				else{
					//blad
					printf(":: Logowanie nieudane!\n");
				}
				
				msgctl(tmp_th, IPC_RMID, 0);
				
			}else{
				//blad
				printf(":: Nie mozna sie polaczyc z serwerem!\n");
			}
		}
		
	}else if(strcmp(command, "exit") == 0){
		
		//wyloguj	
		set_buf(&buf, MSG_LOGOUT, "", 0, "");
		msgsnd(*listen_th, &buf, sizeof(msg)-sizeof(long), 0);
		logged = NO;
		*quit = YES;
		
	}else if(strcmp(command, "help") == 0 || strcmp(command, "?") == 0){
		
		printf("\n:: Dostepne polecenia:\n");
		printf(":: logout - wylogowanie sie\n");
		printf(":: chname <new_name> - zmiana nazwy uzytkownika\n");
		printf(":: chpass <new_password> - zmiana hasla\n");
		printf(":: dispall - wyswietlenie wszystkich uzytkownikow\n");
		printf(":: dispactive - wyswietlenie aktywnych uzytkownikow\n");
		printf(":: dispmembers <group_name> - wyswietlenie czlonkow grupy\n");
		printf(":: dispgroups - wyswietlenie nazw wszystkich grup\n");
		printf(":: joingroup <group_name> - wylogowanie sie\n");
		printf(":: leavegroup <group_name> - wylogowanie sie\n");
		printf(":: senduser <user_name> <message> - wyslanie wiadomosci do uzytkownika\n");
		printf(":: sendgroup <group_name> <message>- wyslanie wiadomosci do czlonkow grupy\n");
		printf(":: logout - wylogowanie sie\n");
		printf(":: exit - wyjscie z programu\n");
		printf(":: ? lub help - spis polecen\n::\n");
		printf(":: Kolejne argumenty polecen nalezy oddzielac klawiszem 'Enter',\n");
		printf(":: nie jest to jednak konieczne.\n\n");
		
	}else{
		
		printf("\n:: Nieznane polecenie. Aby zobaczyc liste dostepnych\n");
		printf(":: polecen, wpisz: 'help' lub '?'.\n\n");
		
	}
	
	*proc = 'C';
}


int main(int argc, char * argv[]){
	printf("::\n:: IPC SMS Client\n");
	printf(":: Marcin Robaszynski, 80135\n::\n\n");
	printf(":: Wpisz '?' lub 'help', aby wyswietlic spis polecen.\n\n");
			
	int memq = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | 0644);
	int memc = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0644);
	int memt = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0644);
	int memp = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | 0644);
	
	msg buf;
	
	quit = (char*)shmat(memq, 0, 0);
	*quit = NO;
	
	client_th = (int*)shmat(memc, 0, 0);
	*client_th = -1;
	
	listen_th = (int*)shmat(memt, 0, 0);
	*listen_th = -1;
	
	proc = (char*)shmat(memp, 0, 0);
	*proc = 'C';
	
	
	if(fork() > 0){
		while(*quit != YES){
			if(*proc == 'C'){
				process();
				sleep(1);
			}
		}
	}else{
		char tekst[2048];
		int i;
		int parts = 0;
		
		while(*quit != YES){
			
			if(*proc == 'C' && *client_th != -1 && *listen_th != -1){
				*proc = 'B';
				
				if(msgrcv(*client_th, &buf, sizeof(msg)-sizeof(long), 0, IPC_NOWAIT) != -1){
					
					if(buf.id == MSG_SERV){
						for(i = 0; i < 127; i++){
							tekst[127*parts+i] = buf.tekst[i];
						}
						
						if(buf.ack != YES){										
							parts++;
						}else{
							parts = 0;
							
							struct tm* t;
							t = localtime(&buf.time);							
							
							printf(":: %d:%d:%d Wiadomosc od %s:\n%s\n", t->tm_hour, t->tm_min, t->tm_sec, buf.rec_id, tekst);			
						}
					}else
						;
				}
				
				*proc = 'C';
			}
			sleep(1);
		}
	}

	shmctl(memp, IPC_RMID, 0);
	shmctl(memt, IPC_RMID, 0);
	shmctl(memc, IPC_RMID, 0);
	shmctl(memq, IPC_RMID, 0);
	
	return 0;
}