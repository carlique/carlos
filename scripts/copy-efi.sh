#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

direnv exec . bash -lc '
  set -euo pipefail
  EFI="$(find "$WORKSPACE/Build" -name "CarlOs.efi" | head -n 1)"
  if [ -z "${EFI:-}" ] || [ ! -f "$EFI" ]; then
    echo "ERROR: CarlOs.efi not found. Run build first."
    exit 1
  fi

  mkdir -p "$WORKSPACE/qemu/esp/EFI/BOOT"
  cp "$EFI" "$WORKSPACE/qemu/esp/EFI/BOOT/BOOTX64.EFI"
  echo "Copied: $EFI -> $WORKSPACE/qemu/esp/EFI/BOOT/BOOTX64.EFI"
'
