#!/bin/bash
mkdir -p docs/site
cp docs/style.css docs/site/
for md in docs/pages/*.md; do
  echo "Building $md"
  name=$(basename "$md" .md)
  pandoc "$md" \
    --template=docs/template.html \
    --highlight-style=docs/lackluster.theme \
    --css=style.css \
    -o "docs/site/${name}.html"
done
