#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"

int catch_error(char* line)
{
  if(strstr(line, "#> error") == NULL) return 0;

  while(line[0] == ' ' || line[0] == '\t') line++;

  char *match = strstr(line, "#> error ");

  char *msg = match + strlen("#> error ");

  printf("%s Error: %s\n", LOG_ERROR, msg);

  return 1;
}

void catch_warning(char* line)
{
  if(strstr(line, "#> warning") == NULL) return;

  while(line[0] == ' ' || line[0] == '\t') line++;

  char *match = strstr(line, "#> warning ");

  char *msg = match + strlen("#> warning ");

  printf("%s Warning: %s\n", LOG_WARNING, msg);

  return;
}

void catch_deprecated(char* line)
{
  if(strstr(line, "#> deprecated") == NULL) return;

  while(line[0] == ' ' || line[0] == '\t') line++;

  char *match = strstr(line, "#> deprecated ");

  char *msg = match + strlen("#> deprecated ");

  printf("%s Deprecated: %s\n", LOG_WARNING, msg);

  return;
}
