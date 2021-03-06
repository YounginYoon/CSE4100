/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        if(!strcmp(buf, "exit\n")) break;
        
        Rio_writen(clientfd, buf, MAXLINE);
        Rio_readnb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
        //printf("%s", buf);
    }
    
    Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */
