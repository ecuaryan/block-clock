#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$SCRIPT_DIR/.build"
CXX=${CXX:-clang++}

PROFILE=e290
ARGS=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --profile)
      [ "$#" -ge 2 ] || { echo "Missing value for --profile" >&2; exit 2; }
      PROFILE=$2
      shift 2
      ;;
    *)
      ARGS="$ARGS '$1'"
      shift
      ;;
  esac
done

case "$PROFILE" in
  e290|E290) PROFILE=e290; PROFILE_FLAG= ;;
  e213|E213) PROFILE=e213; PROFILE_FLAG=-DPROFILE_E213=1 ;;
  *) echo "Profile must be e290 or e213." >&2; exit 2 ;;
esac

mkdir -p "$BUILD_DIR"
OUTPUT="$BUILD_DIR/eink_emulator-$PROFILE"
"$CXX" -std=c++17 -Wall -Wextra -Wno-unused-function $PROFILE_FLAG \
  -o "$OUTPUT" "$SCRIPT_DIR/eink_emulator.cpp"

eval "\"\$OUTPUT\" --profile \"\$PROFILE\" $ARGS"