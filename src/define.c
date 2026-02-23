#include "define.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

#include "log.h"
#include "parser.h"
#include "r.h"

static const int MAX_MACRO_DEPTH = 32;

const char* DYNAMIC_DEFINITION = "<DYNAMIC>";

Define* create_define()
{
  Define* arr = malloc(sizeof(Define));
  if (arr == NULL) {
    return NULL;
  }

  arr->capacity = 8;
  arr->size = 0;

  arr->name = malloc(arr->capacity * sizeof(char*));
  if (arr->name == NULL) {
    free(arr);
    return NULL;
  }

  arr->value = malloc(arr->capacity * sizeof(char*));
  if (arr->value == NULL) {
    free(arr->name);
    free(arr);
    return NULL;
  }

  arr->type = (DefineType*)malloc(arr->capacity * sizeof(DefineType));
  if (arr->type == NULL) {
    free(arr->name);
    free(arr->value);
    free(arr);
    return NULL;
  }

  arr->global = (int*)malloc(arr->capacity * sizeof(int));
  if (arr->global == NULL) {
    free(arr->name);
    free(arr->value);
    free(arr->type);
    free(arr);
    return NULL;
  }

  push_builtins(arr);

  return arr;
}

void overwrite(Define** arr, char* name, char* value)
{
  for (int i = 0; i < (*arr)->size; i++) {
    if (strcmp((*arr)->name[i], name) == 0) {
      free((*arr)->value[i]);
      (*arr)->value[i] = strdup(value);
      return;
    }
  }
}

void push_builtins(Define* arr)
{
  time_t now = time(NULL);
  struct tm* local = localtime(&now);
  char date[11];
  char time_str[9];
  strftime(date, sizeof(date), "%Y-%m-%d", local);
  strftime(time_str, sizeof(time_str), "%H:%M:%S", local);

  push(arr, strdup("..FILE.."), strdup(DYNAMIC_DEFINITION), DEF_VARIABLE, 0);
  push(arr, strdup("..LINE.."), strdup(DYNAMIC_DEFINITION), DEF_VARIABLE, 0);
  push(arr, strdup("..COUNTER.."), strdup("-1"), DEF_VARIABLE, 0);
  push(arr, strdup("..DATE.."), strdup(date), DEF_VARIABLE, 0);
  push(arr, strdup("..TIME.."), strdup(time_str), DEF_VARIABLE, 0);

  struct utsname buffer;

  if (uname(&buffer) == -1) {
    return;
  }

  push(arr, strdup("..OS.."), strdup(buffer.sysname), DEF_VARIABLE, 0);
}

void increment_counter(Define** arr, char* line)
{
  if (arr == NULL || line == NULL) {
    return;
  }

  if (strstr(line, "..COUNTER..") == NULL) {
    return;
  }

  char* value = get_define_value(arr, "..COUNTER..");

  if (value == NULL) {
    return;
  }

  int counter = atoi(value);
  counter++;
  int value_astr = asprintf(&value, "%d", counter);
  if (value_astr == -1) {
    LOG_ERROR("Failed to allocate memory");
    return;
  }
  overwrite(arr, "..COUNTER..", value);
  free(value);

  return;
}

void push(Define* arr, char* name, char* value, DefineType type, int global)
{
  if (arr->size == arr->capacity) {
    arr->capacity += 8;

    char** tmp_name = realloc(arr->name, arr->capacity * sizeof(char*));
    if (tmp_name == NULL) {
      arr->capacity -= 8;
      return;
    }
    arr->name = tmp_name;

    char** tmp_value = realloc(arr->value, arr->capacity * sizeof(char*));
    if (tmp_value == NULL) {
      arr->capacity -= 8;
      return;
    }
    arr->value = tmp_value;

    DefineType* tmp_type = realloc(arr->type, arr->capacity * sizeof(DefineType));
    if (tmp_type == NULL) {
      arr->capacity -= 8;
      return;
    }
    arr->type = tmp_type;

    int* tmp_global = realloc(arr->global, arr->capacity * sizeof(int));
    if (tmp_global == NULL) {
      arr->capacity -= 8;
      return;
    }
    arr->global = tmp_global;
  }

  arr->name[arr->size] = name;
  arr->value[arr->size] = value;
  arr->type[arr->size] = type;
  arr->global[arr->size] = global;
  arr->size++;
}

void free_array(Define* arr)
{
  if (arr == NULL) {
    return;
  }

  for (int i = 0; i < arr->size; i++) {
    if (arr->name[i] != NULL) {
      free(arr->name[i]);
    }
    if (arr->value[i] != NULL) {
      free(arr->value[i]);
    }
  }

  free(arr->name);
  free(arr->value);
  free(arr->type);
  free(arr->global);

  free(arr);
}

void* define_macro_init(char** macro)
{
  *macro = strdup("");
  return *macro;
}

void push_macro(Define** defs, char* macro, char* ns)
{
  int local = 0;
  char* p_orig = strdup(macro);
  char* p = strstr(p_orig, "#> macro ");
  if (p != NULL) {
    p += 9;
    strtok(p, "\n");
    local = strcmp(p, "local") == 0;
  }

  free(p_orig);

  char* original_macro = macro;
  macro = strchr(macro, '\n');
  if (macro == NULL) {
    return;
  }
  macro++;

  char* arrow = strstr(macro, "<-");
  if (arrow == NULL) {
    free(original_macro);
    return;
  }

  int temp_nargs;
  char** temp_args = extract_macro_args(macro, &temp_nargs);
  for (int i = 0; i < temp_nargs; i++) {
    if (temp_args[i][0] == '.') {
      LOG_ERROR("Macro argument '%s' cannot start with '.'", temp_args[i]);
      for (int j = 0; j < temp_nargs; j++) free(temp_args[j]);
      free(temp_args);
      free(original_macro);
      return;
    }
  }

  for (int i = 0; i < temp_nargs; i++) free(temp_args[i]);
  free(temp_args);

  size_t len = arrow - macro;
  while (len > 0 && (macro[len - 1] == ' ' || macro[len - 1] == '\t')) {
    len--;
  }
  char* name = malloc(len + 1);
  strncpy(name, macro, len);
  name[len] = '\0';

  if (ns != NULL) {
    char* prefixed = malloc(strlen(ns) + 2 + strlen(name) + 1);
    sprintf(prefixed, "%s::%s", ns, name);
    free(name);
    name = prefixed;
  }

  push((*defs), strdup(name), strdup(macro), DEF_FUNCTION, !local);
  free(name);
  free(original_macro);
}

void capture_define(Define** defines, char* line, char* ns)
{
  if (strncmp(line, "#> define", 9) != 0) {
    return;
  }

  char name[256];
  char value[256];
  if (sscanf(line, "#> define %s %[^\n]", name, value) == 2) {
    if (ns != NULL) {
      char* prefixed = malloc(strlen(ns) + 2 + strlen(name) + 1);
      sprintf(prefixed, "%s::%s", ns, name);
      strcpy(name, prefixed);  // copy back to stack buffer
      free(prefixed);
    }

    if (get_define_value(defines, name) != NULL) {
      LOG_WARNING("%s is already defined", name);
      return;
    }

    push(*defines, strdup(name), strdup(value), DEF_VARIABLE, 0);
  }

  return;
}

char* str_replace(const char* orig, const char* find, const char* replace)
{
  const char* pos = orig;
  int count = 0;
  int find_len = strlen(find);
  if (find_len == 0) {
    return strdup(orig);
  }
  int replace_len = strlen(replace);

  while ((pos = strstr(pos, find)) != NULL) {
    count++;
    pos += find_len;
  }

  if (count == 0) {
    return strdup(orig);
  }

  int orig_len = strlen(orig);
  int new_len = orig_len + count * (replace_len - find_len);

  char* result = malloc(new_len + 1);
  if (!result) {
    return NULL;
  }

  char* current = result;
  const char* src = orig;

  while ((pos = strstr(src, find)) != NULL) {
    int len_before = pos - src;
    memcpy(current, src, len_before);
    current += len_before;

    memcpy(current, replace, replace_len);
    current += replace_len;

    src = pos + find_len;
  }

  strcpy(current, src);

  return result;
}

static char* extract_first_line(const char* str, char* buffer, size_t buffer_size)
{
  if (str == NULL || buffer == NULL || buffer_size == 0) {
    return NULL;
  }

  char* newline_pos = strchr(str, '\n');
  size_t length;

  if (newline_pos != NULL) {
    length = newline_pos - str;
  } else {
    length = strlen(str);
  }

  if (length >= buffer_size) {
    length = buffer_size - 1;
  }

  strncpy(buffer, str, length);
  buffer[length] = '\0';

  return buffer;
}

static char* define_replace_once(Define** defines, char* line)
{
  // we define, nothing to do
  if (strncmp(line, "#> define", 9) == 0) {
    return strdup(line);
  }

  char* current = strdup(line);
  if (current == NULL) {
    return NULL;
  }

  for (int i = 0; i < (*defines)->size; i++) {
    char* name = (*defines)->name[i];
    char* value = (*defines)->value[i];
    char type = (*defines)->type[i];

    if (name == NULL || value == NULL || strcmp(value, NO_DEFINITION) == 0) {
      continue;
    }

    if (type == DEF_VARIABLE) {
      char* replaced = str_replace(current, name, value);
      if (replaced == NULL) {
        continue;
      }

      free(current);
      current = replaced;
      continue;
    }

    if (strstr(line, name) == NULL) {
      continue;
    }

    char* call = malloc(strlen(name) + 2);  // +1 for '(', +1 for '\0'
    strcpy(call, name);
    strcat(call, "(");
    if (strstr(line, call) == NULL) {
      free(call);
      continue;
    }
    free(call);

    // we cleanup the function name
    char fn[1028];
    extract_first_line(value, fn, sizeof(fn));
    int nargs_macro;
    // we extract the macro arguments
    char** args_macro = extract_macro_args(fn, &nargs_macro);

    char* body_macro = extract_function_body(value);

    // we extract the function arguments
    int nargs;
    char** args = extract_macro_args(current, &nargs);

    if (nargs != nargs_macro) {
      LOG_ERROR("Macro %s has %d arguments but function %s has %d arguments", name, nargs_macro, fn,
             nargs);
      // Cleanup before returning
      free(body_macro);
      for (int j = 0; j < nargs_macro; j++) {
        free(args_macro[j]);
      }
      free(args_macro);
      for (int j = 0; j < nargs; j++) {
        free(args[j]);
      }
      free(args);
      return current;
    }

    for (int i = 0; i < nargs; i++) {
      char* old_body;

      // 1. replace ..argname -> "value" (stringify) - MUST be first
      char* dblpat = NULL;
      int dblpat_astr = asprintf(&dblpat, "..%s", args_macro[i]);
      if (dblpat_astr == -1) {
        LOG_ERROR("Failed to allocate memory");
        return current;
      }
      char* quoted = NULL;
      int quoted_astr = asprintf(&quoted, "\"%s\"", args[i]);
      if (quoted_astr == -1) {
        LOG_ERROR("Failed to allocate memory");
        return current;
      }
      old_body = body_macro;
      body_macro = str_replace(body_macro, dblpat, quoted);
      free(old_body);
      free(dblpat);
      free(quoted);

      // 2. replace .argname -> value (second)
      char* dotpat = NULL;
      int dotpat_astr = asprintf(&dotpat, ".%s", args_macro[i]);
      if (dotpat_astr == -1) {
        LOG_ERROR("Failed to allocate memory");
        return current;
      }
      old_body = body_macro;
      body_macro = str_replace(body_macro, dotpat, args[i]);
      free(old_body);
      free(dotpat);
    }

    for (int i = 0; i < nargs_macro; i++) {
      free(args_macro[i]);
    }
    free(args_macro);

    for (int i = 0; i < nargs; i++) {
      free(args[i]);
    }
    free(args);

    free(current);

    int global = (*defines)->global[i];
    if (global) {
      return body_macro;
    }

    int extra = strlen("local({\n})");

    char* wrapped = malloc(strlen(body_macro) + extra + 1);
    strcpy(wrapped, "local({");
    strcat(wrapped, body_macro);
    strcat(wrapped, "\n})");
    free(body_macro);

    return wrapped;
  }

  return current;
}

char* define_replace(Define** defines, char* line)
{
  if (*defines == NULL) {
    return strdup(line);
  }

  char* current = strdup(line);
  if (current == NULL) {
    return NULL;
  }

  int depth = 0;

  while (depth < MAX_MACRO_DEPTH) {
    char* result = define_replace_once(defines, current);
    if (result == NULL) {
      return current;
    }

    // Nothing changed, we're done
    if (strcmp(result, current) == 0) {
      free(result);
      break;
    }

    free(current);
    current = result;
    depth++;
  }

  if (depth == MAX_MACRO_DEPTH) {
    LOG_WARNING("Max macro expansion depth (%d) reached, possible circular definition", MAX_MACRO_DEPTH);
  }

  return current;
}

char* get_define_value(Define** defines, char* name)
{
  if (defines == NULL) {
    return NULL;
  }

  for (int i = 0; i < (*defines)->size; i++) {
    if (strcmp((*defines)->name[i], name) == 0) {
      return (*defines)->value[i];
    }
  }

  return NULL;
}

int enter_macro(char* line)
{
  return strstr(line, "#> macro") != NULL;
}
