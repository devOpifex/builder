#include <stdio.h>
#include <string.h>

#include "log.h"
#include "r.h"

int valid_assert(char *line)
{
  if(strstr(line, "#> assert ") == NULL) return 1;

  line += strlen("#> assert ");
  if(line[0] != '(') {
    printf("%s Failed to parse assert, expected '(' but got '%c'\n", LOG_ERROR, line[0]);
    return 1;
  }

  int i = 0;
  char buffer[512];
  buffer[i] = line[0];
  line++;
  i++;

  int bracket_count = 1;
  while(bracket_count != 0 && line) {
    if(line[0] == '(') {
      bracket_count++;
    }
    if(line[0] == ')') {
      bracket_count--;
    }
    buffer[i] = line[0];
    line++;
    i++;
  }

  int valid = evaluate_if(buffer);
  if(valid){
    return 1;
  }

  printf("%s Assertion failed:%s %s\n", LOG_ERROR, line, buffer);
  return 0;
}
