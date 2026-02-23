#ifndef DEFINE_H
#define DEFINE_H

#include <stdio.h>

typedef struct Value_t Value;

extern const char *DYNAMIC_DEFINITION;

typedef enum {
  DEF_VARIABLE,
  DEF_FUNCTION
} DefineType;

typedef struct {
    char **name;
    char **value;
    DefineType *type;
    int *global;
    int size;
    int capacity;
} Define;

Define *create_define();
void push(Define *arr, char *name, char *value, DefineType type, int global);
void overwrite(Define **arr, char *name, char *value);
void push_builtins(Define *arr);
void free_array(Define *arr);
void capture_define(Define **defines, char *line, char *ns);
char *define_replace(Define **defines, char *line);
char *get_define_value(Define **defines, char *name);
void *define_macro_init(char **macro);
char* str_replace(const char *orig, const char *find, const char *replace);
void push_macro(Define **defs, char *macro, char *ns);
void increment_counter(Define **arr, char *line);
int enter_macro(char *line);

#endif
