CC = gcc
CFLAGS = -g -Wall

.PHONY: all
all: http-server mdb-lookup-server

#Targets
http-server: http-server.o

mdb-lookup-server: mdb-lookup-server.o mdb.o

#Object files
http-server.o: http-server.c

mdb-lookup-server.o: mdb-lookup-server.c mdb.h

mdb.o: mdb.c mdb.h

.PHONY: clean
clean:  
rm -f *.o a.out mdb-lookup-server http-server
