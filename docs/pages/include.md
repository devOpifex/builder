---
title: Include
---

# Include

The `#> include` directive allows you to read external files at build time and embed their contents directly into your R code as variables. This enables you to keep SQL queries, data files, templates, and other content in separate files while embedding them at build time.

## Advantages

- **Separation of concerns:** Keep SQL queries, templates, and other content in separate files while still embedding them in your code at build time.
- **Build-time processing:** File contents are read and processed during preprocessing, not at runtime, eliminating file I/O overhead.
- **Type safety:** File contents are converted to R objects at build time, catching errors early.
- **Built-in support:** Common file types are supported out of the box with no configuration needed.

## Syntax

```
#> include:TYPE file_path variable_name
```

The directive consists of three space-separated parts:

- `TYPE` - The file type (e.g., `txt`, `csv`, `json`) which determines how the file is read
- `file_path` - The path to the file to include
- `variable_name` - The R variable name that will be assigned the result

## Built-in Readers

Builder includes readers for common file types:

| Type | Function |
|------|----------|
| `txt` | `readLines` |
| `sql` | `readLines` |
| `csv` | `read.csv` |
| `tsv` | `read.delim` |
| `rds` | `readRDS` |
| `json` | `jsonlite::fromJSON` |
| `yml` | `yaml::read_yaml` |
| `yaml` | `yaml::read_yaml` |
| `xml` | `xml2::read_xml` |
| `xlsx` | `readxl::read_excel` |
| `parquet` | `arrow::read_parquet` |
| `fst` | `fst::read_fst` |
| `base64` | `base64enc::base64encode` |

## Basic Examples

### Reading SQL Files

```r
#> include:sql queries/users.sql user_query
```

**Expands to:**

```r
user_query <- c("SELECT * FROM users", "WHERE active = 1")
```

### Reading CSV Data

Embed small lookup tables or static data directly in your code:

```r
#> include:csv data/lookup_table.csv lookup_data
```

**Expands to:**

```r
lookup_data <- structure(list(id = 1:3, name = c("a", "b", "c")), class = "data.frame", row.names = c(NA, -3L))
```

### Reading JSON Configuration

```r
#> include:json config/settings.json app_config
```

### Embedding Binary Files as Base64

Embed images or other binary files as base64-encoded strings:

```r
#> include:base64 assets/logo.png logo_data
```

**Expands to:**

```r
logo_data <- "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJ..."
```

This is useful for embedding small assets directly in your R code without external file dependencies at runtime.

## Custom Readers

You can define custom readers for new file types or override the defaults using the `-reader` CLI argument or `reader:` config option.

### Via CLI

```bash
builder -reader tsv '\(x) read.delim(x, sep="\t", header=FALSE)'
```

### Via Config File

```ini
reader: tsv \(x) read.delim(x, sep="\t", header=FALSE)
reader: myformat \(x) mypackage::read_myformat(x)
```

### Custom Reader Format

The reader function must accept a single argument (the file path). You can use:

- Simple function names: `readLines`, `read.csv`
- Namespaced functions: `jsonlite::fromJSON`
- Lambda functions: `\(x) read.csv(x, header=FALSE)`

When you override a built-in reader, a warning is displayed:

```
[WARNING] Overriding reader for 'tsv'
```

## How It Works

When the preprocessor encounters an `#> include` directive:

1. It parses the file type, file path, and variable name
2. It looks up the reader function for that file type
3. It constructs an R expression: `reader_function('file_path')`
4. It captures the result using `dput()` and `capture.output()`
5. It generates an assignment: `variable_name <- <result>`

This happens at build time, so the final R code contains no file reading operations—just the embedded data.

## Extending via Plugins

For more complex include handling, you can use a [plugin](/plugins) that implements the `include` hook. This allows you to intercept and transform `#> include` directives with custom logic.

```r
plugin <- function(input, output) {
  list(
    include = function(type, path, object, file) {
      # type = file type (e.g., "csv")
      # path = file path to include
      # object = variable name for result
      # file = source file being processed
      
      # Return NULL to use default processing
      # Or return a replacement line
      NULL
    }
  )
}
```

See [Plugins](/plugins) for more details.

## Important Notes

- File paths are relative to the build directory or can be absolute paths
- The result is captured using R's `dput()` function, which converts R objects to their code representation
- File reading happens at build time, not runtime, so files must exist when running the builder
- Changes to included files require rebuilding to be reflected in the output
- For readers that require external packages (json, yaml, etc.), ensure the package is installed
- Variable names should follow R naming conventions

## Processing Pipeline

The `#> include` directive is processed in the second pass, after macro definitions are collected but before macro expansion.

1. First pass: All `#> define` constants and `#> macro` definitions are collected
2. Second pass: `#> include` directives are processed, then macros are expanded
3. The result is pure R code with all preprocessing directives removed

See [Architecture](/architecture) for the complete processing pipeline.

## Common Use Cases

- **SQL Queries:** Keep SQL in `.sql` files with proper syntax highlighting and embed them in R packages
- **Configuration:** Store configuration in JSON, YAML, or other formats and embed at build time
- **Static Data:** Include small reference datasets or lookup tables directly in code
- **Templates:** Store HTML, Markdown, or text templates externally and include them for string generation
- **Constants:** Define large constant structures in separate files for better organization
