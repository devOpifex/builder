#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>

#include "define.h"

int enter_for(char *line)
{
  return strstr(line, "#> for") != NULL;
}

int exit_for(char *line)
{
  return strstr(line, "#> endfor") != NULL;
}

char *replace_for(char *buffer, char *line)
{
  char *delimiter = strchr(buffer, '\n');

  if(delimiter == NULL) {
    printf("%s Error: single line #> for loop\n", LOG_ERROR);
    return "";
  }

  *delimiter = '\0';
  char *for_statement = buffer;
  char *for_body = delimiter + 1;

  char token[64];
  int start;
  int end;

  if(sscanf(for_statement, "#> for %63s in %d:%d", token, &start, &end) != 3) {
    printf("%s Error: invalid #> for statement\n", LOG_ERROR);
    return "";
  }

  char pattern[70];
  snprintf(pattern, sizeof(pattern), "..%s..", token);

  char *result = NULL;

  for(int i = start; i <= end; i++) {
    char replacement[20];
    snprintf(replacement, sizeof(replacement), "%d", i);

    char *iteration = str_replace(for_body, pattern, replacement);
    if(iteration == NULL) {
      free(result);
      return NULL;
    }

    if(result == NULL) {
      result = iteration;
      continue;
    }

    char *new_result = malloc(strlen(result) + strlen(iteration) + 2);
    if(new_result == NULL) {
      free(result);
      free(iteration);
      return NULL;
    }
    strcpy(new_result, result);
    strcat(new_result, iteration);
    free(result);
    free(iteration);
    result = new_result;
  }

  return result;
}
