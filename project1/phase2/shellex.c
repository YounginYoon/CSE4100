#include "csapp.h"
#include "shellex.h"
#include <errno.h>
#define MAXARGS   128
#define TOKEN_MAX 128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
char** tokenpipe(char* cmdline);
void runpipe(char* tok);


int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    signal(SIGINT, myHandler);
    signal(SIGQUIT, myHandler);

    while (1) {
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
		int i;
		for(i = 0; i < strlen(cmdline); i++) {
			if(cmdline[i] == '|') {
				runpipe(cmdline);
				break;
			}
		} 
		
		if(i == strlen(cmdline)) eval(cmdline);
	
	}
    } 
}
/* $end shellmain */


void myHandler(int sig) {
	printf("\nhandler execute!\n");
	return;
}
  
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

    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> runa
        if((pid = Fork()) == 0) {
		if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al &
        	    printf("%s: Command not found.\n", argv[0]);
           	 exit(0);
        	}
	}

	if (!bg){ 
	    int status;
	    if(waitpid(pid, &status, 0) < 0) unix_error("waitfg: waitpid error");
	}
	else  //when there is backgrount process!
	    printf("%d %s", pid, cmdline);
	

      }

      
 }

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
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






