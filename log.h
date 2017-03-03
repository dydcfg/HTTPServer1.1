#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>
#include "liso.h"
#include <stdio.h>
#include <stdlib.h>


FILE *logOpen(const char *path);
void Log(const char *s);

#endif
