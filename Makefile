CC = cc
CFLAGS = -Wall -g --std=c99
LFLAGS = -ledit -lm

all : prompt

prompt : mpc.c prompt.c
	$(CC) $(CFLAGS)  $(LFLAGS) prompt.c mpc.c -o $@
