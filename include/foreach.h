#ifndef FOREACH_H
#define FOREACH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "define.h"

int enter_foreach(char *line);
int exit_foreach(char *line);
char *replace_foreach(char *buffer, char *line);

#endif
