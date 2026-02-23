#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Rinternals.h>
#include <R_ext/Parse.h>

#include "deadcode.h"
#include "log.h"

static const char *EXCLUDED_NAMES[] = {
  ".onLoad", ".onUnload", ".onAttach", ".onDetach", ".Last.lib",
  ".First.lib", ".packageName", ".conflicts.OK", ".noGenerics",
  NULL
};

Environment* env_create(Environment *parent)
{
  Environment *env = malloc(sizeof(Environment));
  if (env == NULL) return NULL;

  env->bindings = NULL;
  env->parent = parent;
  env->is_global = (parent == NULL) ? 1 : 0;
  return env;
}

void env_free(Environment *env)
{
  if (env == NULL) return;

  Binding *b = env->bindings;
  while (b != NULL) {
    Binding *next = b->next;
    free(b->name);
    free(b->file);
    free(b);
    b = next;
  }
  free(env);
}

void env_free_recursive(Environment *env)
{
  if (env == NULL) return;
  env_free(env);
}

static Binding* find_binding(Environment *env, const char *name)
{
  while (env != NULL) {
    Binding *b = env->bindings;
    while (b != NULL) {
      if (strcmp(b->name, name) == 0) return b;
      b = b->next;
    }
    env = env->parent;
  }
  return NULL;
}

static Binding* find_binding_local(Environment *env, const char *name)
{
  if (env == NULL) return NULL;
  Binding *b = env->bindings;
  while (b != NULL) {
    if (strcmp(b->name, name) == 0) return b;
    b = b->next;
  }
  return NULL;
}

void env_define(Environment *env, const char *name, int is_func, int line, const char *file)
{
  if (env == NULL || name == NULL) return;

  Binding *existing = find_binding_local(env, name);
  if (existing != NULL) {
    existing->is_function = is_func;
    existing->line = line;
    return;
  }

  Binding *b = malloc(sizeof(Binding));
  if (b == NULL) return;

  b->name = strdup(name);
  b->is_function = is_func;
  b->is_used = 0;
  b->line = line;
  b->file = file ? strdup(file) : NULL;
  b->next = env->bindings;
  env->bindings = b;
}

void env_mark_used(Environment *env, const char *name)
{
  Binding *b = find_binding(env, name);
  if (b != NULL) {
    b->is_used = 1;
  }
}

int is_excluded_name(const char *name)
{
  if (name == NULL) return 1;
  if (name[0] == '.') return 1;

  for (int i = 0; EXCLUDED_NAMES[i] != NULL; i++) {
    if (strcmp(name, EXCLUDED_NAMES[i]) == 0) return 1;
  }
  return 0;
}

static int is_assignment_symbol(SEXP sym)
{
  if (TYPEOF(sym) != SYMSXP) return 0;
  const char *name = CHAR(PRINTNAME(sym));
  return (strcmp(name, "<-") == 0 || 
          strcmp(name, "=") == 0 || 
          strcmp(name, "<<-") == 0 ||
          strcmp(name, "assign") == 0);
}

static int is_function_call(SEXP sym)
{
  if (TYPEOF(sym) != SYMSXP) return 0;
  const char *name = CHAR(PRINTNAME(sym));
  return strcmp(name, "function") == 0;
}

static void walk_expr(SEXP expr, Environment *env, int pass, int line, const char *file);

static void walk_function_def(SEXP expr, Environment *env, int pass, int line, const char *file)
{
  Environment *func_env = env_create(env);
  if (func_env == NULL) return;

  SEXP formals = CADR(expr);
  while (formals != R_NilValue && TYPEOF(formals) == LISTSXP) {
    SEXP tag = TAG(formals);
    if (tag != R_NilValue && TYPEOF(tag) == SYMSXP) {
      const char *param_name = CHAR(PRINTNAME(tag));
      env_define(func_env, param_name, 0, line, file);
    }
    formals = CDR(formals);
  }

  SEXP body = CADDR(expr);

  // For local scopes, do both passes in one traversal since func_env
  // doesn't persist between the global passes
  walk_expr(body, func_env, 1, line, file);
  walk_expr(body, func_env, 2, line, file);

  if (pass == 2) {
    Binding *b = func_env->bindings;
    while (b != NULL) {
      if (!b->is_used && !is_excluded_name(b->name)) {
        LOG_WARNING("Unused %s '%s' in function %s:%d",
          b->is_function ? "function" : "variable",
          b->name,
          b->file ? b->file : "",
          b->line
        );
      }
      b = b->next;
    }
  }

  env_free(func_env);
}

static void walk_expr(SEXP expr, Environment *env, int pass, int line, const char *file)
{
  if (expr == R_NilValue) return;

  int expr_type = TYPEOF(expr);

  switch (expr_type) {
    case SYMSXP: {
      if (pass == 2) {
        const char *name = CHAR(PRINTNAME(expr));
        env_mark_used(env, name);
      }
      break;
    }

    case LANGSXP: {
      SEXP head = CAR(expr);

      if (is_assignment_symbol(head)) {
        SEXP lhs = CADR(expr);
        SEXP rhs = CADDR(expr);

        if (TYPEOF(lhs) == SYMSXP) {
          const char *var_name = CHAR(PRINTNAME(lhs));

          int is_func = 0;
          if (TYPEOF(rhs) == LANGSXP) {
            SEXP rhs_head = CAR(rhs);
            if (is_function_call(rhs_head)) {
              is_func = 1;
            }
          }

          if (pass == 1) {
            env_define(env, var_name, is_func, line, file);
          }

          if (is_func) {
            walk_function_def(rhs, env, pass, line, file);
          } else {
            walk_expr(rhs, env, pass, line, file);
          }
        } else {
          walk_expr(lhs, env, pass, line, file);
          walk_expr(rhs, env, pass, line, file);
        }
      } else if (is_function_call(head)) {
        walk_function_def(expr, env, pass, line, file);
      } else {
        walk_expr(head, env, pass, line, file);
        SEXP args = CDR(expr);
        while (args != R_NilValue) {
          walk_expr(CAR(args), env, pass, line, file);
          args = CDR(args);
        }
      }
      break;
    }

    case EXPRSXP: {
      R_xlen_t n = XLENGTH(expr);
      for (R_xlen_t i = 0; i < n; i++) {
        walk_expr(VECTOR_ELT(expr, i), env, pass, line, file);
      }
      break;
    }

    case VECSXP: {
      R_xlen_t n = XLENGTH(expr);
      for (R_xlen_t i = 0; i < n; i++) {
        walk_expr(VECTOR_ELT(expr, i), env, pass, line, file);
      }
      break;
    }

    case LISTSXP:
    case DOTSXP: {
      while (expr != R_NilValue) {
        walk_expr(CAR(expr), env, pass, line, file);
        expr = CDR(expr);
      }
      break;
    }

    default:
      break;
  }
}

static SEXP parse_code(const char *code)
{
  SEXP code_sexp, parsed;
  ParseStatus status;

  PROTECT(code_sexp = mkString(code));
  parsed = PROTECT(R_ParseVector(code_sexp, -1, &status, R_NilValue));

  if (status != PARSE_OK) {
    UNPROTECT(2);
    return R_NilValue;
  }

  UNPROTECT(2);
  return parsed;
}

static char* read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL) return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = malloc(size + 1);
  if (content == NULL) {
    fclose(f);
    return NULL;
  }

  size_t read = fread(content, 1, size, f);
  content[read] = '\0';
  fclose(f);
  return content;
}

int analyse_deadcode(RFile *files)
{
  LOG_INFO("Running dead code analysis...");

  Environment *global_env = env_create(NULL);
  if (global_env == NULL) return 1;

  RFile *current = files;
  while (current != NULL) {
    if (current->dst == NULL) {
      current = current->next;
      continue;
    }
    char *content = read_file(current->dst);
    if (content == NULL) {
      LOG_WARNING("Failed to read %s for dead code analysis", current->dst);
      current = current->next;
      continue;
    }

    SEXP parsed = parse_code(content);
    if (parsed == R_NilValue) {
      LOG_WARNING("Failed to parse %s for dead code analysis", current->dst);
      free(content);
      current = current->next;
      continue;
    }

    PROTECT(parsed);

    R_xlen_t n = XLENGTH(parsed);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_expr(VECTOR_ELT(parsed, i), global_env, 1, (int)i + 1, current->dst);
    }

    UNPROTECT(1);
    free(content);
    current = current->next;
  }

  current = files;
  while (current != NULL) {
    if (current->dst == NULL) {
      current = current->next;
      continue;
    }
    char *content = read_file(current->dst);
    if (content == NULL) {
      current = current->next;
      continue;
    }

    SEXP parsed = parse_code(content);
    if (parsed == R_NilValue) {
      free(content);
      current = current->next;
      continue;
    }

    PROTECT(parsed);

    R_xlen_t n = XLENGTH(parsed);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_expr(VECTOR_ELT(parsed, i), global_env, 2, (int)i + 1, current->dst);
    }

    UNPROTECT(1);
    free(content);
    current = current->next;
  }

  int unused_count = 0;
  Binding *b = global_env->bindings;
  while (b != NULL) {
    if (!b->is_used && !is_excluded_name(b->name)) {
      LOG_WARNING("Unused %s '%s' - %s:%d",
        b->is_function ? "function" : "variable",
        b->name, 
        b->file ? b->file : "<unknown>",
        b->line
      );
      unused_count++;
    }
    b = b->next;
  }

  if (unused_count == 0) {
    LOG_INFO("No unused variables or functions detected");
  } else {
    LOG_WARNING("Found %d unused variable(s)/function(s)", unused_count);
  }

  env_free(global_env);
  return 0;
}
