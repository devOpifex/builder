#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "create.h"
#include "define.h"
#include "depends.h"
#include "file.h"
#include "include.h"
#include "log.h"
#include "parser.h"
#include "plugins.h"
#include "r.h"
#include "watch.h"

#define VERSION "0.0.2"

static int build(BuildContext* ctx)
{
  Define* defines = create_define();
  get_definitions(defines, ctx->argc, ctx->argv);

  if (!ctx->dry_run && ctx->must_clean) {
    printf("%s Cleaning: %s and testthat/\n", LOG_INFO, ctx->output);
    walk(ctx->output, clean);
  }

  RFile* files = NULL;
  int success = collect_files(&files, ctx->input, ctx->output);

  if (!success) {
    printf("%s Failed to collect files\n", LOG_ERROR);
    free_array(defines);
    return 1;
  }

  if (!resolve_imports(&files, ctx->imports)) {
    printf("%s Failed to resolve imports\n", LOG_ERROR);
    free_rfile(files);
    free_array(defines);
    return 1;
  }

  int ok = process_depends(ctx->depends);
  if (ok) {
    printf("%s Failed to process depends\n", LOG_ERROR);
    free_rfile(files);
    free_array(defines);
    return 1;
  }

  Arguments args = {.files = files,
                    .defs = &defines,
                    .plugins = ctx->plugins,
                    .prepend = ctx->prepend,
                    .append = ctx->append,
                    .bundle = ctx->bundle,
                    .sourcemap = ctx->sourcemap,
                    .deadcode = ctx->deadcode,
                    .registry = &ctx->registry,
                    .dry_run = ctx->dry_run,
                    .strip = ctx->strip,
                    .input = ctx->input,
                    .output = ctx->output};

  int result = two_pass(&args);

  if (ctx->diff) {
    printf("%s Running diff\n", LOG_INFO);

    RFile* current = files;
    while (current != NULL) {
      if (!current->dst) {
        current = current->next;
        continue;
      }
      // Get relative path by skipping ctx->output prefix
      char* relative = current->dst + strlen(ctx->output);
      // Construct original file path
      char* original_file = malloc(strlen(ctx->output_original) + strlen(relative) + 2);
      sprintf(original_file, "%s/%s", ctx->output_original, relative);
      // Run diff between original and dry-run output
      char* cmd = malloc(strlen(original_file) + strlen(current->dst) + strlen("diff --color --side-by-side") + 3);
      sprintf(cmd, "diff --color --side-by-side %s %s", original_file, current->dst);
      system(cmd);
      free(cmd);
      free(original_file);
      current = current->next;
    }
  }

  free_rfile(files);
  free_array(defines);

  if (result) {
    printf("%s Failed to process files\n", LOG_ERROR);
    return 1;
  }

  printf("%s All built!\n", LOG_SUCCESS);
  return 0;
}

int main(int argc, char* argv[])
{
  if (has_arg(argc, argv, "-version")) {
    printf("Builder v%s\n", VERSION);
    return 0;
  }

  if (has_arg(argc, argv, "-init")) {
    char* path = (char*)malloc(2);
    strcpy(path, ".");
    create_config(path);
    return 0;
  }

  char* create = get_arg_value(argc, argv, "-create");
  if (create != NULL) {
    create_package(create);
    return 0;
  }

  if (has_arg(argc, argv, "-help")) {
    printf("builder - R package preprocessor with macro support\n\n");
    printf("Usage: builder [OPTIONS]\n\n");

    printf("Input/Output:\n");
    printf("  -input <path>           Input directory (default: srcr/)\n");
    printf("  -output <path>          Output directory (default: R/)\n");
    printf(
        "  -noclean                Skip cleaning output directory before "
        "build\n");
    printf("\n");

    printf("Build Options:\n");
    printf(
        "  -watch                  Watch input directory and rebuild on "
        "changes\n");
    printf("  -deadcode               Enable dead variable/function detection\n");
    printf("  -sourcemap              Enable source map generation\n");
    printf(
        "  -dry-run                Build to temporary directory without "
        "modifying output\n");
    printf(
        "  -diff                   Show differences between -dry-run output "
        "and existing output\n");
    printf(
        "  -strip                  Strip comments, preserves special "
        "comments: #'\n");
    printf("\n");

    printf("Preprocessing:\n");
    printf(
        "  -D<NAME> [value]        Define a macro, e.g., -DDEBUG or -DVALUE "
        "42\n");
    printf(
        "  -import <file> ...      Import .rh header files, e.g., -import "
        "inst/main.rh\n");
    printf(
        "  -reader <type> <fn>     Define file type reader, e.g., -reader "
        "tsv read.delim\n");
    printf("\n");

    printf("File Injection:\n");
    printf(
        "  -prepend <path>         Prepend file contents to every output "
        "file\n");
    printf(
        "  -append <path>          Append file contents to every output "
        "file\n");
    printf("  -bundle <path>          Bundle all output into a single file\n");
    printf("\n");

    printf("Plugins & Dependencies:\n");
    printf("  -plugin <pkg::fn> ...   Use plugins, e.g., -plugin pkg::minify\n");
    printf(
        "  -depends <pkg> ...      Declare package dependencies, e.g., "
        "-depends rlang\n");
    printf("\n");

    printf("Project Setup:\n");
    printf("  -init                   Create a builder.ini config file\n");
    printf("  -create <name>          Create a new package skeleton\n");
    printf("\n");

    printf("Info:\n");
    printf("  -version                Show version number\n");
    printf("  -help                   Show this help message\n");
    printf("\n");

    printf("Examples:\n");
    printf("  builder -input srcr/ -output R/ -DDEBUG\n");
    printf("  builder -watch -deadcode\n");
    printf("  builder -plugin pkg::minify -depends rlang dplyr\n");
    printf("  builder -init\n");
    return 0;
  }

  set_R_home();

  char* r_argv[] = {"R", "--silent", "--no-save"};
  Rf_initEmbeddedR(3, r_argv);

  Registry* registry = initialize_registry();

  BuildContext* cfg = NULL;
  if (has_config()) {
    cfg = get_config(&registry);
  }

  char* input = get_arg_value(argc, argv, "-input");
  if (input == NULL && cfg != NULL && cfg->input != NULL) {
    input = strdup(cfg->input);
  }
  if (input == NULL) {
    input = strdup("srcr/");
    printf("%s No -input, defaulting to srcr\n", LOG_WARNING);
  }
  if (input == NULL) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    free_config(cfg);
    return 1;
  }
  input = strip_last_slash(input);
  if (!exists(input)) {
    printf("%s Input directory does not exist\n", LOG_ERROR);
    free_config(cfg);
    free(input);
    return 1;
  }

  char* output = get_arg_value(argc, argv, "-output");
  if (output == NULL && cfg != NULL && cfg->output != NULL) {
    output = strdup(cfg->output);
  }

  char* output_copy = NULL;
  int dry_run = has_arg(argc, argv, "-dry-run");
  if (dry_run) {
    output_copy = strdup(output);
    char* template = strdup("/tmp/dryrun-XXXXXX");
    free(output);
    output = mkdtemp(template);
    if (output == NULL) {
      printf("%s Failed to create temporary directory\n", LOG_ERROR);
      free_config(cfg);
      free(input);
      return 1;
    }
  }

  if (output == NULL) {
    output = strdup("R/");
    printf("%s No -output, defaulting to R\n", LOG_WARNING);
  }

  if (output == NULL) {
    printf("%s Failed to allocate memory\n", LOG_ERROR);
    free_config(cfg);
    free(input);
    return 1;
  }

  if (!exists(output)) {
    printf("%s Output directory does not exist\n", LOG_ERROR);
    free_config(cfg);
    free(input);
    free(output);
    return 1;
  }

  output = ensure_dir(output);

  Value* imports = get_arg_values(argc, argv, "-import");
  if (imports == NULL && cfg != NULL && cfg->imports != NULL) {
    Value* current = cfg->imports;
    while (current != NULL) {
      imports = push_value(imports, strdup(current->name));
      current = current->next;
    }
  }

  Value* plugins_str = get_arg_values(argc, argv, "-plugin");
  if (plugins_str == NULL && cfg != NULL && cfg->plugins_str != NULL) {
    Value* current = cfg->plugins_str;
    while (current != NULL) {
      plugins_str = push_value(plugins_str, strdup(current->name));
      current = current->next;
    }
  }

  Value* depends = get_arg_values(argc, argv, "-depends");
  if (depends == NULL && cfg != NULL && cfg->depends != NULL) {
    Value* current = cfg->depends;
    while (current != NULL) {
      depends = push_value(depends, strdup(current->name));
      current = current->next;
    }
  }

  if (plugins_str != NULL) {
    printf("%s Using plugins:", LOG_INFO);
    Value* current = plugins_str;
    while (current != NULL) {
      printf(" %s", current->name);
      current = current->next;
    }
    printf("\n");
  }

  Plugins* plugins = plugins_init(plugins_str, input, output);
  free_value(plugins_str);

  int p_failed = plugins_failed(plugins);
  if (p_failed) {
    printf("%s Failed to initialize plugin(s) - stopping execution\n", LOG_ERROR);
    free_config(cfg);
    free(input);
    free(output);
    free_value(depends);
    free_value(imports);
    return 1;
  }

  char* prepend = get_arg_value(argc, argv, "-prepend");
  if (prepend == NULL && cfg != NULL && cfg->prepend != NULL) {
    prepend = strdup(cfg->prepend);
  }
  if (prepend != NULL) {
    printf("%s Prepending: %s\n", LOG_INFO, prepend);
  }

  char* append = get_arg_value(argc, argv, "-append");
  if (append == NULL && cfg != NULL && cfg->append != NULL) {
    append = strdup(cfg->append);
  }
  if (append != NULL) {
    printf("%s Appending: %s\n", LOG_INFO, append);
  }

  char* bundle = get_arg_value(argc, argv, "-bundle");
  if (bundle == NULL && cfg != NULL && cfg->bundle != NULL) {
    bundle = strdup(cfg->bundle);
  }
  if (bundle != NULL) {
    printf("%s Bundling to: %s\n", LOG_INFO, bundle);
  }

  int deadcode = has_arg(argc, argv, "-deadcode");
  if (!deadcode && cfg != NULL) {
    deadcode = cfg->deadcode;
  }

  int sourcemap = has_arg(argc, argv, "-sourcemap");
  if (!sourcemap && cfg != NULL) {
    sourcemap = cfg->sourcemap;
  }

  int must_clean = 1;
  if (has_arg(argc, argv, "-noclean")) {
    must_clean = 0;
  } else if (cfg != NULL) {
    must_clean = cfg->must_clean;
  }

  int watch_mode = has_arg(argc, argv, "-watch");
  if (!watch_mode && cfg != NULL) {
    watch_mode = cfg->watch;
  }

  for (int i = 1; i < argc - 2; i++) {
    if (strcmp(argv[i], "-reader") == 0) {
      push_registry(&registry, argv[i + 1], argv[i + 2]);
      i += 2;
    }
  }

  free_config(cfg);

  int diff = has_arg(argc, argv, "-diff");
  if (diff && !dry_run) {
    printf("%s -diff requires -dry-run\n", LOG_ERROR);
    return 1;
  }

  BuildContext ctx = {.argc = argc,
                      .argv = argv,
                      .input = input,
                      .output = output,
                      .output_original = output_copy,
                      .imports = imports,
                      .plugins_str = NULL,
                      .prepend = prepend,
                      .append = append,
                      .bundle = bundle,
                      .deadcode = deadcode,
                      .sourcemap = sourcemap,
                      .must_clean = must_clean,
                      .watch = watch_mode,
                      .plugins = plugins,
                      .registry = registry,
                      .depends = depends,
                      .diff = diff,
                      .dry_run = dry_run,
                      .strip = has_arg(argc, argv, "-strip")};

  int result = 0;

  if (watch_mode) {
    printf("%s Watch mode enabled, monitoring %s\n", LOG_INFO, input);

    int fd = watch_init(input);
    if (fd == -1) {
      result = 1;
    } else {
      build(&ctx);
      ctx.must_clean = 0;

      while (1) {
        printf("%s Waiting for changes...\n", LOG_INFO);
        if (!watch_wait(fd)) break;
        printf("%s Change detected, rebuilding...\n", LOG_INFO);
        build(&ctx);
      }

      watch_close(fd);
    }
  } else {
    result = build(&ctx);
  }

  plugins_call(plugins, "end", NULL, NULL);
  free_plugins(plugins);
  free_registry(registry);
  free(prepend);
  free(append);
  free(bundle);
  free_value(imports);
  free_value(depends);
  free(input);

  if (has_arg(argc, argv, "-dry-run")) {
    walk(output, clean);
    rmdir(output);
  }
  free(output);

  Rf_endEmbeddedR(0);

  return result;
}
