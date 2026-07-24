#!/usr/bin/env bash

set -euo pipefail

install_prefix="${PLANCK_INSTALL_PREFIX:-${HOME}/.local}"
binary="${install_prefix}/bin/planck-pet"
desktop_entry="${install_prefix}/share/applications/planck-pet.desktop"
icon="${install_prefix}/share/pixmaps/planck-pet.png"
frames="${install_prefix}/share/planck-pet"
autostart="${XDG_CONFIG_HOME:-${HOME}/.config}/autostart/planck-pet.desktop"

rm -f -- "${binary}" "${desktop_entry}" "${icon}" "${autostart}"
rm -rf -- "${frames}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${install_prefix}/share/applications"
fi

printf 'Bulldog Planck fue desinstalado. Su configuración personal se conservó.\n'
