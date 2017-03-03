#ifndef _LISO_H_
#define _LISO_H_

#include <time.h>
#include <sys/mman.h>
#include <stdio.h>

#define MAX_PATH 1024

struct Para{
   FILE *log; 
   int port;
   char logPath[MAX_PATH];
   char wwwPath[MAX_PATH];
};

extern struct Para params;

void serveError(int id, char *errorNum, char *shortMsg, char *longMsg);

#endif
