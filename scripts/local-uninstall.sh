#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
"$project_dir/scripts/local-disable.sh"
printf '%s\n' 'For a packaged installation, uninstall through your package manager.' \
  'For a source installation, consult build/install_manifest.txt for installed files.'
