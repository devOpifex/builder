---
title: Directives
---

# Directives

Builder lets you use directives in your R files and via the command line.

## Command line

You can pass definitions via the command line using `-D` flags. This is useful for setting build-time constants without modifying your source files.

**Syntax:** `-D<NAME>` for boolean flags, or `-D<NAME> <value>` for valued definitions.

```bash
./builder -input srcr -output R -DDEBUG -DTEST '"a string"' -DXXX 42
```

Note: Command-line definitions override file-based `#> define` directives.

## #> define

Define constants that will be replaced throughout your code. All occurrences of the defined name will be replaced with the specified value.

**Syntax:** `#> define NAME value`

```r
#> define PI 3.14159
#> define STRING "hello world!"

x <- PI  # becomes: x <- 3.14159
print(STRING)  # becomes: print("hello world!")
```

**Note:** For function-like macros with parameters, use `#> macro` instead. See [Macros](/macros) for details.

## #> ifdef

Include a code block only if the specified name is defined (either via `#> define` or command-line `-D` flag).

**Syntax:** `#> ifdef NAME ... #> endif`

```r
#> ifdef DEBUG
cat("debugging!\n")
#> endif

# With #> else for alternative path
#> ifdef DEBUG
cat("debug!")
#> else
cat("world!")
#> endif
```

## #> ifndef

Include a code block only if the specified name is NOT defined. This is the opposite of `#> ifdef`.

**Syntax:** `#> ifndef NAME ... #> endif`

```r
#> ifndef PROD
cat("Not in production mode")
#> endif
```

## #> if

Evaluate an R expression and include the code block if the result is TRUE. The expression is evaluated using the embedded R interpreter and supports any valid R expression that returns a logical value.

**Syntax:** `#> if R_EXPRESSION ... #> endif`

```r
#> define VERSION 3

#> if VERSION > 2
cat("Using new API\n")
#> endif

#> if XXX > 1
cat("it's 1!\n")
#> endif
```

## #> elif

Chain multiple conditions together. Only the first matching branch is included.

**Syntax:** `#> ifdef|#> ifndef|#> if ... #> elif NAME ... #> else ... #> endif`

```r
#> ifdef BACKEND_POSTGRES
db <- connect_postgres()
#> elif BACKEND_SQLITE
db <- connect_sqlite()
#> else
db <- connect_default()
#> endif
```

## #> else

Provide an alternative code path for conditional blocks. Used with `#> ifdef`, `#> ifndef`, or `#> if` to specify code that should be included when the condition is false.

See the `#> ifdef` example above for usage.

## #> endif

Close a conditional compilation block. Required for all conditional directives (`#> ifdef`, `#> ifndef`, `#> if`).

## #> error

You can use `#> error` to stop compilation and print an error message.

```r
#> ifdef DEBUG
#> error "DEBUG mode is not supported"
#> endif
```

## #> assert

Validate conditions at build time. If the assertion fails, compilation stops with an error message.

**Syntax:** `#> assert (expression)`

```r
#> define VERSION 2

#> assert (VERSION > 1)
# Build continues - assertion passes

#> assert (VERSION > 5)
# Build fails: Assertion failed: (VERSION > 5)
```

## #> for

Generate repetitive code by iterating over a numeric range. The loop variable is replaced using the `..variable..` syntax within the loop body.

**Syntax:** `#> for VARIABLE in START:END ... #> endfor`

```r
#> for i in 1:5
validate_col_..i.. <- function(x) check(x$col..i..)
#> endfor
```

Expands to:

```r
validate_col_1 <- function(x) check(x$col1)
validate_col_2 <- function(x) check(x$col2)
validate_col_3 <- function(x) check(x$col3)
validate_col_4 <- function(x) check(x$col4)
validate_col_5 <- function(x) check(x$col5)
```

The range is inclusive on both ends. Multiple occurrences of `..variable..` in each line are all replaced.

## Nesting Limitation

Nested conditionals are **not supported**. Each `#> ifdef`/`#> ifndef`/`#> if` block must be independent:

```r
# NOT supported
#> ifdef A
  #> ifdef B
    code
  #> endif
#> endif

# Use combined conditions instead
#> if A && B
  code
#> endif
```

## Built-in directives

### ..FILE..

A directive that is automatically replaced with the current source file path. Useful for debugging and logging to track which file generated specific code.

```r
cat("Processing file: ..FILE..\n")
# If in srcr/analysis.R, becomes: cat("Processing file: srcr/analysis.R \n")
```

### ..LINE..

A directive that is automatically replaced with the current line number in the source file. Useful for debugging and error tracking to identify the exact location of code execution.

```r
cat("Executing line ..LINE..\n")
# If on line 42, becomes: cat("Executing line 42\n")
```

### ..OS..

A directive that is automatically populated with the current operating system. Useful for conditionally compiling code based on the current OS.

```r
cat("Running on: ..OS..\n")
# becomes: cat("Running on: Linux\n")
```

### ..DATE..

A directive that is automatically populated with the current date (compile time).

```r
cat("Running on: ..DATE..\n")
# becomes: cat("Running on: 2026-01-01\n")
```

### ..TIME..

A directive that is automatically populated with the current time (compile time).

```r
cat("Running on: ..TIME..\n")
# becomes: cat("Running on: 12:00:00\n")
```

### ..COUNTER..

A directive that is automatically replaced with a unique incrementing integer, starting at 0. Each occurrence of `..COUNTER..` in the source code is replaced with the next value in the sequence. Useful for generating unique identifiers or labels.

```r
x <- ..COUNTER..  # becomes: x <- 0
y <- ..COUNTER..  # becomes: y <- 1
z <- ..COUNTER..  # becomes: z <- 2
```
