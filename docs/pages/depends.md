---
title: Depends
---

# Depends

Check that dev dependencies are installed before building. This is useful for ensuring development tools like testing frameworks, documentation generators, or linting packages are available.

## CLI Usage

```bash
builder -depends testthat devtools roxygen2
```

With version requirements:

```bash
builder -depends testthat(>=3.0.0) devtools(>=2.4.0) roxygen2
```

## Config Usage

```ini
depends: testthat devtools roxygen2
```

With version requirements:

```ini
depends: testthat(>=3.0.0) devtools(>=2.4.0) roxygen2
```

## Behavior

- Checks each package with `requireNamespace()`
- If any package is missing, build fails with an error
- All missing packages are reported before exiting
- Optionally specify version constraint with `package(<op>major.minor.patch)` format
- Supported operators: `>=`, `<=`, `==`, `>`, `<`
- If installed version doesn't satisfy the constraint, build fails with an error

## Example

```bash
# Check dev dependencies before building
builder -depends testthat covr lintr

# With version requirements
builder -depends testthat(>=3.0.0) covr(>=3.5.0) lintr

# Combined with other options
builder -input srcr/ -depends testthat(>=3.0.0) devtools
```

This is intended for **development dependencies** - packages needed during the build process but not necessarily at runtime.
