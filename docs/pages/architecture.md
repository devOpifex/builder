---
title: Architecture
---

# Architecture

Builder assumes you're building a package
and simulates R's lazy loading behavior by performing a two-pass build.
Understanding this architecture helps you write more predictable preprocessing directives.

## Two-Pass System

<div class="desktop-only">

```
┌─────────────────────────────────────────────────────────────┐
│                       FIRST PASS                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  Collect    │  │    Run      │  │     Validate        │  │
│  │  #> define &│─▶│ #> preflight│─▶│    #> assert        │  │
│  │  #> macro   │  │  blocks     │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                                              │              │
│                                              ▼              │
│                              Plugin preprocess hook         │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      SECOND PASS                            │
│                                                             │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  #> for / #> endfor expansion (before line processing) │ │
│  └────────────────────────────────────────────────────────┘ │
│                             │                               │
│                             ▼                               │
│  For each line:                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐   │
│  │ F-string │─▶│#> include│─▶│  Macro   │─▶│Deconstruct │   │
│  │ replace  │  │ replace  │  │ replace  │  │  replace   │   │
│  └──────────┘  └──────────┘  └──────────┘  └────────────┘   │
│                                                   │         │
│       ┌───────────────────────────────────────────┘         │
│       ▼                                                     │
│  ┌──────────┐  ┌──────────┐  ┌────────────┐  ┌───────────┐  │
│  │  Const   │─▶│ #> test  │─▶│ Directive  │─▶│Conditional│  │
│  │ replace  │  │ collect  │  │   filter   │  │   check   │  │
│  └──────────┘  └──────────┘  └────────────┘  └───────────┘  │
│                                                   │         │
│                                                   ▼         │
│                              Plugin postprocess hook        │
└─────────────────────────────────────────────────────────────┘
```

</div>

## First Pass

The first pass scans all source files to collect definitions and run preflight checks. No output is written during this pass.

### What happens:

1. **Macro Collection** - All `#> define` and `#> macro` directives are parsed and stored
2. **Preflight Execution** - `#> preflight` / `#> endflight` blocks are evaluated
3. **Assert Validation** - `#> assert` directives are checked; build fails if any assertion is false
4. **Import Processing** - `#> import` directives are noted (for namespace prefixing)
5. **Plugin Hook** - The `preprocess` plugin hook is called on each file's content

## Second Pass

The second pass processes each line through a series of replacements and writes the final output.

### Loop Expansion

Before line-by-line processing, `#> for` / `#> endfor` blocks are collected and expanded into repeated code. The loop variable uses `..variable..` syntax for substitution.

### Replacement Order

Each line is processed through these transformations in order:

| Order | Transformation | Example |
|-------|---------------|---------|
| 1 | F-strings | `..FMT("{x}")` → `sprintf('%s', x)` |
| 2 | #> include: | `#> include:READ file.sql q` → `q <- c(...)` |
| 3 | Macro expansion | `MY_MACRO(x)` → expanded code |
| 4 | Deconstruction | `.[a, b] <- fn()` → indexed assignments |
| 5 | Constants | `x -< 1` → `x <- 1;lockBinding(...)` |

After replacements, the line goes through:

- **Conditional compilation** - `#> ifdef`, `#> ifndef`, `#> if`, `#> elif`, `#> else`, `#> endif`
- **Test collection** - `#> test` blocks are extracted
- **Error checking** - `#> error` directives halt compilation

## Why Order Matters

The replacement order has important implications:

### F-strings First

F-strings are processed before macros, so you can use macro-defined values inside f-strings:

```r
#> define VERSION 2.0.0

print(..FMT("Version: {VERSION}"))  # VERSION expanded after f-string processing
```

### Constants Last

The constant operator `-<` is processed after all other transformations, ensuring the final value is locked:

```r
#> define DEFAULT 42
x -< DEFAULT  # First: DEFAULT → 42, Then: x <- 42;lockBinding("x", environment())
```

## Built-in Definitions

These definitions are automatically available and updated during processing:

| Definition | Description | Updated |
|------------|-------------|---------|
| `..FILE..` | Current source file path | Per file |
| `..LINE..` | Current line number | Per line |
| `..OS..` | Operating system name | Once at start |
| `..DATE..` | Build date (YYYY-MM-DD) | Once at start |
| `..TIME..` | Build time (HH:MM:SS) | Once at start |
| `..COUNTER..` | Auto-incrementing integer | Per occurrence |
