---
title: Plugins
---

# Plugins

Builder supports plugins to extend processing.
Plugins are R packages that export a function returning a list of lifecycle methods.

Note, you can always make use tof the `builder.ini` file for configuration if needed.

---

## Using Plugins

### Via CLI

Specify plugins with the `-plugin` flag:

```bash
builder -plugin package::function
```

### Via Config File

Add plugins to your `builder.ini`:

```ini
plugin: package::function
```

### Multiple Plugins

You can use multiple plugins. They are called in order.

**CLI:**
```bash
builder -plugin pkg1::fn1 pkg2::fn2
```

**Config (space-separated):**
```ini
plugin: pkg1::fn1 pkg2::fn2
```

---

## Developing Plugins

### Overview

A plugin is an R function that returns a list of lifecycle methods. Each method is called at a specific point during the build process.

See [builder.air](https://github.com/devOpifex/builder.air), and 
[fassplyr](https://github.com/devOpifex/fassplyr)
for real-world examples.

### Lifecycle Methods

#### setup(input, output)

Called during initialization with the input and output directory paths. Use this to store configuration or perform one-time setup.

#### preprocess(str, file)

Called on each file's content **before** Builder processes it. Receives the file content as a string and should return the modified content.

#### postprocess(str, file)

Called on each file's content **after** Builder processes it. Receives the file content as a string and should return the modified content.

#### include(type, path, object, file)

Called for each `#> include` directive with parsed components:

- `type` - The file type (e.g., `"csv"`, `"json"`)
- `path` - The file path to include
- `object` - The variable name for the result
- `file` - The source file being processed

Return `NULL` to use default processing, or return a replacement line.

#### end()

Called when Builder finishes processing all files. Use this for cleanup or final operations.

### Examples

A simple minifier plugin that removes empty lines and joins with semicolons
(_It's doing it wrong, don't actually use this!_):

```r
#' @export
plugin <- function() {
 list(
   setup = function(input, output, ...) {
     # Store paths or initialize state
   },
   preprocess = function(str, file, ...) {
     # Modify content before processing
     lines <- strsplit(str, "\n")[[1]]
     lines <- lines[nzchar(trimws(lines))]
     paste(lines, collapse = ";")
   },
   postprocess = function(str, file, ...) {
     # Modify content after processing
     str
   },
    include = function(type, path, object, file, ...) {
      # Handle #> include directives
      # Return NULL to use default processing
      NULL
    },
   end = function(...) {
     # Cleanup or final operations
   }
 )
}
```

A plugin that uses [readr](https://readr.tidyverse.org/) to read CSV files
(and overrides the default `csv` type which uses the built-in `read.csv`):

```r
#' @export
plugin <- function() {
  enabled <- FALSE
 list(
   setup = function(input, output, ...) {
     enabled <<- requireNamespace("readr", quietly = TRUE)
     warning("readr not installed, fallback on default `csv` support")
   },
   include = function(type, path, object, file, ...) {
     if(!enabled) return(NULL)
     if(type != "csv") return(NULL)
     readr::read_csv(path)
   },
   end = function(...) {
   }
 )
}
```

A formatter plugin that uses [air](https://github.com/posit-dev/air) to format the files:

```r
#' @export
plugin <- function() {
  out_dir <- NULL
  list(
    setup = function(input, output, ...) {
      out_dir <<- output
    },
    end = function(...) {
      if (is.null(out_dir)) {
        return()
      }

      system2("air", c("format", out_dir))
    },
    include = function(type, path, object, file, ...) {}
  )
}
```

### Tips

- Use `...` in function signatures for forward compatibility
- Methods that don't modify content can return `NULL`
- If your plugin requires configuration, read from a config file in `setup()`
- Plugins are called in the order they are specified

### Using R6 Classes

For plugins with complex state or when you want inheritance, you can use R6 classes:

```r
#' @export
Plugin <- R6::R6Class("Plugin",
  private = list(
    out_dir = NULL,
    file_count = 0
  ),
  public = list(
    setup = function(input, output, ...) {
      private$out_dir <- output
    },
    preprocess = function(str, file, ...) {
      str
    },
    postprocess = function(str, file, ...) {
      private$file_count <- private$file_count + 1
      str
    },
    include = function(type, path, object, file, ...) {
      NULL
    },
    end = function(...) {
      message(sprintf("Processed %d files", private$file_count))
    }
  )
)

#' @export
plugin <- \() Plugin$new()
```

This approach offers:

- **Encapsulated state** - Private fields instead of `<<-` assignments
- **Inheritance** - Create plugin families with shared behavior
- **Testability** - Instantiate and test methods directly
