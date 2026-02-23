#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "file.h"
#include "log.h"

void create_package(char *name)
{
  int package = mkdir(name, 0755);
  if(package != 0) {
    LOG_ERROR("Failed to create package: %s", name);
    return;
  }

  // create R directory
  char *r = (char *)malloc(strlen(name) + strlen("/R") + 1);
  strcpy(r, name);
  strcat(r, "/R");
  mkdir(r, 0755);
  LOG_INFO("Created R directory: %s", r);
  free(r);

  // create srcr directory
  char *srcr = (char *)malloc(strlen(name) + strlen("/srcr") + 1);
  strcpy(srcr, name);
  strcat(srcr, "/srcr");
  mkdir(srcr, 0755);
  LOG_INFO("Created srcr directory: %s", srcr);
  free(srcr);

  // .Rbuildignore
  char *build_ignore = (char *)malloc(strlen(name) + strlen("/.Rbuildignore") + 1);
  strcpy(build_ignore, name);
  strcat(build_ignore, "/.Rbuildignore");

  FILE *build_ignore_file = fopen(build_ignore, "w");
  if(build_ignore_file == NULL) {
    LOG_ERROR("Failed to create %s", build_ignore);
    free(build_ignore);
    return;
  }

  fprintf(build_ignore_file, "^srcr/\n");
  fprintf(build_ignore_file, "^builder.ini\n");
  fclose(build_ignore_file);
  LOG_INFO("Creating %s, ignoring: %s", build_ignore, "srcr/ builder.ini");
  free(build_ignore);

  // DESCRIPTION
  char *description = (char *)malloc(strlen(name) + strlen("/DESCRIPTION") + 1);
  strcpy(description, name);
  strcat(description, "/DESCRIPTION");

  FILE *description_file = fopen(description, "w");
  if(description_file == NULL) {
    LOG_ERROR("Failed to create %s", description);
    free(description);
    return;
  }

  fprintf(description_file, "Package: %s\n", name);
  fprintf(description_file, "Title: What the Package Does (One line, Title case)\n");
  fprintf(description_file, "Version: 0.0.0.9000\n");
  fprintf(description_file, "Authors@R:\n\tperson('First', 'Last', email = 'first.last@example.com', role = c('aut', 'cre'))\n");
  fprintf(description_file, "Description: What the package does (one paragraph).\n");
  fprintf(description_file, "License: What license it uses\n");
  fprintf(description_file, "Encoding: UTF-8\n");
  fprintf(description_file, "Roxygen: list(markdown = TRUE)\n");
  fclose(description_file);
  LOG_INFO("Created %s", description);
  free(description);

  //

  char *copy = strdup(name);
  create_config(name);

  LOG_SUCCESS("Created %s package skeleton", copy);
  printf("cd %s && builder --profile dev\n", copy);
  free(copy);
}
