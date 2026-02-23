#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parser.h"
#include "file.h"
#include "log.h"

#define MAX_LINE 1024

int has_config()
{
  FILE *fp = fopen("builder.ini", "r");
  if (fp == NULL) return 0;
  fclose(fp);
  return 1;
}

static char *get_profile(char *line)
{
  char *bracket = strchr(line, '[');
  if (bracket == NULL) return NULL;

  char *colon = strchr(line, ':');
  if (bracket == NULL) return NULL;

  char *value = colon + 1;
  while (*value == ' ') value++;

  size_t len = strlen(value);
  while(len > 0 && value[len - 1] != ']') {
    len--;
  }

  // remove trailing ]
  len--;

  value[len] = '\0';

  return strdup(value);
}

static char *get_value(char *line)
{
  char *colon = strchr(line, ':');
  if (colon == NULL) return NULL;

  char *value = colon + 1;
  while (*value == ' ') value++;

  size_t len = strlen(value);
  if (len > 0 && value[len - 1] == '\n') {
    value[len - 1] = '\0';
  }

  return strdup(value);
}

static int get_bool(char *line)
{
  char *value = get_value(line);
  if (value == NULL) return 0;

  int result = strcmp(value, "true") == 0;
  free(value);
  return result;
}

static Value *parse_values(char *line)
{
  char *str = get_value(line);
  if (str == NULL) return NULL;

  Value *values = NULL;
  char *token = strtok(str, " ");

  while (token != NULL) {
    values = push_value(values, strdup(token));
    token = strtok(NULL, " ");
  }

  free(str);
  return values;
}

static void parse_registry(char *line, Registry **registry)
{
  char *str = get_value(line);
  if (str == NULL) return;

  // First space-separated token is the type, rest is the function
  char *space = strchr(str, ' ');
  if (space == NULL) {
    free(str);
    return;
  }

  *space = '\0';
  char *type = str;
  char *func = space + 1;

  // Skip leading spaces in function
  while (*func == ' ') func++;

  if (strlen(type) > 0 && strlen(func) > 0) {
    push_registry(registry, type, func);
  }

  free(str);
}

BuildContext *get_config(Registry **registry, char *profile)
{
  FILE *fp = fopen("builder.ini", "r");
  if (fp == NULL) return NULL;
  printf("%s Using config file: builder.ini\n", LOG_INFO);

  BuildContext *ctx = malloc(sizeof(BuildContext));
  if (ctx == NULL) {
    fclose(fp);
    return NULL;
  }

  ctx->argc = 0;
  ctx->argv = NULL;
  ctx->input = NULL;
  ctx->output = NULL;
  ctx->imports = NULL;
  ctx->prepend = NULL;
  ctx->append = NULL;
  ctx->bundle = NULL;
  ctx->plugins_str = NULL;
  ctx->plugins = NULL;
  ctx->registry = NULL;
  ctx->depends = NULL;
  ctx->deadcode = 0;
  ctx->sourcemap = 0;
  ctx->must_clean = 1;
  ctx->watch = 0;

  int profile_found = 0;

  char line[MAX_LINE];
  while (fgets(line, MAX_LINE, fp) != NULL) {
    if (line[0] == '#') continue;
    if (line[0] == '\n') continue;

    char *p = get_profile(line);
    if(p != NULL && !profile_found) {
      // we have profiles in the config
      // but no profile was specified to cli
      if(profile == NULL) {
        printf("%s No profile specified\n", LOG_ERROR);
        return NULL;
      }
      // we found a found a [profile]
      // but we already had a match
      // we're done
      if(profile_found) {
        return ctx;
      }

      profile_found = strcmp(p, profile) == 0;

      if(profile_found) {
        printf("%s Using profile [%s]\n", LOG_INFO, p);
      }
      free(p);
      continue;
    }

    if (strstr(line, "input:") != NULL) {
      ctx->input = get_value(line);
      continue;
    }

    if (strstr(line, "output:") != NULL) {
      ctx->output = get_value(line);
      continue;
    }

    if (strstr(line, "prepend:") != NULL) {
      ctx->prepend = get_value(line);
      continue;
    }

    if (strstr(line, "append:") != NULL) {
      ctx->append = get_value(line);
      continue;
    }

    if (strstr(line, "deadcode:") != NULL) {
      ctx->deadcode = get_bool(line);
      continue;
    }

    if (strstr(line, "sourcemap:") != NULL) {
      ctx->sourcemap = get_bool(line);
      continue;
    }

    if (strstr(line, "clean:") != NULL) {
      ctx->must_clean = get_bool(line);
      continue;
    }

    if (strstr(line, "watch:") != NULL) {
      ctx->watch = get_bool(line);
      continue;
    }

    if (strstr(line, "strip:") != NULL) {
      ctx->strip = get_bool(line);
      continue;
    }

    if (strstr(line, "bundle:") != NULL) {
      ctx->bundle = get_value(line);
      continue;
    }

    if (strstr(line, "depends:") != NULL) {
      Value *depends = parse_values(line);
      if (ctx->depends == NULL) {
        ctx->depends = depends;
      } else {
        Value *current = ctx->depends;
        while (current->next != NULL) current = current->next;
        current->next = depends;
      }
      continue;
    }

    if (strstr(line, "plugin:") != NULL) {
      Value *plugins = parse_values(line);
      if (ctx->plugins_str == NULL) {
        ctx->plugins_str = plugins;
      } else {
        Value *current = ctx->plugins_str;
        while (current->next != NULL) current = current->next;
        current->next = plugins;
      }
      continue;
    }

    if (strstr(line, "import:") != NULL) {
      Value *imports = parse_values(line);
      if (ctx->imports == NULL) {
        ctx->imports = imports;
      } else {
        Value *current = ctx->imports;
        while (current->next != NULL) current = current->next;
        current->next = imports;
      }
      continue;
    }

    if (strstr(line, "reader:") != NULL) {
      parse_registry(line, registry);
      continue;
    }
  }

  fclose(fp);
  return ctx;
}

void free_config(BuildContext *ctx)
{
  if (ctx == NULL) return;
  free(ctx->input);
  free(ctx->output);
  free(ctx->prepend);
  free(ctx->append);
  free(ctx->bundle);
  free_value(ctx->depends);
  free_value(ctx->imports);
  free_value(ctx->plugins_str);
  free(ctx);
}

void create_config(char *root)
{
  if(root == NULL) {
    printf("%s Failed to create config: root is NULL\n", LOG_ERROR);
    return;
  }

  char *dir = ensure_dir(root);
  char *path = (char *)malloc(strlen(dir) + strlen("/builder.ini") + 1);
  strcpy(path, dir);
  strcat(path, "/builder.ini");

  FILE *check = fopen(path, "r");
  if (check != NULL) {
    printf("%s builder.ini already exists\n", LOG_INFO);
    free(path);
    fclose(check);
    return;
  }

  FILE *fp = fopen(path, "w");
  if (fp == NULL) {
    printf("%s builder.ini cannot be created at %s\n", LOG_INFO, path);
    free(path);
    return;
  }

  free(path);

  fprintf(fp, "input: srcr/\n");
  fprintf(fp, "output: R/\n");
  fprintf(fp, "must_clean: true\n\n");
  fprintf(fp, "[profile: dev]\n");
  fprintf(fp, "sourcemap: true\n\n");
  fprintf(fp, "[profile: prod]\n");
  fprintf(fp, "sourcemap: false\n");

  fclose(fp);

  printf("%s builder.ini created\n", LOG_INFO);
}
