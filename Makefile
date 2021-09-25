all: mytop myps mylscpu

CC = gcc

mytop: mytop.c
	$(CC) -o $@ $< -lcurses

myps: myps.c
	$(CC) -o $@ $<

mylscpu: mylscpu.c
	$(CC) -o $@ $<
