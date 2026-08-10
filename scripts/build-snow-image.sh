#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
preset=${1:-windows-msvc-debug}
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$repo_root/scripts/build-snow-image.ps1" -Preset "$preset" -SkipBootstrap
