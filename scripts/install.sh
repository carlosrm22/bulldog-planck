#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build"
install_prefix="${PLANCK_INSTALL_PREFIX:-${HOME}/.local}"

cmake \
    -S "${project_dir}" \
    -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_prefix}"

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${install_prefix}/share/applications"
fi

printf '\nBulldog Planck quedó instalado en %s.\n' "${install_prefix}"
printf 'Búscalo como “Planck” en el lanzador o ejecuta: planck-pet\n'
