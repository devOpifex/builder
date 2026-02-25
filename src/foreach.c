#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>

#include "define.h"

int enter_foreach(char *line)
{
  return strstr(line, "#> foreach") != NULL;
}

int exit_foreach(char *line)
{
  return strstr(line, "#> endforeach") != NULL;
}

char *replace_foreach(char *buffer, char *line)
{
  char *delimiter = strchr(buffer, '\n');

  if(delimiter == NULL) {
    LOG_ERROR("Error: single line #> foreach loop");
    return "";
  }

  *delimiter = '\0';
  char *for_body = delimiter + 1;

  char *variable = buffer + strlen("#> foreach ");

  size_t len = strlen(variable);
  int i = 0;
  while(i < len && variable[i] != ' ') {
    i++;
  }

  char *values_str = strdup(variable + i + 1);
  variable[i] = '\0';

  values_str += strlen("in ");

  char *values[64];
  int count = 0;
  char *start = values_str;

  while (*start) {
    while (*start == ' ') start++;
    char *end = strchr(start, ',');
    if (!end) end = start + strlen(start);

    int vlen = end - start;
    values[count] = malloc(vlen + 1);
    memcpy(values[count], start, vlen);
    values[count][vlen] = '\0';

    count++;

    start = *end ? end + 1 : end;
  }

  char pattern[70];
  snprintf(pattern, sizeof(pattern), "..%s..", variable);

  char *result = NULL;

  for(int j = 0; j < count; j++) {
    char *iteration = str_replace(for_body, pattern, values[j]);
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
