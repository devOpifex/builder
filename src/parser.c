#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "define.h"

const char *NO_DEFINITION = "<UNDEFINED>";

static Value *create_value(char *value)
{
  Value *new_value = malloc(sizeof(Value));
  if(new_value == NULL) {
    return NULL;
  }
  new_value->name = value;
  new_value->next = NULL;
  return new_value;
}

Value *push_value(Value *head, char *name)
{
  Value *new_value = create_value(name);
  if(new_value == NULL) {
    return NULL;
  }

  if(head == NULL) {
    return new_value;
  }

  Value *current = head;
  while(current->next != NULL) {
    current = current->next;
  }
  current->next = new_value;
  return head;
}

static int is_directive(char *arg) {
  if(strlen(arg) < 2) {
    return 0;
  }

  if(arg[0] == '-' && arg[1] == 'D') {
    return 1;
  }
  return 0;
}

char *get_arg_value(int argc, char *argv[], char *arg)
{
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], arg) == 0)
    {
      if(i + 1 >= argc) {
        return NULL;
      }
      return strdup(argv[i + 1]);
    }
  }
  return NULL;
}

Value *get_arg_values(int argc, char *argv[], char *arg)
{
  Value *values = NULL;
  int got_arg = 0;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], arg) != 0 && !got_arg) continue;

    // we got -arg
    if (strcmp(argv[i], arg) == 0){
      got_arg = 1;
      continue;
    }

    if(!got_arg) {
      continue;
    }

    // we probably encountered another -arg
    if(strncmp(argv[i], "-", 1) == 0) {
      break;
    }

    if(values == NULL) {
      values = create_value(strdup(argv[i]));
      continue;
    }

    values = push_value(values, strdup(argv[i]));
  }

  return values;
}

int has_arg(int argc, char *argv[], char *arg)
{
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], arg) == 0)
    {
      return 1;
    }
  }
  return 0;
}

void get_definitions(Define *arr, int argc, char *argv[])
{
  for (int i = 1; i < argc; i++){
    // this is awful: need refactor
    if (strcmp(argv[i], "-noclean") == 0){
      continue;
    }

    if (strcmp(argv[i], "-input") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-output") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-plugin") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-import") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-deadcode") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-watch") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-sourcemap") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-strip") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-prepend") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-append") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-profile") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-help") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-init") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-bundle") == 0){
      i++;
      continue;
    }

    if(strcmp(argv[i], "-reader") == 0){
      i++;
      continue;
    }

    // these are directives
    if(is_directive(argv[i])) {
      if(i < argc - 1 && !is_directive(argv[i + 1])) {
        char *name = strdup(argv[i] + 2);
        char *value = strdup(argv[i + 1]);
        if(name == NULL || value == NULL) {
          free(name);
          free(value);
          return;
        }
        push(arr, name, value, DEF_VARIABLE, 0);
        i++;
        continue;
      }

      char *name = strdup(argv[i] + 2);
      char *undefined = strdup(NO_DEFINITION);
      if(name == NULL || undefined == NULL) {
        free(name);
        free(undefined);
        return;
      }
      push(arr, name, undefined, DEF_VARIABLE, 0);
    }
  }
}

void free_value(Value *head) {
  Value *current = head;
  while (current != NULL) {
    Value *next = current->next;
    free(current->name);
    free(current);
    current = next;
  }
}
