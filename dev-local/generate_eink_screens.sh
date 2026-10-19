#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUTPUT_DIR="$SCRIPT_DIR/eink-screens"
SCREENS="main loading welcome setup saved reset"

mkdir -p "$OUTPUT_DIR"
for screen in $SCREENS; do
  "$SCRIPT_DIR/eink_emulator.sh" \
    --profile e290 \
    --screen "$screen" \
    --out "$OUTPUT_DIR/e290-$screen.svg"
done

{
  printf '%s\n' '<!doctype html>' '<html lang="en"><head><meta charset="utf-8">' \
    '<meta name="viewport" content="width=device-width, initial-scale=1">' \
    '<title>Block Clock E-ink Screens</title>' \
    '<style>body{font:16px system-ui,sans-serif;margin:2rem;background:#ecebe6;color:#222}h1{font-size:1.5rem}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:1.5rem}figure{margin:0;padding:1rem;background:#fff;border:1px solid #ccc}figcaption{font-weight:600;margin-bottom:.75rem}img{display:block;width:100%;height:auto;border:1px solid #aaa;image-rendering:pixelated}</style>' \
    '</head><body><h1>Block Clock E-ink Screens</h1><main>'
  for screen in $SCREENS; do
    printf '<figure><figcaption>E290 / %s</figcaption><img src="e290-%s.svg" alt="E290 %s screen preview"></figure>\n' \
      "$screen" "$screen" "$screen"
  done
  printf '%s\n' '</main></body></html>'
} > "$OUTPUT_DIR/index.html"

printf 'Generated gallery: %s/index.html\n' "$OUTPUT_DIR"