CC=gcc

CFLAGS = -Wall -Wextra -lssl -lcrypto

all: stnc

stnc: combined.o stnc.o stnc2.o
	$(CC) $(CFLAGS) -o stnc combined.o stnc.o stnc2.o

combined.o: combined.c
	$(CC) $(CFLAGS) -c combined.c

stnc.o: stnc.c
	$(CC) $(CFLAGS) -c stnc.c

stnc2.o: stnc2.c
	$(CC) $(CFLAGS) -c stnc2.c

clean:
	rm -f stnc combined.o stnc.o stnc2.o
