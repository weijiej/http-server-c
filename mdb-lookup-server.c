#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mdb.h"
#include "mylist.h"

#define KeyMax 5

static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv){
    // ignore SIGPIPE so that we don’t terminate when we call
    // send() on a disconnected socket.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        die("signal() failed");

    //check command line inputs
    if(argc != 3){
        fprintf(stderr, "usage: %s <database_file> <server-port>\n", argv[0]);
        exit(1);
    }

    const char *filename = argv[1];
    unsigned short port = atoi(argv[2]);

    //create server socket
    int servsock;
    if((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    //normal socket procedures
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    //bind to local address
    if(bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    //listen for incoming connections
    if(listen(servsock, 5) < 0)
        die("listen failed");

    //client sockets
    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    char line[1000];
    char key[KeyMax + 1];
    char sendbuf[100];
    //run server
    while(1){
        //accept an incoming connection
        clntlen = sizeof(clntaddr);

        //create client socket for reading and writing
        if((clntsock = accept(servsock, (struct sockaddr *) &clntaddr, &clntlen)) < 0){
            die("accept failed");
        }

        //print client info
        fprintf(stderr, "\nconnection started from: %s\n", inet_ntoa(clntaddr.sin_addr));

        //read from client input and search through linked list
        FILE *input = fdopen(clntsock, "r");
        if(input == NULL){
            die("fdopen failed");
        }

        //populate linked list
        FILE *fp = fopen(filename, "rb");
        if(fp == NULL){
            die(filename);
        }

        struct List list;
        initList(&list);

        int loaded = loadmdb(fp, &list);
        if(loaded < 0)
            die("loadmdb");

        fclose(fp);

        while(fgets(line, sizeof(line), input) != NULL){
            //null-terminate the string manually for key
            strncpy(key, line, sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';

            //remove new line, if exist
            size_t last = strlen(key) - 1;
            if(key[last] == '\n')
                key[last] = '\0';

            //traverse list and print matching records
            struct Node *node = list.head;
            int recNo = 1;
            int charsPrinted;
            while(node){
                struct MdbRec *rec = (struct MdbRec *)node->data;
                if(strstr(rec->name, key) || strstr(rec->msg, key)){
                    charsPrinted = snprintf(sendbuf, sizeof(sendbuf), "%4d: {%s} said {%s}\n", recNo, rec->name, rec->msg);
                    if(send(clntsock, sendbuf, charsPrinted, 0) != charsPrinted){
                        die("send() failed");
                        break;
                    }
                }
                node = node->next;
                recNo++;
            }
            //blank line
            if(send(clntsock, "\n", 1, 0) != 1){
                die("send() failed");
            }
        }

        //check if fgets() produced error
        if(ferror(input))
            die("client socket");

        //close client connection
        fclose(input);
        close(clntsock);
        fprintf(stderr, "connection terminated from: %s\n", inet_ntoa(clntaddr.sin_addr));
        freemdb(&list);
    }

    return 0;
}
