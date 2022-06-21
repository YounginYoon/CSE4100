/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */
#include "csapp.h"
void show(Node* node, int connfd);
void buy(Node* node, int connfd, int id, int n);
void sell(Node* node, int connf, int id, int n);

void echo(int connfd, Node* node)
{
    int n; 
    char buf[MAXLINE]; 
    char cmd[100];
    int id, price, num;
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        //printf("%s\n", buf);
        sscanf(buf, "%s %d %d", cmd, &id, &num);
		printf("Server received %d bytes\n", n);
			
		if(!strcmp(cmd, "show")) {
			//Rio_writen(connfd, buf, MAXLINE);
			show(node, connfd);
			//printf("%d\n",strcmp(cmd, "show"));
			//Rio_writen(connfd, buf, MAXLINE);
					
		}
		else if(!strcmp(cmd, "buy")) {
			//Rio_writen(connfd, buf, MAXLINE);
			buy(node, connfd, id, num);
			//Rio_writen(connfd, buf, MAXLINE);
		}
		else if(!strcmp(cmd, "sell")) {
			//Rio_writen(connfd, buf, MAXLINE);
			sell(node, connfd, id, num);
			//Rio_writen(connfd, buf, MAXLINE);
		}
		else {
			Rio_writen(connfd, buf, n);
		}
    }
}
/* $end echo */

