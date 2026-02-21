---
title: Packages
---

# Packages

Builder reads your R files from the srcr (default) directory and writes the output to the R (default) directory.

```bash
./builder -input srcr -output R
```

This allows building R packages from **multiple**, **nested** directories such as:

```bash
srcr
├── main.R
└── sub
    └── main.R
```

That will produce:

```bash
R
├── main.R
└── sub-main.R
```

Note that the file order is preserved, though it's unlikely to matter in an R package.

Also look at how you can write inlin [unit tests](/tests) for your R packages.

## Creating a Package

Use `-create` to scaffold a new R package with builder support:

```bash
./builder -create mypackage
```

This creates:

```bash
mypackage
├── DESCRIPTION
├── .Rbuildignore
├── builder.ini
├── R/
└── srcr/
```

Then navigate to the package and start building:

```bash
cd mypackage && builder
```

