#!/bin/bash
mkdir -p docs/site

cp docs/style.css docs/site/

# Page order for prev/next navigation (matches nav menu)
pages=(
  index
  installation
  get-started
  config
  architecture
  packages
  directives
  macros
  include
  headers
  import
  fstrings
  const
  deconstruct
  preflight
  depends
  dry-run
  plugins
  tests
  prepend-append
  deadcode
  sourcemap
  watch
  syntax-highlighting
  bundle
  ci
)

# Build each page with TOC and prev/next navigation
for i in "${!pages[@]}"; do
  name="${pages[$i]}"
  md="docs/pages/${name}.md"
  
  if [[ ! -f "$md" ]]; then
    echo "Skipping $md (not found)"
    continue
  fi
  
  echo "Building $md"
  
  # Calculate prev/next
  prev_flag=""
  next_flag=""
  
  if [[ $i -gt 0 ]]; then
    prev_flag="-V prev=${pages[$((i-1))]}"
  fi
  
  if [[ $i -lt $((${#pages[@]} - 1)) ]]; then
    next_flag="-V next=${pages[$((i+1))]}"
  fi
  
  pandoc "$md" \
    --template=docs/template.html \
    --highlight-style=docs/lackluster.theme \
    --toc \
    --toc-depth=3 \
    --css=style.css \
    $prev_flag \
    $next_flag \
    -o "docs/site/${name}.html"
done
