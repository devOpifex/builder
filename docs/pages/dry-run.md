---
title: Dry Run Mode
---

# Dry Run Mode

Builder can perform a dry run to preview what changes will be made without modifying your actual output directory. This is useful for validating builds, previewing macro expansions, or integrating into CI pipelines.

## Usage

```bash
builder -dry-run
builder -dry-run -diff
```

## How It Works

When `-dry-run` is enabled:

1. Builder creates a temporary directory (`/tmp/dryrun-XXXXXX`)
2. Performs the full build process, writing output to the temporary directory
3. Tests are not written in dry-run mode
4. If `-diff` is specified, shows differences between source and processed files
5. Cleans up the temporary directory automatically when finished

## The -diff Flag

The `-diff` flag is a companion to `-dry-run` that shows line-by-line differences between your source files and the processed output. This lets you see exactly what transformations builder will apply.

```bash
builder -dry-run -diff
```

The `-diff` flag requires `-dry-run` to be set. Using `-diff` alone will produce an error:

```
[ERROR] -diff requires -dry-run
```

## Example

```bash
$ builder -dry-run -diff
[INFO] Cleaning: /tmp/dryrun-abc123/ and testthat/
[INFO] Copying srcr/main.R to /tmp/dryrun-abc123/main.R
[INFO] Running diff
3c3
< cat("Version:", VERSION)
---
> cat("Version:", 2)
[SUCCESS] All built!
```

In this example, the diff shows that `VERSION` was replaced with its defined value `2`.

## Options

| Flag | Description |
|------|-------------|
| `-dry-run` | Build to temporary directory without modifying output |
| `-diff` | Show differences between source and processed output (requires `-dry-run`) |

## Use Cases

- **CI validation**: Verify builds complete successfully without side effects
- **Preview macro expansions**: See how macros will expand before committing
- **Review directive processing**: Check conditional compilation results
- **Safe experimentation**: Test build configurations without risk
