#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

# Load env from .envrc (direnv) so VSCode tasks don't need manual exports
direnv exec . bash -lc '
  cd vendor/edk2
  . edksetup.sh BaseTools
  build -a X64 -t CLANGDWARF -b DEBUG -p "$WORKSPACE/CarlPkg/CarlPkg.dsc"
'
