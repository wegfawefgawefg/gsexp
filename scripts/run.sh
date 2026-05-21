#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

"${repo_root}/scripts/build.sh"
"${repo_root}/build/gsexp_demo"
