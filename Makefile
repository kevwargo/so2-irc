CC=gcc
LD=gcc

CFLAGS=-c -march=native --std=c99 -pthread -Iinclude -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE -O2

LDFLAGS=-lrt

FILES=server client prompt

SRC=$(addprefix src/, $(addsuffix .c, $(FILES)))
OBJ=$(addprefix obj/, $(addsuffix .o, $(FILES)))
# TGT=coloring

all:
	make server
	make client

server: obj obj/server.o
	${LD} ${LDFLAGS} obj/server.o -o server

client: obj obj/client.o obj/prompt.o
	${LD} ${LDFLAGS} -lreadline -pthread obj/client.o obj/prompt.o -o client

obj:
	mkdir obj

obj/%.o: src/%.c
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean
clean:
	rm -rf obj/ server client

remake: clean all
