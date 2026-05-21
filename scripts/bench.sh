#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-bench"

cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGSEXP_BUILD_EXAMPLES=OFF \
    -DGSEXP_BUILD_TESTS=OFF \
    -DGSEXP_BUILD_BENCHMARKS=ON \
    -DGSEXP_STRICT=ON \
    -DGSEXP_WARN_AS_ERROR=ON

cmake --build "${build_dir}" --target gsexp_parse_bench -j
"${build_dir}/gsexp_parse_bench"
