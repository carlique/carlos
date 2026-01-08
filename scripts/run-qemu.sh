#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

# No need for direnv here, but it's fine either way.
qemu-system-x86_64 \
  -machine q35 -m 1024 \
  -drive if=pflash,format=raw,readonly=on,file=qemu/firmware/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=qemu/firmware/OVMF_VARS.fd \
  -drive format=raw,file=fat:rw:qemu/esp \
  -serial mon:stdio
