#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

char *add_sourcemap(char *line, int line_number, char *filename)
{
  // there's a special comment in the R code that we don't want to touch
  if(strstr(line, "#") != NULL) {
    return line;
  }

  char *p = line;
  while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if(*p == '\0') {
    return line;
  }

  char *line_str = NULL;
  int line_ast = asprintf(&line_str, "# %s:%d", filename, line_number);
  if(line_ast == -1) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    return NULL;
  }

  size_t len = strlen(line);
  int add_new_line = (len == 0 || line[len - 1] != '\n') ? 1 : 0;

  char *new_line = malloc(strlen(line) + strlen(line_str) + 2 + add_new_line);
  snprintf(new_line, strlen(line) + strlen(line_str) + 2 + add_new_line, "%s %s", line, line_str);
  if(add_new_line) {
    strcat(new_line, "\n");
  }
  free(line);
  free(line_str);
  return new_line;
}
