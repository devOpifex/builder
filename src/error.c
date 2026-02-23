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

  LOG_ERROR("Error: %s", msg);

  return 1;
}

void catch_warning(char* line)
{
  if(strstr(line, "#> warning") == NULL) return;

  while(line[0] == ' ' || line[0] == '\t') line++;

  char *match = strstr(line, "#> warning ");

  char *msg = match + strlen("#> warning ");

  LOG_WARNING("Warning: %s", msg);

  return;
}

void catch_deprecated(char* line)
{
  if(strstr(line, "#> deprecated") == NULL) return;

  while(line[0] == ' ' || line[0] == '\t') line++;

  char *match = strstr(line, "#> deprecated ");

  char *msg = match + strlen("#> deprecated ");

  LOG_WARNING("Deprecated: %s", msg);

  return;
}
