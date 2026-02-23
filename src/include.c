#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Rinternals.h>

#include "include.h"
#include "define.h"
#include "r.h"
#include "log.h"

Registry *create_registry(char *type, char *call)
{
  Registry *registry = malloc(sizeof(Registry));
  registry->type = strdup(type);
  registry->call = strdup(call);
  registry->next = NULL;
  return registry;
}

void push_registry(Registry **registry, char *type, char *call)
{
  Registry *current = *registry;
  while(current != NULL) {
    if(strcmp(current->type, type) == 0) {
      LOG_WARNING("Overriding reader for '%s'", type);
      free(current->call);
      current->call = strdup(call);
      return;
    }
    current = current->next;
  }
  Registry *new = create_registry(type, call);
  new->next = *registry;
  *registry = new;
}

void free_registry(Registry *registry)
{
  Registry *current = registry;
  while(current != NULL) {
    Registry *next = current->next;
    free(current->type);
    free(current->call);
    free(current);
    current = next;
  }
}

Registry *initialize_registry()
{
  Registry *registry = create_registry("txt", "readLines");
  push_registry(&registry, "sql", "readLines");
  push_registry(&registry, "csv", "read.csv");
  push_registry(&registry, "tsv", "read.delim");
  push_registry(&registry, "rds", "readRDS");
  push_registry(&registry, "json", "jsonlite::fromJSON");
  push_registry(&registry, "yml", "yaml::read_yaml");
  push_registry(&registry, "yaml", "yaml::read_yaml");
  push_registry(&registry, "xml", "xml2::read_xml");
  push_registry(&registry, "xlsx", "readxl::read_excel");
  push_registry(&registry, "parquet", "arrow::read_parquet");
  push_registry(&registry, "fst", "fst::read_fst");
  push_registry(&registry, "base64", "base64enc::base64encode");
  return registry;
}

static void free_include(Include *include)
{
  if(include->type != NULL) free(include->type);
  if(include->path != NULL) free(include->path);
  if(include->object != NULL) free(include->object);
}

static int has_include(char *line)
{
  return strstr(line, "#> include:") != NULL;
}

static Include parse_include(char *line)
{
  Include result = {NULL, NULL, NULL};

  const char *start = strstr(line, "#> include:");
  if(start == NULL) return result;

  start += strlen("#> include:");

  char *work = strdup(start);
  if(work == NULL) return result;

  char *saveptr;
  char *token;
  int part = 0;

  token = strtok_r(work, " ", &saveptr);
  while(token != NULL && part < 3) {
    switch (part) {
      case 0:
        result.type = strdup(token); break;
      case 1:
        result.path = strdup(token); break;
      case 2:
        result.object = strdup(token); break;
    }
    part++;
    token = strtok_r(NULL, " ", &saveptr);
  }

  free(work);
  return result;
}

static const char *capture_object(char *func, char *file)
{
  char *call = (char*)malloc(strlen(func) + strlen(file) + 53);
  snprintf(call, strlen(func) + strlen(file) + 53, "paste0(capture.output(dput((%s)('%s'))),collapse='')", func, file);
  const char *result = eval_string(call);
  free(call);

  if(result == NULL) {
    return NULL;
  }

  return strdup(result);
}

static const char *capture_path(Registry **registry, char *type, char *path)
{
  Registry *current = *registry;
  while(current != NULL) {
    if(strcmp(current->type, type) == 0) {
      return capture_object(current->call, path);
    }
    current = current->next;
  }
  return NULL;
}

char *include_replace(char *line, Plugins *plugins, char *file, Registry **registry)
{
  if(!has_include(line)) {
    return line;
  }

  Include inc = parse_include(line);

  char *plugged = plugins_call_include(plugins, inc.type, inc.path, inc.object, file);
  if(plugged != NULL) {
    free_include(&inc);
    return plugged;
  }

  const char *content = capture_path(registry, inc.type, inc.path);

  if(content == NULL) {
    LOG_ERROR("Could not find reader for include:%s", inc.type);
    free_include(&inc);
    return line;
  }

  char *r = (char*)malloc(strlen(content) + strlen(inc.object) + 5);
  snprintf(r, strlen(content) + strlen(inc.object) + 5, "%s <- %s", inc.object, content);

  free_include(&inc);
  free((void*)content);

  return r;
}
