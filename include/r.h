#ifndef R_H
#define R_H

#include <Rinternals.h>
#include <R_ext/Parse.h>
#include <Rembedded.h>

const char *eval_string(char *expr);
SEXP evaluate(char *expr);
int evaluate_if(char *expr);
void set_R_home();
void set_R_share();
char** extract_macro_args(const char *args_text, int *nargs);
char* extract_function_body(const char *func_text);

#endif
