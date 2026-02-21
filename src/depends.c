#include <Rinternals.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "parser.h"
#include "r.h"

typedef struct {
  int major;
  int minor;
  int patch;
} Version;

static Version parse_version(char* version)
{
  Version result = {0, 0, 0};
  sscanf(version, "%d.%d.%d", &result.major, &result.minor, &result.patch);
  return result;
}

int is_installed(char* package)
{
  char* call = (char*)malloc(strlen(package) + 37);
  snprintf(call, strlen(package) + 37, "requireNamespace('%s', quietly = TRUE)", package);
  int result = evaluate_if(call);
  free(call);
  return result;
}

static const char* get_installed_version(char* package)
{
  char* call = (char*)malloc(strlen(package) + strlen("as.character(packageVersion(''))") + 1);
  snprintf(call, strlen(package) + strlen("as.character(packageVersion(''))") + 1, "as.character(packageVersion('%s'))",
           package);
  const char* result = eval_string(call);
  free(call);
  return result;
}

static int compare_version(Version a, Version b)
{
  if (a.major != b.major) return (a.major > b.major) ? 1 : -1;
  if (a.minor != b.minor) return (a.minor > b.minor) ? 1 : -1;
  if (a.patch != b.patch) return (a.patch > b.patch) ? 1 : -1;
  return 0;
}

static int version_satisfies(Version installed, const char* op, Version required)
{
  int cmp = compare_version(installed, required);
  if (strcmp(op, ">=") == 0) return cmp >= 0;
  if (strcmp(op, "<=") == 0) return cmp <= 0;
  if (strcmp(op, "==") == 0) return cmp == 0;
  if (strcmp(op, ">") == 0) return cmp > 0;
  if (strcmp(op, "<") == 0) return cmp < 0;
  return 1;
}

int process_depends(Value* depends)
{
  int result = 0;
  Value* current = depends;
  while (current != NULL) {
    char name[64], operator[3], version[64];
    int n = sscanf(current->name, "%[^(](%2[<>=]%[^)])", name, operator, version);

    int installed = is_installed(name);
    if (!installed) {
      printf("%s Package '%s' is not installed\n", LOG_ERROR, current->name);
      result = 1;
      current = current->next;
      continue;
    }

    if (n < 3) {
      current = current->next;
      continue;
    }

    const char* installed_version = get_installed_version(name);
    Version installed_version_parsed = parse_version((char*)installed_version);
    Version version_parsed = parse_version(version);

    if (!version_satisfies(installed_version_parsed, operator, version_parsed)) {
      printf("%s '%s' is required but version '%s' is installed\n", LOG_ERROR, current->name, installed_version);
      result = 1;
    }

    current = current->next;
  }
  return result;
}
