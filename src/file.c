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
#include "foreach.h"
#include "define.h"
#include "include.h"
#include "fstring.h"
#include "assert.h"
#include "const.h"
#include "error.h"
#include "file.h"
#include "test.h"
#include "log.h"
#include "for.h"
#include "r.h"
#include "buffer.h"
#include "deadcode.h"
#include "s7check.h"

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
    LOG_ERROR("Failed to allocate memory");
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

int walk(char *src_dir, Callback func)
{
  DIR *source;
  struct dirent *entry;
  char path[PATH_MAX];
  
  source = opendir(src_dir);
  if (source == NULL) {
    LOG_ERROR("Failed to open source directory: %s", src_dir);
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
      if(ext == NULL) continue;
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
    LOG_ERROR("Failed to allocate memory");
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
    LOG_ERROR("Failed to resolve import: %s", import_spec);
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
    LOG_ERROR("Failed to open import: %s", resolved_path);
    free(resolved_path);
    return 0;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = malloc(size + 1);
  int read_result = fread(content, 1, size, file);
  if (read_result != size) {
    LOG_ERROR("Failed to read import: %s", resolved_path);
    free(resolved_path);
    return 0;
  }
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
  LOG_INFO("Import: %s", resolved_path);

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
    LOG_ERROR("Failed to open source directory: %s", src_dir);
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
      if(ext == NULL) continue;
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
static int first_pass(Arguments *args)
{
  RFile *current = args->files;
  while(current != NULL) {
    overwrite(args->defs, "..FILE..", current->src);

    Buffer *buffer = buffer_init();
    int line_number = -1;
    char *line_number_str = NULL;
    int in_preflight = 0;
    int in_macro = 0;

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
      int number_astr = asprintf(&line_number_str, "%d", line_number);
      if (number_astr == -1) {
        LOG_ERROR("Failed to allocate memory");
        return 1;
      }
      overwrite(args->defs, "..LINE..", line_number_str);

      if(enter_macro(line)) {
        in_macro = 1;
        buffer_append_nl(buffer, line);
        free(line);
        continue;
      }

      if(strncmp(line, "#> endmacro", 11) == 0) {
        in_macro = 0;
        push_macro(args->defs, strdup(buffer->data), current->ns);
        buffer_reset(buffer);
        free(line);
        continue;
      }

      if(in_macro) {
        buffer_append_nl(buffer, line);
        free(line);
        continue;
      }
      
      capture_define(args->defs, line, current->ns);

      if(strncmp(line, "#> import ", 10) == 0) {
        free(line);
        continue;
      }

      if(strncmp(line, "#> preflight", 12) == 0) {
        in_preflight = 1;
        buffer_append_nl(buffer, line);
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
        LOG_INFO("Running preflight checks");
        SEXP result = evaluate(buffer->data);
        if(result == NULL) {
          LOG_ERROR("Preflight checks failed");
          buffer_free(buffer);
          free(line);
          return 1;
        }
        buffer_reset(buffer);
        free(line);
        continue;
      }

      if(in_preflight) {
        buffer_append_nl(buffer, line);
        free(line);
        continue;
      }

      int valid = valid_assert(line);
      if(!valid) {
        free(line);
        return 1;
      }

      free(line);
    }

    free(line_number_str);
    buffer_free(buffer);

    char *output = plugins_call(args->plugins, "preprocess", current->content, current->src);
    if(output != NULL) {
      free(current->content);
      current->content = strdup(output);
      free(output);
    }

    current = current->next;
  }

  return 0;
}

static int second_pass(Arguments *args)
{
  Buffer *bundle_buf = buffer_init();
  int bundling = args->bundle != NULL;

  RFile *current = args->files;
  while(current != NULL) {
    if(current->dst == NULL) {
      current = current->next;
      continue;
    }
    LOG_INFO("Copying %s to %s", current->src, current->dst);
    overwrite(args->defs, "..FILE..", current->src);

    Buffer *buf = buffer_init();
    Buffer *for_buf = buffer_init();
    Buffer *foreach_buf = buffer_init();
    int line_number = 0;
    char *line_number_str = NULL;
    int should_write = 1;
    int branch_taken = 0;
    int in_macro = 0;
    int in_for = 0;
    int in_foreach = 0;
    int err = 0;

    if(args->prepend != NULL) {
      FILE *prepend_file = fopen(args->prepend, "r");
      if(prepend_file == NULL) {
        LOG_ERROR("Failed to open %s", args->prepend);
        return 1;
      }
      char prepend_buffer[1024];
      while(fgets(prepend_buffer, sizeof(prepend_buffer), prepend_file) != NULL) {
        buffer_append(buf, prepend_buffer);
      }
      fclose(prepend_file);
    }

    TestCollector tc = {NULL, NULL, NULL, 0};

    char *pos = current->content;

    while (*pos) {
      line_number++;
      char *new_line = strchr(pos, '\n');
      if(!new_line) {
        break;
      }

      size_t len = new_line - pos;

      char *line = NULL;

      if(!args->sourcemap) {
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

      if(enter_foreach(trimmed)) {
        in_foreach = 1;
        buffer_reset(foreach_buf);
        buffer_append_nl(foreach_buf, line);
        free(line);
        continue;
      }

      if(in_foreach && !exit_foreach(trimmed)) {
        buffer_append_nl(foreach_buf, line);
        free(line);
        continue;
      }

      if(exit_foreach(trimmed)) {
        char *expanded = replace_foreach(foreach_buf->data, line);
        free(line);
        line = expanded;
        in_foreach = 0;
      }

      if(enter_for(trimmed)) {
        in_for = 1;
        buffer_reset(for_buf);
        buffer_append_nl(for_buf, line);
        free(line);
        continue;
      }

      if(in_for && !exit_for(trimmed)) {
        buffer_append_nl(for_buf, line);
        free(line);
        continue;
      }

      if(exit_for(trimmed)) {
        char *expanded = replace_for(for_buf->data, line);
        free(line);
        line = expanded;
        in_for = 0;
      }

      free(line_number_str);
      int number_astr = asprintf(&line_number_str, "%d", line_number);
      if (number_astr == -1) {
        LOG_ERROR("Failed to allocate memory");
        return 1;
      }
      overwrite(args->defs, "..LINE..", line_number_str);
      increment_counter(args->defs, line);

      char *fstring_result = fstring_replace(line, 0);
      char *included = include_replace(fstring_result, args->plugins, current->src, args->registry);
      if(fstring_result != line) free(line);
      if(included != fstring_result) free(fstring_result);

      char *replaced = define_replace(args->defs, included);
      free(included);

      char *deconstructed = deconstruct_replace(replaced);
      if(deconstructed != replaced) free(replaced);

      char *cnst = replace_const(deconstructed);
      if(cnst != deconstructed) free(deconstructed);

      if(collect_test_line(&tc, cnst)) {
        free(cnst);
        continue;
      }

      err = catch_error(cnst);

      if(err) {
        free(cnst);
        return 1;
      }

      catch_warning(cnst);
      catch_deprecated(cnst);

      char *directive_check = remove_leading_spaces(cnst);
      if(strncmp(directive_check, "#> ", 3) == 0) {
        should_write = should_write_line(should_write, &branch_taken, cnst, args->defs);
        free(cnst);
        continue;
      }

      if(!should_write) {
        free(cnst);
        continue;
      }

      if(args->strip && strncmp(directive_check, "#", 1) == 0 && strncmp(directive_check, "#'", 2) != 0){
        free(cnst);
        continue;
      }

      if(buf->size > 0 && buf->data[buf->size - 1] != '\n')
        buffer_append(buf, "\n");
      buffer_append(buf, cnst);
      free(cnst);
    }

    if(args->append != NULL) {
      FILE *append_file = fopen(args->append, "r");
      if(append_file == NULL) {
        LOG_ERROR("Failed to open %s", args->append);
        return 1;
      }
      char app_buffer[1024];
      while(fgets(app_buffer, sizeof(app_buffer), append_file) != NULL) {
        buffer_append(buf, app_buffer);
      }
      fclose(append_file);
    }

    char *output = plugins_call(args->plugins, "postprocess", buf->data, current->src);

    if(bundling) {
      if(output != NULL) {
        if(bundle_buf->size > 0 && bundle_buf->data[bundle_buf->size - 1] != '\n')
          buffer_append(bundle_buf, "\n");
        buffer_append(bundle_buf, output);
      } else if(buf->size > 0) {
        if(bundle_buf->size > 0 && bundle_buf->data[bundle_buf->size - 1] != '\n')
          buffer_append(bundle_buf, "\n");
        buffer_append(bundle_buf, buf->data);
      }
    } else {
      FILE *dst_file = fopen(current->dst, "w");
      if(dst_file == NULL) {
        LOG_ERROR("Failed to open %s", current->dst);
        return 1;
      }
      if(output != NULL) {
        fputs(output, dst_file);
      } else if(buf->size > 0) {
        fputs(buf->data, dst_file);
      }
      fclose(dst_file);
    }

    buffer_free(buf);
    buffer_free(for_buf);
    free(output);

    if(!args->dry_run) {
      write_tests(tc.tests, current->src);
    }

    free(line_number_str);

    current = current->next;
  }

  if(bundling && bundle_buf->size > 0) {
    FILE *bundle_file = fopen(args->bundle, "w");
    if(bundle_file == NULL) {
      LOG_ERROR("Failed to open bundle file %s", args->bundle);
      buffer_free(bundle_buf);
      return 1;
    }
    fputs(bundle_buf->data, bundle_file);
    fclose(bundle_file);
    buffer_free(bundle_buf);
    LOG_INFO("Bundled to %s", args->bundle);
    return 0;
  }
  
  buffer_free(bundle_buf);

  return 0;
}

int two_pass(Arguments *args)
{
  int first_pass_result = first_pass(args);
  if(first_pass_result) {
    return 1;
  }

  int second_pass_result = second_pass(args);

  if(second_pass_result) {
    return 1;
  }

  if(args->deadcode) {
    analyse_deadcode(args->files);
  }

  if(args->s7) {
    analyse_s7(args->files);
  }

  return 0;
}
