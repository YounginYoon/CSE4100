/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define N 10
#define NTHREADS 16
#define SBUFSIZE 32

void* thread(void* vargp);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);
void echo(int connfd, Node* node);

Stock setStock(int id, int left_stock, int price, int read_cnt);
Node* addNode(Node* node, Stock stock);
Node* searchNode(Node* current, int search_id);
void initTree(Node* node); 
void visitTree(Node* root, char* buf);


void show(Node* node, int connfd);
void buy(Node* node, int connfd, int id, int n);
void sell(Node* node, int connf, int id, int n);
void updateTxt(Node* node, FILE* fp);

//int byte_cnt = 0;

Node* root = NULL;
sbuf_t sbuf;
static sem_t mutex;

int main(int argc, char **argv) 
{
    int listenfd, connfd, i;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    //static pool pool;
	Stock stock;
	int id, left_stock, price;
	int txt_tmp[N];
	pthread_t tid;
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
		stock = setStock(id, left_stock, price, 0);
		root = addNode(root, stock);
	}
	fclose(fp);

	//
	Sem_init(&mutex, 0, 1);
	sbuf_init(&sbuf, SBUFSIZE);
    listenfd = Open_listenfd(argv[1]);
    
	for(i = 0; i < NTHREADS; i++)
		Pthread_create(&tid, NULL, thread, NULL);

    while (1){
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
			sbuf_insert(&sbuf, connfd);
		
		//check_clients(&pool);
   }
   
	//echo(connfd);
	//Close(connfd);
    initTree(root);
	
    exit(0);
}
/* $end echoserverimain */

Stock setStock(int id, int left_stock, int price, int read_cnt) {
	Stock stock;
	stock.id = id;
	stock.left_stock = left_stock;
	stock.price = price;
	stock.read_cnt = 0;
	Sem_init(&stock.mutex, 0, 1);
	Sem_init(&stock.w, 0, 1);
	
	return stock;
}


Node* addNode(Node* node, Stock stock) {
	if(node == NULL) { // 빈 노드
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

		P(&cur->stock.mutex);
		cur->stock.read_cnt++;
		if(cur->stock.read_cnt == 1) P(&cur->stock.w);
		V(&cur->stock.mutex);

		visitTree(cur->leftChild, buf);
		
		P(&cur->stock.mutex);
		cur->stock.read_cnt--;
		if(cur->stock.read_cnt == 0) V(&cur->stock.w);
		V(&cur->stock.mutex);
		visitTree(cur->rightChild, buf);
	}
}


void show(Node* node, int connfd) {
	char buf[MAXLINE] = "\0";
	//int cnt = 0;
	visitTree(node, buf);
	Rio_writen(connfd, buf, MAXLINE);
}

void buy(Node* node, int connfd, int id, int n) {
	char buf[MAXLINE];
	Node* buyNode = searchNode(node, id);
	int left = buyNode->stock.left_stock;

	P(&node->stock.w);
	if((left - n )>= 0) {
		left -= n;
		sprintf(buf, "[buy] \033[0;32msuccess\033[0m\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
	else {
		sprintf(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
	V(&node->stock.w);
}

void sell(Node* node, int connfd, int id, int n) {
	char buf[MAXLINE];
	Node* sellNode = searchNode(node, id);

	P(&node->stock.w);

	sellNode->stock.left_stock += n;
	sprintf(buf, "[sell] \033[0;32msuccess\033[0m\n");
	Rio_writen(connfd, buf, MAXLINE);

	V(&node->stock.w);
	
}

void updateTxt(Node* node, FILE* fp) {
	if(node != NULL) {
		//fprintf(fp, "%d %d %d\n", node->stock.id, node->stock.left_stock, node->stock.price);
		P(&node->stock.mutex);
		node->stock.read_cnt++;
		if(node->stock.read_cnt == 1) P(&node->stock.w);
		V(&node->stock.mutex);

		fprintf(fp, "%d %d %d\n", node->stock.id, node->stock.left_stock, node->stock.price);
		updateTxt(node->leftChild, fp);
		
		P(&node->stock.mutex);
		node->stock.read_cnt--;
		if(node->stock.read_cnt == 0) V(&node->stock.w);
		V(&node->stock.mutex);
		
		updateTxt(node->rightChild, fp);

	}
}

void* thread(void *vargp) {
	Pthread_detach(pthread_self());

	while(1) {
		int connfd = sbuf_remove(&sbuf);
		echo(connfd, root);
		Close(connfd);

	}
}

void sbuf_init(sbuf_t* sp, int n)
{
	sp->buf = Calloc(n, sizeof(int));
	sp->n = n;                       /* Buffer holds max of n items */
	sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
	Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
	Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
	Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}

void sbuf_deinit(sbuf_t* sp)
{
	Free(sp->buf);
}

void sbuf_insert(sbuf_t* sp, int item)
{
	P(&sp->slots);                          /* Wait for available slot */
	P(&sp->mutex);                          /* Lock the buffer */
	sp->buf[(++sp->rear) % (sp->n)] = item;   /* Insert the item */
	V(&sp->mutex);                          /* Unlock the buffer */
	V(&sp->items);                          /* Announce available item */
}

int sbuf_remove(sbuf_t* sp)
{
	int item;
	P(&sp->items);                          /* Wait for available item */
	P(&sp->mutex);                          /* Lock the buffer */
	item = sp->buf[(++sp->front) % (sp->n)];  /* Remove the item */
	V(&sp->mutex);                          /* Unlock the buffer */
	V(&sp->slots);                          /* Announce available slot */
	return item;
}

void echo(int connfd, Node* node)
{
    int n; 
    char buf[MAXLINE]; 
    char cmd[100];
    int id, num;
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
	printf("disconnected server %d\n", connfd);
	FILE* fp2 = fopen("stock.txt", "wt");
		
	if(fp2!=NULL) {
		//printf("text\n");
		updateTxt(root, fp2);
		//printf("success!\n");
		fclose(fp2);
	}
	else {
		perror("error\n");
	}
}
/* $end echo */
