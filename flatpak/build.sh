#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
manifest="${script_dir}/com.obsproject.Studio.Plugin.MultiRTMP.yml"
cache_dir="${XDG_CACHE_HOME:-${HOME}/.cache}/obs-multi-rtmp-flatpak"
build_dir="${cache_dir}/build"
state_dir="${cache_dir}/state"
repo_dir="${cache_dir}/repo"
bundle="${project_dir}/release/obs-multi-rtmp.flatpak"

if ! flatpak --user remotes --columns=name | grep -Fxq flathub; then
  flatpak --user remote-add flathub https://flathub.org/repo/flathub.flatpakrepo
fi

mkdir -p "${project_dir}/release"

flatpak-builder \
  --user \
  --force-clean \
  --install-deps-from=flathub \
  --state-dir="${state_dir}" \
  --repo="${repo_dir}" \
  "${build_dir}" \
  "${manifest}"

flatpak build-bundle \
  --runtime \
  "${repo_dir}" \
  "${bundle}" \
  com.obsproject.Studio.Plugin.MultiRTMP \
  stable

printf 'Created %s\n' "${bundle}"
