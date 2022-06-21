#ifndef __SHELLEX_H__
#define __SHELLEX_H__

#include "csapp.h"

void make_cd(char *cmdline);
char *token(char *cmdline);
void myHandler(int sig);
void pipeCMD(char* cmdline);
#endif
