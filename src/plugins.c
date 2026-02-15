#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugins.h"
#include "log.h"
#include "r.h"

static Plugins *create_plugins(char *name, int setup, SEXP obj)
{
  Plugins *plugins = malloc(sizeof(Plugins));
  if(plugins == NULL) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    return NULL;
  }

  plugins->name = strdup(name);
  plugins->setup = setup;
  plugins->obj = obj;
  plugins->next = NULL;

  return plugins;
}

static Plugins *push_plugins(Plugins *head, char *name, int setup, SEXP obj)
{
  if(head == NULL) {
    return create_plugins(name, setup, obj);
  }

  Plugins *new = create_plugins(name, setup, obj);
  if(new == NULL) {
    return NULL;
  }

  Plugins *current = head;
  while(current->next != NULL) {
    current = current->next;
  }

  current->next = new;
  return head;
}

Plugins *plugins_init(Value *plugins, char *input, char *output)
{
  Plugins *head = NULL;

  Value *current = plugins;
  while(current != NULL) {
    char *copy = strdup(current->name);

    char *pkg = strtok(copy, ":");
    char *fn = strtok(NULL, ":");

    SEXP ns = PROTECT(R_FindNamespace(mkString(pkg)));
    SEXP func = PROTECT(findVarInFrame(ns, install(fn)));

    free(copy);

    SEXP fn_call = PROTECT(lang1(func));
    SEXP result = PROTECT(eval(fn_call, R_GlobalEnv));
    UNPROTECT(2);

    defineVar(install(current->name), result, R_GlobalEnv);

    if(result == NULL) {
      printf("%s Failed to initialize plugin: %s\n", LOG_ERROR, current->name);
      head = push_plugins(head, current->name, 0, R_NilValue);
      current = current->next;
      continue;
    }

    SEXP obj = PROTECT(findVar(install(current->name), R_GlobalEnv));

    SEXP setup_func = PROTECT(
      eval(
        lang3(install("$"), obj, install("setup")), R_GlobalEnv
      )
    );

    SEXP setup_call = PROTECT(lang3(setup_func, mkString(input), mkString(output)));
    result = PROTECT(eval(setup_call, R_GlobalEnv));
    UNPROTECT(3);

    if(result == NULL) {
      printf("%s Failed to initialize plugin: %s\n", LOG_ERROR, current->name);
      head = push_plugins(head, current->name, 0, R_NilValue);
      current = current->next;
      continue;
    }

    head = push_plugins(head, current->name, 1, obj);

    printf("%s Initialized plugin: %s\n", LOG_INFO, current->name);

    current = current->next;
  }

  return head;
}

int plugins_failed(Plugins *head)
{
  Plugins *current = head;
  while(current != NULL) {
    if(current->setup == 0) {
      return 1;
    }
    current = current->next;
  }
  return 0;
}

char *plugins_call(Plugins *head, char *fn, char *str, char *file)
{
  char *copy = NULL;

  if(str != NULL) {
    copy = strdup(str);
  }

  int max_iter = 64;
  Plugins *current = head;
  while(current != NULL) {
    max_iter--;

    if(max_iter == 0) {
      printf("%s Plugin %s call to %s() exceeded max iterations (64)\n", LOG_ERROR, current->name, fn);
      head = push_plugins(head, current->name, 0, R_NilValue);
      current = current->next;
      continue;
    }

    // that setup failed
    if(current->setup == 0) {
      current = current->next;
      continue;
    }

    // check that method exists
    char *caller = malloc(strlen(fn) + strlen(current->name) + 4);
    sprintf(caller, "`%s`$%s", current->name, fn);
    SEXP called = PROTECT(evaluate(caller));
    free(caller);

    if(isNull(called)) {
      UNPROTECT(2);
      current = current->next;
      continue;
    }

    SEXP func = PROTECT(
      eval(
        lang3(install("$"), current->obj, install(fn)), R_GlobalEnv
      )
    );

    SEXP call = NULL;
    SEXP result = NULL;
    int errorOccurred = 0;
    if(str != NULL && file != NULL) {
      call = PROTECT(lang3(func, mkString(str), mkString(file)));
      result = R_tryEvalSilent(call, R_GlobalEnv, &errorOccurred);
    } else if(str != NULL && file == NULL) {
      call = PROTECT(lang2(func, mkString(str)));
      result = R_tryEvalSilent(call, R_GlobalEnv, &errorOccurred);
    } else {
      call = PROTECT(lang1(func));
      result = R_tryEvalSilent(call, R_GlobalEnv, &errorOccurred);
    }

    if(result == NULL || errorOccurred) {
      printf("%s Failed to call plugin: %s => %s()\n", LOG_ERROR, current->name, fn);
      UNPROTECT(2);
      head = push_plugins(head, current->name, 0, R_NilValue);
      current = current->next;
      continue;
    }

    UNPROTECT(2);

    if(result == R_NilValue) {
      current = current->next;
      continue;
    }

    if(TYPEOF(result) == STRSXP) {
      copy = realloc(copy, strlen(CHAR(STRING_ELT(result, 0))) + 1);
      strcpy(copy, CHAR(STRING_ELT(result, 0)));
    }

    current = current->next;
  }

  return copy;
}

char *plugins_call_include(Plugins *head, char *type, char *path, char *object, char *file)
{
  char *result_str = NULL;

  Plugins *current = head;
  while(current != NULL) {
    if(current->setup == 0) {
      current = current->next;
      continue;
    }

    // check that method exists
    char *caller = malloc(strlen("include") + strlen(current->name) + 4);
    sprintf(caller, "`%s`$include", current->name);
    SEXP called = PROTECT(evaluate(caller));
    free(caller);

    if(isNull(called)) {
      UNPROTECT(1);
      current = current->next;
      continue;
    }

    SEXP func = PROTECT(
      eval(
        lang3(install("$"), current->obj, install("include")), R_GlobalEnv
      )
    );

    int errorOccurred = 0;
    SEXP call = PROTECT(lang5(func, mkString(type), mkString(path), mkString(object), mkString(file)));
    SEXP result = R_tryEvalSilent(call, R_GlobalEnv, &errorOccurred);

    if(result == NULL || errorOccurred) {
      printf("%s Failed to call plugin: %s => include()\n", LOG_ERROR, current->name);
      UNPROTECT(2);
      current = current->next;
      continue;
    }

    UNPROTECT(2);

    if(result == R_NilValue) {
      current = current->next;
      continue;
    }

    if(TYPEOF(result) == STRSXP) {
      result_str = strdup(CHAR(STRING_ELT(result, 0)));
      return result_str;
    }

    current = current->next;
  }

  return NULL;
}

void free_plugins(Plugins *head) {
  Plugins *current = head;
  while (current != NULL) {
    Plugins *next = current->next;
    free(current->name);
    free(current);
    current = next;
  }
}
