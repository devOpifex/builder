#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Rinternals.h>
#include <R_ext/Parse.h>

#include "log.h"

void set_R_home()
{
  if(getenv("R_HOME") != NULL) return;

  FILE *fp = popen("R RHOME", "r");
  if(fp == NULL) {
    printf("%s Failed to run R RHOME and R_HOME environment variables are not set\n", LOG_ERROR);
    return;
  }

  char path[512];
  if(fgets(path, sizeof(path), fp) != NULL) {
    path[strcspn(path, "\n")] = 0;
    setenv("R_HOME", path, 1);
    printf("%s Setting R_HOME environment variable to `%s` (R RHOME)\n", LOG_WARNING, path);
  } else {
    printf("%s Failed to run R RHOME and R_HOME environment variables are not set\n", LOG_ERROR);
  }

  pclose(fp);
}

static char *remove_trailing_newline(char *line)
{
  size_t len = strlen(line);
  if(len > 0 && line[len - 1] == '\n') {
    line[len - 1] = '\0';
  }
  return line;
}

SEXP evaluate(char *expr)
{
  SEXP code_sexp, result;
  ParseStatus status;
  int has_error;

  PROTECT(code_sexp = mkString(expr));

  SEXP parsed = PROTECT(R_ParseVector(code_sexp, -1, &status, R_NilValue));

  if (status != PARSE_OK) {
    UNPROTECT(2);
    printf("%s Parsing expression `%s`\n", LOG_ERROR, remove_trailing_newline(expr));
    return NULL;
  }

  result = PROTECT(R_tryEvalSilent(VECTOR_ELT(parsed, 0), R_GlobalEnv, &has_error));

  if (has_error) {
    UNPROTECT(3);
    printf("%s Evaluating expression `%s`\n", LOG_ERROR, remove_trailing_newline(expr));
    return NULL;
  }

  UNPROTECT(3);

  return result;
}

int evaluate_if(char *expr)
{
  SEXP result = evaluate(expr);

  if(result == NULL) {
    return 0;
  }

  if(TYPEOF(result) != LGLSXP) {
    printf("%s Expression `%s` did not evaluate to a logical value\n", LOG_ERROR, remove_trailing_newline(expr));
    return 0;
  }

  return asLogical(result);
}

const char *eval_string(char *expr)
{
  SEXP result = evaluate(expr);

  if(result == NULL) {
    return 0;
  }

  if(TYPEOF(result) != STRSXP) {
    printf("%s Expression `%s` did not evaluate to a character value\n", LOG_ERROR, remove_trailing_newline(expr));
    return 0;
  }

  return CHAR(STRING_ELT(result, 0));
}

char** extract_macro_args(const char *call_text, int *nargs) {
  const char *paren_start = strchr(call_text, '(');
  if (!paren_start) {
    *nargs = 0;
    return NULL;
  }
  
  const char *paren_end = strrchr(call_text, ')');
  if (!paren_end || paren_end <= paren_start) {
    *nargs = 0;
    return NULL;
  }
  
  int args_len = paren_end - paren_start - 1;
  char *args_text = malloc(args_len + 1);
  strncpy(args_text, paren_start + 1, args_len);
  args_text[args_len] = '\0';
  
  const char *p = args_text;
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '\0') {
    free(args_text);
    *nargs = 0;
    return NULL;
  }
  
  int paren_depth = 0;
  int brace_depth = 0;
  int bracket_depth = 0;
  int in_string = 0;
  char string_char = 0;
  
  int count = 1;
  for (p = args_text; *p; p++) {
    if (in_string) {
      if (*p == '\\') { p++; continue; }
      if (*p == string_char) in_string = 0;
    } else {
      if (*p == '"' || *p == '\'') { in_string = 1; string_char = *p; }
      else if (*p == '(') paren_depth++;
      else if (*p == ')') paren_depth--;
      else if (*p == '{') brace_depth++;
      else if (*p == '}') brace_depth--;
      else if (*p == '[') bracket_depth++;
      else if (*p == ']') bracket_depth--;
      else if (*p == ',' && paren_depth == 0 && brace_depth == 0 && bracket_depth == 0) {
        count++;
      }
    }
  }
  
  char **args = malloc(count * sizeof(char*));
  paren_depth = brace_depth = bracket_depth = in_string = 0;
  
  const char *arg_start = args_text;
  int arg_idx = 0;
  
  for (p = args_text; ; p++) {
    if (in_string) {
      if (*p == '\\') { p++; continue; }
      if (*p == string_char) in_string = 0;
    } else {
      if (*p == '"' || *p == '\'') { in_string = 1; string_char = *p; }
      else if (*p == '(') paren_depth++;
      else if (*p == ')') paren_depth--;
      else if (*p == '{') brace_depth++;
      else if (*p == '}') brace_depth--;
      else if (*p == '[') bracket_depth++;
      else if (*p == ']') bracket_depth--;
    }
    
    if ((*p == ',' && paren_depth == 0 && brace_depth == 0 && bracket_depth == 0 && !in_string) || *p == '\0') {
      while (*arg_start == ' ' || *arg_start == '\t' || *arg_start == '\n') arg_start++;
      const char *arg_end = p - 1;
      while (arg_end > arg_start && (*arg_end == ' ' || *arg_end == '\t' || *arg_end == '\n')) arg_end--;
      
      int len = arg_end - arg_start + 1;
      args[arg_idx] = malloc(len + 1);
      strncpy(args[arg_idx], arg_start, len);
      args[arg_idx][len] = '\0';
      arg_idx++;
      
      if (*p == '\0') break;
      arg_start = p + 1;
    }
  }
  
  free(args_text);
  *nargs = count;
  return args;
}

char* extract_function_body(const char *func_text) 
{
  const char *brace_start = strchr(func_text, '{');
  if (!brace_start) {
    return NULL;
  }
  
  int brace_depth = 0;
  int in_string = 0;
  char string_char = 0;
  const char *brace_end = NULL;
  
  for (const char *p = brace_start; *p; p++) {
    if (in_string) {
      if (*p == '\\') { 
        p++; 
        continue; 
      }
      if (*p == string_char) in_string = 0;
    } else {
      if (*p == '"' || *p == '\'') { 
        in_string = 1; 
        string_char = *p; 
      }
      else if (*p == '{') {
        brace_depth++;
      }
      else if (*p == '}') {
        brace_depth--;
        if (brace_depth == 0) {
          brace_end = p;
          break;
        }
      }
    }
  }
  
  if (!brace_end || brace_depth != 0) {
    return NULL;
  }
  
  const char *content_start = brace_start + 1;
  const char *content_end = brace_end - 1;
  
  while (content_start <= content_end && 
         (*content_start == ' ' || *content_start == '\t' || 
          *content_start == '\n' || *content_start == '\r')) {
    content_start++;
  }
  
  while (content_end >= content_start && 
         (*content_end == ' ' || *content_end == '\t' || 
          *content_end == '\n' || *content_end == '\r')) {
    content_end--;
  }
  
  int len = content_end - content_start + 1;
  if (len <= 0) {
    return strdup("");
  }
  
  char *result = malloc(len + 1);
  strncpy(result, content_start, len);
  result[len] = '\0';
  
  return result;
}
