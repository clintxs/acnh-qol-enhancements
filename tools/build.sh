#!/bin/sh
set -eu
# Clean when switching modes: make does not track compiler flag changes.
case "${1:-}" in
  "") trace=0; hurry=0 ;;
  --verbose-dialogue) trace=1; hurry=0 ;;
  --hurry-diagnostic) trace=0; hurry=1 ;;
  *) echo "Usage: $0 [--verbose-dialogue|--hurry-diagnostic]" >&2; exit 2 ;;
esac
if [ "$#" -gt 1 ]; then
  echo "Usage: $0 [--verbose-dialogue|--hurry-diagnostic]" >&2
  exit 2
fi
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
docker run --rm --network none \
  --mount "type=bind,source=$project_dir,target=/project" \
  --workdir /project \
  devkitpro/devkita64:20260219@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
  sh -c 'make clean && make -j4 CAS_VERBOSE_DIALOGUE_TRACE="$1" CAS_VERBOSE_HURRY_TRACE="$2"' sh "$trace" "$hurry"
