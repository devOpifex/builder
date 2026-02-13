#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <regex.h>

#include "deconstruct.h"
#include "preflight.h"
#include "sourcemap.h"
#include "plugins.h"
#include "define.h"
#include "include.h"
#include "fstring.h"
#include "const.h"
#include "error.h"
#include "file.h"
#include "test.h"
#include "log.h"
#include "for.h"
#include "r.h"
#include "deadcode.h"

int exists(char *path)
{
  FILE *file = fopen(path, "r");

  if(file == NULL) {
    return 0;
  }

  fclose(file);
  return 1;
}

char *strip_last_slash(char *path)
{
  size_t len = strlen(path);

  if(len > 0 && path[len - 1] == '/') {
    char *dir = malloc(len);
    if(dir == NULL) {
      return NULL;
    }
    strncpy(dir, path, len - 1);
    dir[len - 1] = '\0';
    free(path);
    return dir;
  }

  return path;
}

char *ensure_dir(char *path)
{
  size_t len = strlen(path);

  if(len > 0 && path[len - 1] != '/') {
    char *dir = malloc(len + 2);  // len + '/' + null terminator
    if(dir == NULL) {
      return NULL;
    }
    strcpy(dir, path);
    strcat(dir, "/");
    free(path);  // Free the input since we're returning a new allocation
    return dir;
  }

  // Already ends with '/', return the same pointer
  return path;
}

int clean(char *src)
{
  // Remove the output file
  int result = remove(src);

  // Also remove the corresponding test file
  // Extract filename from src (e.g., "R/sub-main.R" -> "sub-main.R")
  const char *filename = strrchr(src, '/');
  if(filename == NULL) {
    filename = src;
  } else {
    filename++;  // Skip the '/'
  }

  // Construct test filename: tests/testthat/test-builder-<filename>
  size_t test_path_len = strlen("tests/testthat/test-builder-") + strlen(filename) + 1;
  char *test_path = malloc(test_path_len);
  if(test_path != NULL) {
    snprintf(test_path, test_path_len, "tests/testthat/test-builder-%s", filename);

    // Remove test file if it exists (don't report error if it doesn't)
    remove(test_path);

    free(test_path);
  }

  return result;
}

char *remove_leading_spaces(char *line)
{
  while(*line && (*line == ' ' || *line == '\t')) {
    line++;
  }
  return line;
}

static char *remove_keyword(char *line)
{
  char *t = strchr(line, ' ');
  if(t == NULL) {
    return strdup(line);
  }
  t++;

  size_t len = strlen(t);
  if(len > 0 && t[len - 1] == '\n') {
    t[len - 1] = '\0';
  }

  return strdup(t);
}

static int should_write_line(int state, int *branch_taken, char line[1024], Define **defs)
{
  char *trimmed = remove_leading_spaces(line);

  if(strncmp(trimmed, "#> ", 3) != 0) {
    return state;
  }

  if(strncmp(trimmed, "#> test", 7) == 0) {
    return 0;
  }

  if(strncmp(trimmed, "#> endtest", 10) == 0) {
    return 1;
  }

  if(strncmp(trimmed, "#> preflight", 12) == 0) {
    return 0;
  }

  if(strncmp(trimmed, "#> endflight", 12) == 0) {
    return 1;
  }

  if(strncmp(trimmed, "#> endpreflight", 15) == 0) {
    return 1;
  }

  if(strncmp(trimmed, "#> ifdef", 8) == 0) {
    char *keyword = remove_keyword(trimmed + 3);
    int result = get_define_value(defs, keyword) != NULL;
    free(keyword);
    *branch_taken = result;
    return result;
  }

  if(strncmp(trimmed, "#> ifndef", 9) == 0) {
    char *keyword = remove_keyword(trimmed + 3);
    int result = get_define_value(defs, keyword) == NULL;
    free(keyword);
    *branch_taken = result;
    return result;
  }

  if(strncmp(trimmed, "#> elif", 7) == 0) {
    if(*branch_taken) {
      return 0;
    }
    char *keyword = remove_keyword(trimmed + 3);
    int result = get_define_value(defs, keyword) != NULL;
    free(keyword);
    if(result) *branch_taken = 1;
    return result;
  }

  if(strncmp(trimmed, "#> else", 7) == 0) {
    if(*branch_taken) return 0;
    return 1;
  }

  if(strncmp(trimmed, "#> endif", 8) == 0) {
    *branch_taken = 0;
    return 1;
  }

  if(strncmp(trimmed, "#> if", 5) == 0) {
    int result = evaluate_if(trimmed + 6);
    *branch_taken = result;
    return result;
  }

  return state;
}

static char *replace_slash(char *path)
{
  char *new_path = strdup(path);
  for(int i = 0; i < strlen(new_path); i++) {
    if(new_path[i] == '/') {
      new_path[i] = '-';
    }
  }
  
  return new_path;
}

static char *make_dest_path(char *src, char *dst)
{
  size_t l = strlen(src) + strlen(dst) + 1;
  char *path = malloc(l);

  if(path == NULL) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    return NULL;
  }

  char *replaced = replace_slash(src);
  snprintf(path, l, "%s%s", dst, replaced);
  free(replaced);

  char *slash = strrchr(path, '/');
  char *start = (slash != NULL) ? slash + 1 : path;
  char *hyphen = strchr(start, '-');

  if(hyphen == NULL) {
    return path;
  }

  memmove(start, hyphen + 1, strlen(hyphen + 1) + 1);

  return path;
}

static char *append_buffer(char *buffer, char *line)
{
  if(buffer == NULL) {
    return strdup(line);
  }

  size_t buf_len = strlen(buffer);
  int add_new_line = (buf_len == 0 || buffer[buf_len - 1] != '\n') ? 1 : 0;

  char *new_buffer = malloc(strlen(buffer) + strlen(line) + 1 + add_new_line);
  if(new_buffer == NULL) {
    // we're fucked down here
    // heap failed to allocate memory: run for the hills
    return NULL;
  }

  strcpy(new_buffer, buffer);
  if(add_new_line) {
    strcat(new_buffer, "\n");
  }
  strcat(new_buffer, line);
  free(buffer);

  return new_buffer;
}

int walk(char *src_dir, Callback func)
{
  DIR *source;
  struct dirent *entry;
  char path[PATH_MAX];
  
  source = opendir(src_dir);
  if (source == NULL) {
    printf("%s Failed to open source directory: %s\n", LOG_ERROR, src_dir);
    return 1;
  }

  while((entry = readdir(source)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    snprintf(path, PATH_MAX, "%s/%s", src_dir, entry->d_name);

    // it's a directory, recurse
    if (entry->d_type == DT_DIR) {
      walk(path, func);
    } else {
      char *ext = strrchr(path, '.');
      if(strcmp(ext, ".R") != 0 && strcmp(ext, ".r") != 0) continue;
      func(path);
    }
  }
  
  closedir(source);
  return 0;
}

static RFile *create_rfile(char *src, char *dst, char *content, char *ns)
{
  RFile *file = malloc(sizeof(RFile));
  if(file == NULL) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    return NULL;
  }

  file->src = strdup(src);
  file->dst = dst ? strdup(dst) : NULL;
  file->content = content ? strdup(content) : NULL;
  file->ns = ns ? strdup(ns) : NULL;
  file->next = NULL;

  return file;
}

static void push_rfile(RFile **files, char *src, char *dst, char *content, char *ns)
{
  RFile *file = create_rfile(src, dst, content, ns);
  if(file == NULL) {
    return;
  }

  file->next = *files;
  *files = file;
}

void free_rfile(RFile *files)
{
  RFile *current = files;
  while(current != NULL) {
    RFile *next = current->next;
    free(current->src);
    free(current->dst);
    free(current->content);
    free(current->ns);
    free(current);
    current = next;
  }
}

static char *get_path_from_package(char *input)
{
  char *delimiter = "::";
  char *split_point = strstr(input, delimiter);
  size_t name_len = split_point - input;

  char *name = malloc(name_len + 1);
  strncpy(name, input, name_len);
  name[name_len] = '\0';

  char *path = strdup(split_point + strlen(delimiter));

  SEXP system_file = PROTECT(install("system.file"));
  SEXP filename = PROTECT(mkString(path));
  SEXP pkg_name = PROTECT(mkString(name));

  free(name);
  free(path);

  SEXP call = PROTECT(lang3(system_file, filename, pkg_name));
  SET_TAG(CDDR(call), install("package"));

  SEXP result = PROTECT(eval(call, R_GlobalEnv));
  UNPROTECT(5);

  const char *filepath = CHAR(asChar(result));
  return strdup(filepath);
}

static char *get_import_path(char *path)
{
  if(strstr(path, "::") != NULL) {
    return get_path_from_package(path);
  }
  return strdup(path);
}

static char *get_import_namespace(char *path)
{
  char *delim = strstr(path, "::");
  if(delim == NULL) return NULL;

  size_t len = delim - path;
  char *ns = malloc(len + 1);
  strncpy(ns, path, len);
  ns[len] = '\0';
  return ns;
}

static int is_path_seen(char **seen, int seen_count, char *path)
{
  for(int i = 0; i < seen_count; i++) {
    if(strcmp(seen[i], path) == 0) return 1;
  }
  return 0;
}

static void add_seen_path(char ***seen, int *seen_count, char *path)
{
  *seen = realloc(*seen, (*seen_count + 1) * sizeof(char*));
  (*seen)[*seen_count] = strdup(path);
  (*seen_count)++;
}

static void free_seen(char **seen, int seen_count)
{
  for(int i = 0; i < seen_count; i++) {
    free(seen[i]);
  }
  free(seen);
}

static Value *scan_for_imports(char *content)
{
  Value *imports = NULL;
  char *pos = content;

  while(*pos) {
    char *line_end = strchr(pos, '\n');
    if(!line_end) break;

    if(strncmp(pos, "#> import ", 10) == 0) {
      char *import_start = pos + 10;
      size_t len = line_end - import_start;
      char *import_path = malloc(len + 1);
      strncpy(import_path, import_start, len);
      import_path[len] = '\0';

      while(len > 0 && (import_path[len-1] == '\r' || import_path[len-1] == ' ')) {
        import_path[--len] = '\0';
      }

      Value *v = malloc(sizeof(Value));
      v->name = import_path;
      v->next = imports;
      imports = v;
    }
    pos = line_end + 1;
  }

  return imports;
}

static int prepend_import(RFile **files, char *import_spec, char ***seen, int *seen_count)
{
  char *resolved_path = get_import_path(import_spec);
  if(resolved_path == NULL || strlen(resolved_path) == 0) {
    printf("%s Failed to resolve import: %s\n", LOG_ERROR, import_spec);
    free(resolved_path);
    return 0;
  }

  if(is_path_seen(*seen, *seen_count, resolved_path)) {
    free(resolved_path);
    return 1;
  }

  add_seen_path(seen, seen_count, resolved_path);

  FILE *file = fopen(resolved_path, "r");
  if(file == NULL) {
    printf("%s Failed to open import: %s\n", LOG_ERROR, resolved_path);
    free(resolved_path);
    return 0;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = malloc(size + 1);
  fread(content, 1, size, file);
  content[size] = '\0';
  fclose(file);

  char *ns = get_import_namespace(import_spec);

  Value *nested = scan_for_imports(content);
  Value *current = nested;
  while(current != NULL) {
    if(!prepend_import(files, current->name, seen, seen_count)) {
      free(content);
      free(resolved_path);
      free(ns);
      free_value(nested);
      return 0;
    }
    current = current->next;
  }
  free_value(nested);

  push_rfile(files, resolved_path, NULL, content, ns);
  printf("%s Import: %s\n", LOG_INFO, resolved_path);

  free(content);
  free(resolved_path);
  free(ns);
  return 1;
}

int resolve_imports(RFile **files, Value *cli_imports)
{
  char **seen = NULL;
  int seen_count = 0;

  RFile *current = *files;
  while(current != NULL) {
    add_seen_path(&seen, &seen_count, current->src);
    current = current->next;
  }

  Value *cli = cli_imports;
  while(cli != NULL) {
    if(!prepend_import(files, cli->name, &seen, &seen_count)) {
      free_seen(seen, seen_count);
      return 0;
    }
    cli = cli->next;
  }

  current = *files;
  while(current != NULL) {
    if(current->dst == NULL) {
      current = current->next;
      continue;
    }
    Value *imports = scan_for_imports(current->content);
    Value *imp = imports;
    while(imp != NULL) {
      if(!prepend_import(files, imp->name, &seen, &seen_count)) {
        free_value(imports);
        free_seen(seen, seen_count);
        return 0;
      }
      imp = imp->next;
    }
    free_value(imports);
    current = current->next;
  }

  free_seen(seen, seen_count);
  return 1;
}

int collect_files(RFile **files, char *src_dir, char *dst_dir)
{
  DIR *source;
  struct dirent *entry;
  char path[PATH_MAX];
  
  source = opendir(src_dir);
  if (source == NULL) {
    printf("%s Failed to open source directory: %s\n", LOG_ERROR, src_dir);
    return 0;
  }

  while((entry = readdir(source)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    snprintf(path, PATH_MAX, "%s/%s", src_dir, entry->d_name);

    // it's a directory, recurse
    if (entry->d_type == DT_DIR) {
      collect_files(files, path, dst_dir);
    } else {
      char *ext = strrchr(path, '.');
      if(strcmp(ext, ".R") != 0 && strcmp(ext, ".r") != 0) continue;
      char buffer[20000];
      FILE *file = fopen(path, "r");
      size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
      buffer[bytesRead] = '\0';
      char *dest = make_dest_path(path, dst_dir);
      push_rfile(files, path, dest, buffer, NULL);
      free(dest);
      fclose(file);
    }
  }
  
  closedir(source);

  return 1;
}

// first pass:
// - capture defines
// - Run preflight
static int first_pass(RFile *files, Define **defs, Plugins *plugins)
{
  RFile *current = files;
  while(current != NULL) {
    overwrite(defs, "..FILE..", current->src);

    // state
    char *buffer = NULL;
    int line_number = -1;
    char *line_number_str = NULL;
    int in_preflight = 0;
    int in_macro = 0;

    // line
    char *pos = current->content;

    while (*pos) {
      line_number++;
      char *new_line = strchr(pos, '\n');
      if(!new_line) {
        break;
      }

      size_t len = new_line - pos;
      char *line = malloc(len + 1);
      strncpy(line, pos, len);
      line[len] = '\0';
      pos = new_line + 1;

      free(line_number_str);
      asprintf(&line_number_str, "%d", line_number);
      overwrite(defs, "..LINE..", line_number_str);

      if(enter_macro(line)) {
        in_macro = 1;
        buffer = append_buffer(buffer, line);
        free(line);
        continue;
      }

      if(strncmp(line, "#> endmacro", 11) == 0) {
        in_macro = 0;
        push_macro(defs, buffer, current->ns);
        buffer = NULL;
        free(line);
        continue;
      }

      if(in_macro) {
        buffer = append_buffer(buffer, line);
        free(line);
        continue;
      }
      
      capture_define(defs, line, current->ns);

      if(strncmp(line, "#> import ", 10) == 0) {
        free(line);
        continue;
      }

      if(strncmp(line, "#> preflight", 12) == 0) {
        in_preflight = 1;
        buffer = append_buffer(buffer, line);
        free(line);
        continue;
      }

      if(strncmp(line, "#> endpreflight", 15) == 0) {
        in_preflight = 0;
        free(line);
        continue;
      }

      if(strncmp(line, "#> endflight", 12) == 0) {
        in_preflight = 0;
        printf("%s Running preflight checks\n", LOG_INFO);
        SEXP result = evaluate(buffer);
        if(result == NULL) {
          printf("%s Preflight checks failed\n", LOG_ERROR);
          free(buffer);
          free(line);
          return 1;
        }
        free(buffer);
        buffer = NULL;
        free(line);
        continue;
      }

      if(in_preflight) {
        buffer = append_buffer(buffer, line);
        free(line);
        continue;
      }

      free(line);
    }

    free(line_number_str);
    free(buffer);  // Free buffer if preflight wasn't properly closed

    char *output = plugins_call(plugins, "preprocess", current->content, current->src);
    if(output != NULL) {
      free(current->content);
      current->content = strdup(output);
      free(output);
    }

    current = current->next;
  }

  return 0;
}

static int second_pass(RFile *files, Define **defs, Plugins *plugins, char *prepend, char *append, int sourcemap, Registry **registry, int dry_run)
{
  RFile *current = files;
  while(current != NULL) {
    if(current->dst == NULL) {
      current = current->next;
      continue;
    }
    printf("%s Copying %s to %s\n", LOG_INFO, current->src, current->dst);
    overwrite(defs, "..FILE..", current->src);

    // state
    char *buffer = NULL;
    char *for_buffer = NULL;
    int line_number = 0;
    char *line_number_str = NULL;
    int should_write = 1;
    int branch_taken = 0;
    int in_macro = 0;
    int in_for = 0;
    int err = 0;

    // Test collector
    TestCollector tc = {NULL, NULL, NULL, 0};

    // line
    char *pos = current->content;

    while (*pos) {
      line_number++;
      char *new_line = strchr(pos, '\n');
      if(!new_line) {
        break;
      }

      size_t len = new_line - pos;

      char *line = NULL;

      if(!sourcemap) {
        line = malloc(len + 1);
        strncpy(line, pos, len);
        line[len] = '\0';
      } else {
        line = malloc(len + 1);
        strncpy(line, pos, len);
        line[len] = '\0';
        line = add_sourcemap(line, line_number, current->src);
      }

      pos = new_line + 1;

      char *trimmed = remove_leading_spaces(line);

      if(enter_macro(trimmed)) {
        in_macro = 1;
        free(line);
        continue;
      }

      if(strncmp(trimmed, "#> endmacro", 11) == 0) {
        in_macro = 0;
        free(line);
        continue;
      }

      if(in_macro) {
        free(line);
        continue;
      }

      if(strncmp(trimmed, "#> import ", 10) == 0) {
        free(line);
        continue;
      }

      if(enter_for(trimmed)) {
        in_for = 1;
        for_buffer = strdup(line);
        free(line);
        continue;
      }

      if(in_for && !exit_for(trimmed)) {
        for_buffer = append_buffer(for_buffer, line);
        free(line);
        continue;
      }

      if(exit_for(trimmed)) {
        char *expanded = replace_for(for_buffer, line);
        free(for_buffer);
        free(line);
        line = expanded;
        in_for = 0;
      }

      free(line_number_str);
      asprintf(&line_number_str, "%d", line_number);
      overwrite(defs, "..LINE..", line_number_str);
      increment_counter(defs, line);

      char *fstring_result = fstring_replace(line, 0);
      char *included = include_replace(fstring_result, plugins, current->src, registry);
      if(fstring_result != line) free(line);
      if(included != fstring_result) free(fstring_result);

      char *replaced = define_replace(defs, included);
      free(included);

      char *deconstructed = deconstruct_replace(replaced);
      if(deconstructed != replaced) free(replaced);

      char *cnst = replace_const(deconstructed);
      if(cnst != deconstructed) free(deconstructed);

      // Test collection - if line was consumed, skip to next
      if(collect_test_line(&tc, cnst)) {
        free(cnst);
        continue;
      }

      // Check for preprocessor directives
      // Lines starting with #> are directives - always skip
      char *directive_check = remove_leading_spaces(cnst);
      if(strncmp(directive_check, "#> ", 3) == 0) {
        should_write = should_write_line(should_write, &branch_taken, cnst, defs);
        free(cnst);
        continue;
      }

      // For content lines (including # comments), check if we should write
      if(!should_write) {
        free(cnst);
        continue;
      }

      err = catch_error(cnst);

      if(err) {
        free(cnst);
        return 1;
      }

      buffer = append_buffer(buffer, cnst);
      free(cnst);
    }

    FILE *dst_file = fopen(current->dst, "w");
    if(dst_file == NULL) {
      printf("%s Failed to open %s\n", LOG_ERROR, current->dst);
      return 1;
    }
    char *output = plugins_call(plugins, "postprocess", buffer, current->src);
    if(prepend != NULL) {
      FILE *prepend_file = fopen(prepend, "r");
      if(prepend_file == NULL) {
        printf("%s Failed to open %s\n", LOG_ERROR, prepend);
        return 1;
      }
      char prepend_buffer[1024];
      while(fgets(prepend_buffer, sizeof(prepend_buffer), prepend_file) != NULL) {
        fputs(prepend_buffer, dst_file);
      }
      fclose(prepend_file);
    }

    if(output != NULL) {
      fputs(output, dst_file);
      free(output);
    } else if(buffer != NULL) {
      fputs(buffer, dst_file);
    }

    if(append != NULL) {
      FILE *append_file = fopen(append, "r");
      if(append_file == NULL) {
        printf("%s Failed to open %s\n", LOG_ERROR, append);
        return 1;
      }
      char append_buffer[1024];
      while(fgets(buffer, sizeof(append_buffer), append_file) != NULL) {
        fputs(buffer, dst_file);
      }
      fclose(append_file);
    }

    fclose(dst_file);
    free(buffer);

    if(!dry_run) {
      write_tests(tc.tests, current->src);
    }

    free(line_number_str);

    current = current->next;
  }

  return 0;
}

int two_pass(Arguments *args)
{
  int first_pass_result = first_pass(args->files, args->defs, args->plugins);
  if(first_pass_result) {
    return 1;
  }

  int second_pass_result = second_pass(
    args->files,
    args->defs,
    args->plugins,
    args->prepend,
    args->append,
    args->sourcemap,
    args->registry,
    args->dry_run
  );
  if(second_pass_result) {
    return 1;
  }

  if(args->deadcode) {
    analyse_deadcode(args->files);
  }

  return 0;
}
