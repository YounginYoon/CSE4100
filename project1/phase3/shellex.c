#include "csapp.h"
#include "shellex.h"
#include <errno.h>
#define MAXARGS   128
#define TOKEN_MAX 128

/*jobs status*/
#define DFLT 0
#define BG 1
#define FG 2
#define DONE 3
#define STOP 4

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
char** tokenpipe(char* cmdline);
void runpipe(char* tok);

typedef struct _job {
	pid_t pid;
	int id;
	int state;
	char *cmd[MAXLINE];
	struct _job *next;
}job;

job *head;
void initJobs();
void SIGINT_handler(int signal);
void SIGTSTP_handler(int signal);
void SIGCHLD_handler(int signal);
void clear_Job(job* rmnode);
void done_job(pid_t pid);
void add_job(pid_t pid, int state, char* cmd);
void print_job();


int main() 
{
    char cmdline[MAXLINE]; /* Command line */
	head = (job*) malloc(sizeof(job));

	initJobs();


    while (1) {
		Signal(SIGINT, SIGINT_handler);
		Signal(SIGTSTP, SIGTSTP_handler);
		Signal(SIGCHLD, SIGCHLD_handler);
	
		/* Read */
		printf("CSE4100-SP-P#1> ");                   
		fgets(cmdline, MAXLINE, stdin);
		
		if(feof(stdin)) exit(0);
		
		//exit 
		if(!strncmp(cmdline, "exit", 4)) break;	
		
		//cd
		else if(cmdline[0] == 'c' && cmdline[1] == 'd') make_cd(cmdline);

		/* Evaluate */
		else {	
			eval(cmdline);
		}
    } 
}
/* $end shellmain */

  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg, fd[2];             /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    sigset_t set, oldset;
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
	int state;

    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */

	if(strchr(cmdline, '&')) {state = BG;}
	else state = FG;
	
	if(strchr(cmdline, '|')) {
		if(!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> runa
			if((pid = Fork()) == 0) {
				if(state == FG) {
					signal(SIGINT, SIG_DFL);
					signal(SIGTSTP, SIG_DFL);
				}
				else {
					signal(SIGINT, SIG_IGN);
					signal(SIGTSTP, SIG_IGN);
				}
				runpipe(cmdline);
			}
			add_job(pid, state, cmdline);
			
			//parent
			if(state == FG){
				int status;
				if(waitpid(pid, &status, WUNTRACED) >= 0) {
					job* cur = head;
					while(cur!=NULL){
						if(cur->state == FG) clear_Job(cur);
						cur = cur->next;
					}
				}
				else {unix_error("waitfg: waitpid error");}
			
			}
			else{ // bg
				job* cur = head;
				while(cur!=NULL){
					if(cur->pid == pid) {printf("[%d] %d 		     %s", cur->id, cur->pid, cmdline); break;}
					cur = cur->next;
				}
				
			}
		}

	}		
	else{ //파이프 없음
		if(!builtin_command(argv)) {
			if((pid = Fork()) == 0) {
				setpgid(0, 0);
				if(state == FG) {
					signal(SIGINT, SIG_DFL);
					signal(SIGTSTP, SIG_DFL);
				}
				else { //bg
					signal(SIGINT, SIG_IGN);
					signal(SIGTSTP, SIG_IGN);
				}
				if(execvp(argv[0], argv) < 0) {
					printf("%s: Command not found.\n", argv[0]);
					exit(0);
				}
			}
			
			//parents
			add_job(pid, state, cmdline);
			if(state == FG){
				int status;
				if(waitpid(pid, &status, WUNTRACED) >= 0) {
					if(WIFEXITED(status)){
						job* cur = head;
						while(cur!=NULL){
							if(cur->state == FG) clear_Job(cur);
							cur = cur->next;
						}
					}
				}
				else { unix_error("waitfg: waitpid error");}


			}
			else{ // bg
				job* cur = head;
				while(cur!=NULL){
					if(cur->pid == pid) {printf("[%d] %d 			%s", cur->id, cur->pid, cmdline); break;}
					cur = cur->next;
				}
					
			}
		}
	}
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
	if(!strcmp(argv[0], "jobs")) {
		print_job();
		return 1;
	}
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
		char* quotes = strchr(buf, '\"');
		char* squotes = strchr(buf, '\'');

		if(squotes && squotes < delim) {
			argv[argc++] = squotes + 1;
			char* squotes2 = strchr(squotes + 1, '\'');
			*squotes2 = '\0';
			buf = squotes2 + 1;
		}
		else if(quotes && quotes < delim ) {
			argv[argc++] = quotes + 1;
			char* quotes2 = strchr(quotes + 1, '\"');
			*quotes2 = '\0';
			buf = quotes2 + 1;
		}
		else {
			argv[argc++] = buf;
			*delim = '\0';
			buf = delim + 1;
		}
		while (*buf && (*buf == ' ')) /* Ignore spaces */
				buf++;
    }
	
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
char* token(char *cmdline) {
	char* ptr = strtok(cmdline, " ");
	ptr = strtok(NULL, " \n");
	return ptr;
}

void make_cd(char* cmdline) {
	char tmp[30];
	char* tok;
	tok = token(cmdline);
	if(chdir(tok) != 0) perror("cd");
}
	

char** tokenpipe(char* cmdline) {
	char** cmd_tok = (char**)malloc(sizeof(char*)*TOKEN_MAX);
	char* before_tok = (char*)malloc(sizeof(char)*TOKEN_MAX);
	int i, j=0, k=0;
	
	for( i=0; i<strlen(cmdline); i++) {
		if( cmdline[i] == ' ' || cmdline[i] == '\n'|| cmdline[i] == '\0' || cmdline[i] == '\t') {
			if( j!=0 ) {
				cmd_tok[k] = (char*)malloc(sizeof(char)*TOKEN_MAX);
				before_tok[j] = '\0';
				strcpy(cmd_tok[k], before_tok);
				j = 0;
				k++;
				continue;	
			}
		}
		//if(cmdline[i] == ' ' || cmdline[i] == '\n') continue;
		else {

			before_tok[j++] = cmdline[i];
			if(cmdline[i+1] == '|') {
				before_tok[j] = '\0';
				cmd_tok[k] = (char*)malloc(sizeof(char)*TOKEN_MAX);
				strcpy(cmd_tok[k], before_tok);
				j = 0;
				k++;
				continue;			
	
			}
			else if(cmdline[i] == '|' && (cmdline[i+1] != ' ' || cmdline[i+1] != '\t')) {
				before_tok[j] = '\0';
				cmd_tok[k] = (char*)malloc(sizeof(char)*TOKEN_MAX);
				strcpy(cmd_tok[k], before_tok);
				j = 0;
				k++;	
				continue;
			}
		}
	}

	
	free(before_tok);
	cmd_tok[k] = NULL;
	return cmd_tok;

}


void runpipe(char* cmdline) {
	char** tokens = tokenpipe(cmdline);
	int cmdstart[TOKEN_MAX];
	int PIPES[TOKEN_MAX][2];
	int pid, status;
	int pipe_cnt = 0;
	int k = 0;
	
	cmdstart[0] = 0;
	for(int i=0; tokens[i]!=NULL; i++) {
		if(!strcmp(tokens[i], "|")) {
			tokens[i] = NULL;
			pipe_cnt++;
			cmdstart[pipe_cnt] = i+1;
			continue;
		}
		else if(!strcmp(tokens[i], "&")) {
			tokens[i] = NULL;
			break;
		}
	}
	
	//1번째~마지막 직전 명령어
	for(int i=0; i<pipe_cnt;i++) {
		if(pipe(PIPES[i]) < 0) { printf("pipe failed"); exit(0); }
		
		if((pid=Fork()) < 0) { printf("fork error"); exit(1); }
		
		else if(pid == 0) {
                close(STDOUT_FILENO);
                dup2(PIPES[i][1], STDOUT_FILENO);
                close(PIPES[i][1]);
				if( i != 0 ) {
					close(STDIN_FILENO);
					dup2(PIPES[i-1][0], STDIN_FILENO);
					close(PIPES[i-1][0]);
				}

                if (execvp(tokens[cmdstart[i]],&tokens[cmdstart[i]]) < 0) fprintf(stderr, "execvp error\n");
        }
        close(PIPES[i][1]);
		wait(&status);

	}
		
	//마지막
	if((pid = Fork()) < 0) exit(0);
	else if(pid == 0) {
		close(STDIN_FILENO);
		dup2(PIPES[pipe_cnt-1][0], STDIN_FILENO);
		close(PIPES[pipe_cnt-1][0]);
		if(execvp(tokens[cmdstart[pipe_cnt]], &tokens[cmdstart[pipe_cnt]]) < 0) fprintf(stderr, "execvp error");
	}

	wait(&status);
	return;
}


void initJobs(){
	head->pid = 0;
	head->id = 0;
	head->state = DFLT;
	head->next = NULL;
	strcpy(head->cmd, "\0");
}

void clear_Job(job* rmnode){
	int node_cnt = 0;
	job* cur = head;
	job* prev;
	int i=0;
	
	cur->pid = 0;
	cur->id = 0;
	cur->state = DFLT;
	strcpy(cur->cmd, "\0");
	
	while(i < (rmnode->id) ){
		cur = cur->next;
		i++;
	}
	prev = cur;
	prev->next = prev->next->next;
	cur = prev->next;
	cur->id = cur->id-1;

}

void done_job(pid_t pid) {
	job* cur = head;
	while(cur->pid != pid) {
		cur = cur->next;
	}
	cur->state = DONE;
}


void SIGINT_handler(int signal) {
	job* cur = head;

	while(cur!=NULL) {
		if(cur->state == FG) {
			Kill(-(cur->pid), SIGKILL);
			clear_Job(cur);	
		}
		cur = cur->next;
	}
}

void SIGTSTP_handler(int signal) {
	job* cur = head;

	while(cur!=NULL) {
		if(cur->state == FG) {
			Kill(-(cur->pid), SIGTSTP);
			cur->state = STOP;
			printf("[%d]+   STOPPED      	 %s", cur->id, cur->cmd);
		}
		cur = cur->next;
	}
}
void SIGCHLD_handler(int signal) {
	pid_t pid;
	int status;

	while(1){
		if((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0) {
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				done_job(pid);
			}
		}
	}
}

void add_job(pid_t pid, int state, char* cmd) {
	job* cur = head;
	job* new = (job*)malloc(sizeof(job));

	new->pid = pid;
	new->state = state;
	new->next = NULL;
	
	strcpy(new->cmd, cmd);

	while( cur->next != NULL ){
		cur = cur->next;
	}
	new->id = cur->id+1;
	cur->next = new;
	cur = cur->next;
	
}

void print_job() {
	job* cur = head;
	while(cur!=NULL){
		if(cur->state == BG){
			printf("[%d]  Running      	 %s", cur->id, cur->cmd);
		}
		else if(cur->state == FG){
			printf("[%d]  FG  	       %s", cur->id, cur->cmd);
		}
		else if(cur->state == STOP){
			printf("[%d]  STOPPED   	      %s", cur->id, cur->cmd);
		}
		else cur = cur->next;
	}
}
