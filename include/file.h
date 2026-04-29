#ifndef FILE_H
#define FILE_H

#include "define.h"
#include "parser.h"
#include "plugins.h"
#include "include.h"

struct RFile_t {
  char *src;
  char *dst;
  char *content;
  char *ns;
  struct RFile_t *next;
};

typedef struct RFile_t RFile;

typedef int(*Callback)(char *src);

struct Arguments_t {
  int deadcode;
  int s7;
  int sourcemap;
  int dry_run;
  int strip;
  char *input;
  char *output;
  char *append;
  char *prepend;
  char *bundle;
  RFile *files;
  Define **defs;
  Plugins *plugins;
  Registry **registry;
};

typedef struct Arguments_t Arguments;

int exists(char *path);
char *strip_last_slash(char *path);
char *ensure_dir(char *path);
int walk(char *src_dir, Callback func);
int clean(char *src);
char *remove_leading_spaces(char *line);
int collect_files(RFile **files, char *src_dir, char *dst_dir);
int resolve_imports(RFile **files, Value *cli_imports);
int two_pass(Arguments *args);
void free_rfile(RFile *files);

#endif
