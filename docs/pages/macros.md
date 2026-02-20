---
title: Macros
---

# Macros

Macros are function-like preprocessor directives that allow you to define reusable code templates with parameters. Unlike simple `#> define` replacements, macros can accept arguments and expand into multiple lines of code.

## Advantages

- **Easy metaprogramming:** Write code that generates code, enabling powerful abstractions and code generation patterns.
- **Zero overhead:** Macros have no runtime performance cost since they're expanded at compile time.
- **Compile time expansion:** All macro processing happens during preprocessing, before your code executes.

## Basic Syntax

Macros are defined using `#> macro` on its own line, followed by a function definition. The macro ends with `#> endmacro`.

**Syntax:**

```r
#> macro
MACRO_NAME <- function(arg1, arg2, ...) {
  body using .arg1, .arg2, etc.
}
#> endmacro
```

## Global vs Local Macros

By default, macros are **global** - expanded code is inserted directly. Use `#> macro local` to wrap expansion in `local({...})`, preventing side effects on the calling environment.

```r
#> macro
SETUP_ENV <- function(name) {
  .name_env <- new.env()
}
#> endmacro

SETUP_ENV(app)  # Expands to: app_env <- new.env()
```

```r
#> macro local
LOG_INFO <- function(msg) {
  cat("[INFO] ", .msg, "\n", sep = "")
}
#> endmacro

LOG_INFO("Started")  # Expands to: local({ cat("[INFO] ", "Started", "\n", sep = "") })
```

| Type | Use When |
|------|----------|
| `#> macro` (global) | Need to create/modify variables in calling scope |
| `#> macro local` | Self-contained operations, no side effects |

## Argument Syntax

Macro arguments use explicit markers for replacement:

| Syntax | Description | Example (`name` = `count`) |
|--------|-------------|----------------------------|
| `name` | Not replaced (literal) | `name` stays `name` |
| `.name` | Value replacement | `.name` becomes `count` |
| `..name` | Stringification | `..name` becomes `"count"` |

**Note:** Macro argument names cannot start with `.`.

### Example

This macro demonstrates value replacement (`.var`), stringification (`..var`), and token pasting (`.name` within identifiers):

```r
#> macro
ACCESSOR <- function(name) {
  get_.name <- function() {
    cat("Accessing:", ..name, "\n")
    private$.name
  }
}
#> endmacro

ACCESSOR(count)
```

**Expands to:**

```r
get_count <- function() {
  cat("Accessing:", "count", "\n")
  private$count
}
```

## Namespaced Macros

When importing macros from a package using `#> import pkg::file.rh`, macros are prefixed with the package namespace to avoid naming conflicts.

```r
#> import logger::macros.rh
```

If `macros.rh` (in the `logger` package's `inst/` directory) defines:

```r
#> macro
LOG <- function(msg) {
  cat("[LOG] ", .msg, "\n", sep = "")
}
#> endmacro
```

You call it with the namespace prefix:

```r
logger::LOG("Application started")
```

## Notes

- Macros can span up to 1024 lines
- Use `#> define NAME value` for simple constants (not function-like macros)
