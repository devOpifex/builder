---
title: Configuration File
---

# Configuration File

Instead of passing command-line arguments, you can use a `builder.ini` file in your project root. Command-line arguments override config file values.

## Usage

Create a `builder.ini` file, you can also call `builder -init` to create one.
```ini
input: srcr/
output: R/
```

Then run builder without arguments:

```bash
builder
```

## Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `input` | string | `srcr/` | Input directory |
| `output` | string | `R/` | Output directory |
| `prepend` | string | - | File to prepend to outputs |
| `append` | string | - | File to append to outputs |
| `bundle` | string | - | Bundle all outputs into a single file |
| `deadcode` | bool | `false` | Enable dead code detection |
| `sourcemap` | bool | `false` | Enable source maps |
| `clean` | bool | `true` | Clean output before build |
| `watch` | bool | `false` | Enable watch mode |
| `plugin` | list | - | Space-separated plugins |
| `import` | list | - | Space-separated imports |
| `depends` | list | - | Space-separated dev dependencies to check |
| `reader` | string | - | Define custom file type reader (format: `type function`) |
| `strip` | bool | `false` | Strip comments from output (preserves `#'` special comments) |

## Full Example

```ini
# Builder configuration
input: srcr/
output: R/

# File manipulation
prepend: inst/license.txt
append: inst/footer.txt
bundle: R/bundled.R

# Features
deadcode: true
sourcemap: false
clean: true
watch: false
strip: false

# Plugins and imports (space-separated)
plugin: mypkg::minify mypkg::lint
import: inst/types.rh inst/utils.rh

# Dev dependencies (space-separated R packages)
depends: testthat devtools roxygen2

# Custom file readers (one per line)
reader: tsv \(x) read.delim(x, sep="\t", header=FALSE)
reader: myformat mypackage::read_myformat
```

## Profiles

You can define named profiles in your config file using `[profile: <name>]` headers. This lets you maintain multiple configurations (e.g., development vs. production) in a single file.

### Common Settings

Any settings placed **before** the first `[profile: ...]` header are common and shared across all profiles. Profile-specific settings override these shared defaults.

```ini
# Common settings (shared by all profiles)
input: srcr/
output: R/
clean: true
depends: dplyr

[profile: dev]
sourcemap: true

[profile: prod]
sourcemap: false
strip: true
```

In this example, both `dev` and `prod` inherit `input: srcr/`, `clean: true`, and `depends: dplyr`. The `dev` profile enables source maps, while `prod` disables them and strips comments.

### Selecting a Profile

Use the `-profile` flag to select which profile to use:

```bash
builder -profile dev
builder -profile prod
```

When your config file contains profiles, the `-profile` flag is **required**. Running `builder` without it will produce an error.

### Config Without Profiles

If your config file has no `[profile: ...]` headers, it works exactly as before — no `-profile` flag is needed.

## Overriding with CLI

Config values are defaults. CLI arguments take precedence:

```bash
# Uses config, but overrides input
builder -input other/

# Uses config, but disables cleaning
builder -noclean
```

## Comments

Lines starting with `#` are ignored.
