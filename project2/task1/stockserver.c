/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define N 10

typedef struct {
	int maxfd;
	fd_set read_set;
	fd_set ready_set;
	int nready;
	int maxi;
	int clientfd[FD_SETSIZE];
	rio_t clientrio[FD_SETSIZE];
} pool;

typedef struct _stock {
	int id;
	int left_stock;
	int price;
} Stock;

typedef struct _node {
	Stock stock;
	struct _node *leftChild;
	struct _node *rightChild;
} Node;

void init_pool(int listenfd, pool* p);
void add_client(int connfd, pool* p);
void check_clients(pool* p);
void echo(int connfd);

Stock setStock(int id, int left_stock, int price);
Node* addNode(Node* node, Stock stock);
Node* searchNode(Node* current, int search_id);
void initTree(Node* node); 
void visitTree(Node* root, char* buf);

void show(Node* node, int connfd);
void buy(Node* node, int connfd, int id, int n);
void sell(Node* node, int connf, int id, int n);
void updateTxt(Node* node, FILE* fp);

Node* root = NULL;

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;
	Stock stock;
	int id, left_stock, price;
	int txt_tmp[N];
	FILE* fp = fopen("stock.txt", "wt");

	//stock.txt에 주식 정보 생성
	for(int i=0; i<N; i++) {
		txt_tmp[i] = 0;
	}
	srand((unsigned int) getpid());
	
	int cnt = 0;
    while (cnt < N) {
        id = rand() % N;
        left_stock = rand() % 10;
        price = 100 * (rand() % 1000 + 1);
        if (!txt_tmp[id]) {
            txt_tmp[id] = 1;
            cnt++;
            fprintf(fp, "%d %d %d\n", id + 1, left_stock + 1, price);
        }
    }
	fclose(fp);
	//

	//주식 정보 읽기
	fp = fopen("stock.txt", "r");

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }

	if(fp == NULL) {
		fprintf(stderr, "File does not exist.\n");
		exit(0);
	}

	while(!feof(fp)) {
		fscanf(fp, "%d %d %d", &id, &left_stock, &price);
		stock = setStock(id, left_stock, price);
		root = addNode(root, stock);
	}
	fclose(fp);

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
		
		if(FD_ISSET(listenfd, &pool.ready_set)) {
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(connfd, &pool);
		}
		check_clients(&pool);
   }
   
	//echo(connfd);
	//Close(connfd);
    initTree(root);
	
    exit(0);
}
/* $end echoserverimain */

Stock setStock(int id, int left_stock, int price) {
	Stock stock;
	stock.id = id;
	stock.left_stock = left_stock;
	stock.price = price;
	
	return stock;
}


Node* addNode(Node* node, Stock stock) {
	if(node == NULL) { // 빈 노드
		//node = makeNode(stock, NULL, NULL);
		node = (Node*) malloc(sizeof(Node));
		node->stock = stock;
		node->leftChild = NULL;
		node->rightChild = NULL;
	}

	else if(node->stock.id > stock.id) {
		node->leftChild = addNode(node->leftChild, stock);
	}
	else if(node->stock.id < stock.id) {
		node->rightChild = addNode(node->rightChild, stock);
	}
	return node;
}

Node* searchNode(Node* current, int search_id) { 
	//int cur_id = current->stock.id;
	while(current != NULL) {
		// 작은 id는 왼쪽으로, 큰 id는 오른쪽으로
		int cur_id = current->stock.id;
		if(cur_id == search_id) break;
		else if(cur_id > search_id) current = current -> leftChild;
		else if (cur_id < search_id) current = current -> rightChild;

	}

	return current;

}

void initTree(Node* node) {
	if(node != NULL) {
		initTree(node->leftChild);
		initTree(node->rightChild);
		free(node);
	}
}

void visitTree(Node* root, char *buf) { //전위 순회
	if(root == NULL) return;
	else {
		Node* cur = root;
		char tmp[MAXLINE];
		sprintf(tmp, "%d %d %d\n", cur->stock.id, cur->stock.left_stock, cur->stock.price);
		strcat(buf, tmp);

		visitTree(cur->leftChild, buf);
		visitTree(cur->rightChild, buf);
	}
}


void show(Node* node, int connfd) {
	char buf[MAXLINE] = "\0";
	visitTree(node, buf);

	Rio_writen(connfd, buf, MAXLINE);
}

void buy(Node* node, int connfd, int id, int n) {
	char buf[MAXLINE];
	Node* buyNode = searchNode(node, id);
	int left = buyNode->stock.left_stock;

	if((left - n)>= 0) {
		left -= n;
		sprintf(buf, "[buy] \033[0;32msuccess\033[0m\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
	else {
		sprintf(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
}

void sell(Node* node, int connfd, int id, int n) {
	char buf[MAXLINE];
	Node* sellNode = searchNode(node, id);

	sellNode->stock.left_stock += n;
	sprintf(buf, "[sell] \033[0;32msuccess\033[0m\n");
	Rio_writen(connfd, buf, MAXLINE);
	
}

void init_pool(int listenfd, pool *p) {
	int i;
	p->maxi = -1;
	for(i=0; i<FD_SETSIZE; i++)
		p->clientfd[i] = -1;

	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
	int i;
	p->nready--;
	for(i=0; i<FD_SETSIZE; i++){
		if(p->clientfd[i] < 0) {
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);
		
			FD_SET(connfd, &p->read_set);
			
			if(connfd > p->maxfd) p->maxfd = connfd;
			if(i > p->maxi) p->maxi = i;
			
			break;
		}	
	}
	
	if(i == FD_SETSIZE) app_error("add_client error: Too many clients");
}

void updateTxt(Node* node, FILE* fp) {
	if(node != NULL) {
		fprintf(fp, "%d %d %d\n", node->stock.id, node->stock.left_stock, node->stock.price);

		updateTxt(node->leftChild, fp);
		updateTxt(node->rightChild, fp);

	}
}

void check_clients(pool *p) {
	int i, connfd, n;
	char buf[MAXLINE];
	char cmd[100];
	int id, num;
	rio_t rio;
	
	for(i=0; (i <= p->maxi) && (p->nready > 0); i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];

		if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			p->nready--;
			if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
				//printf("%s", cmd);
				//byte_cnt += n;
				sscanf(buf, "%s %d %d", cmd, &id, &num);
				printf("Server received %d bytes\n", n);
				
				if(!strcmp(cmd, "show")) {
					//Rio_writen(connfd, buf, MAXLINE);
					show(root, connfd);
					//printf("%d\n",strcmp(cmd, "show"));
					//Rio_writen(connfd, buf, MAXLINE);
					
				}
				else if(!strcmp(cmd, "buy")) {
					//Rio_writen(connfd, buf, MAXLINE);
					buy(root, connfd, id, num);
					//Rio_writen(connfd, buf, MAXLINE);
				}
				else if(!strcmp(cmd, "sell")) {
					//Rio_writen(connfd, buf, MAXLINE);
					sell(root, connfd, id, num);
					//Rio_writen(connfd, buf, MAXLINE);
				}
			}
			else {
				Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i] = -1;
				printf("disconnected with server %d\n", connfd);
				FILE *fp2 = fopen("stock.txt", "w");
				if(fp2!=NULL) {
					updateTxt(root, fp2);
					fclose(fp2);
				}
			}
		
		}
	}
	
}

