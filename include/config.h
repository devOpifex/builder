#ifndef CONFIG_H
#define CONFIG_H

#include "parser.h"
#include "plugins.h"
#include "include.h"

typedef struct {
  char **argv;
  char *input;
  char *output;
  Value *imports;
  Value *plugins_str;
  Value *depends;
  char *prepend;
  char *append;
  Plugins *plugins;
  Registry *registry;
  int argc;
  int deadcode;
  int must_clean;
  int sourcemap;
  int watch;
  int dry_run;
  int diff;
} BuildContext;

int has_config();
BuildContext *get_config(Registry **registry);
void free_config(BuildContext *ctx);
void create_config(char *root);

#endif
