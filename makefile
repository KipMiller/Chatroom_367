CC = gcc
CFLAGS = -Wall
DFLAGS = -g
SARGS = 36799 46799
PARGS = 127.0.0.1 36799
OARGS = 127.0.0.1 46799

all: prog3_server prog3_participant prog3_observer

prog3_server: prog3_server.c
	$(CC) $(CFLAGS) $(DFLAGS) -o $@ $<

prog3_participant: prog3_participant.c
	$(CC) $(CFLAGS) $(DFLAGS) -o $@ $<
	
prog3_observer: prog3_observer.c
	$(CC) $(CFLAGS) $(DFLAGS) -o $@ $<

.PHONY: runs
runs: ./prog3_server
	./prog3_server $(SARGS)

.PHONY: runp
runp: ./prog3_participant
	./prog3_participant $(PARGS)
	
.PHONY: runo
runo: ./prog3_observer
	./prog3_observer $(OARGS)