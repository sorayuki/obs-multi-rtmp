---
name: upgrade-deps
description: Follow the upstream plugin template and OBS version for this plugin.
---

# IMPORTANT

Use cmd or bash instead of powershell when run git or curl commands. Run in powershell may failed.

# Following the steps to upgrade

## Update the plugin template

The official git repository of plugin template is ```https://github.com/obsproject/obs-plugintemplate.git```. My repository merges its commits. You can pull its latest commits and merge them into master. Follow these instructions when resolving merge conflicts:

1. Always use my README.md
2. I have changed the github action workflow files, so as the build scripts (.sh and .ps1 files). For those parts that I commented and the upstream has changed, uncomment them and merge the upstream changes, then comment them again. For those parts that I commented and the upstream has not changed, just keep them commented.

Commit the merge after resolving the conflicts.

## Update OBS version

1. The official OBS repository is ```https://github.com/obsproject/obs-studio.git```, search for tags to find the latest release version.
2. The release page is like ```https://github.com/obsproject/obs-studio/releases/tag/32.1.0```. Replace ```32.1.0``` with the latest version you found. Update dependency metadata under ```CMakePresets.json``` (```vendor.obsproject.com/obs-studio.dependencies```) with the new version and hash.
3. Download the ```zip``` format of the source code package.

## Update the OBS deps

Extract the downloaded OBS source package and read ```CMakePresets.json``` in repo root.

Use ```vendor.obsproject.com/obs-studio.dependencies``` as source of truth.

Update this repository's ```buildspec.json``` entries (Qt, obs-deps, and related hashes).

## Try build this project

This project is build with CMake. Use the ```windows-x64``` preset when run in Windows.

## Troubleshooting (important)

If OBS version/deps in `buildspec.json` were updated but build still uses old OBS source:

1. Do **fresh configure** before build (plain build may reuse old cache and skip dependency setup).
2. Prefer:
	- `cmake --preset windows-x64 --fresh`
	- `cmake --build --preset windows-x64 --config RelWithDebInfo`
3. Check `.deps/` cache:
	- `.deps/.dependency_obs-studio_x64.sha256`
	- `.deps/obs-studio-<version>/`
	- `.deps/<version>.zip`
4. If still stuck on old OBS version, remove old OBS entries in `.deps/` and rerun fresh configure.
