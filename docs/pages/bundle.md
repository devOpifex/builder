---
title: Bundle
---

# Bundle

Bundle all output files into a single file instead of writing individual files.

## Usage

```bash
builder -input srcr -output R -bundle output.R
```

- `-bundle <file>` - All processed content is concatenated into a single file

## Configuration

```ini
bundle: output.R
```

## Example: Single-file Package

Bundle all R files for distribution as a single script:

```bash
builder -input srcr -output R -bundle R/package.R
```

All processed files from `srcr/` are combined into `R/package.R`. Individual files are not written to the output directory.

## With Prepend/Append

When using `-bundle` with `-prepend` or `-append`, the prepend/append content is applied to each file before bundling.
