---
title: CI
---

# CI

Example GitHub Actions workflow to run builder on pull requests.

## GitHub Actions

Create `.github/workflows/builder.yml` in your repository:

```yaml
name: Builder

on:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: r-lib/actions/setup-r@v2

      - name: Install builder
        run: curl -fsSL https://builder.opifex.org/install.sh | sh

      - name: Run builder
        run: builder
```

If builder exits with an error the check fails and the
pull request is blocked.

You can pass any flags to builder, e.g.:

```yaml
      - name: Run builder
        run: builder -profile prod -deadcode
```
