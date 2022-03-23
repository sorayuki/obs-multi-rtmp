# OBS Plugin Template

## Introduction

This plugin is meant to make it easy to quickstart development of new OBS plugins. It includes:

- The CMake project file
- Boilerplate plugin source code
- GitHub Actions workflows and repository actions
- Build scripts for Windows, macOS, and Linux

## Configuring

Open `buildspec.json` and change the name and version of the plugin accordingly. This is also where the obs-studio version as well as the pre-built dependencies for Windows and macOS are defined. Use a release version (with associated checksums) from a recent [obs-deps release](https://github.com/obsproject/obs-deps/releases).

Next, open `CMakeLists.txt` and edit the following lines at the beginning:

```cmake
project(obs-plugintemplate VERSION 1.0.0)

set(PLUGIN_AUTHOR "Your Name Here")

set(LINUX_MAINTAINER_EMAIL "me@contoso.com")
```

The build scripts (contained in the `.github/scripts` directory) will update the `project` line automatically based on values from the `buildspec.json` file. If the scripts are not used, these changes need to be done manually.

## GitHub Actions & CI

The scripts contained in `github/scripts` can be used to build and package the plugin and take care of setting up obs-studio as well as its own dependencies. A default workflow for GitHub Actions is also provided and will use these scripts.

### Retrieving build artifacts

Each build produces installers and packages that you can use for testing and releases. These artifacts can be found on the action result page via the "Actions" tab in your GitHub repository.

#### Building a Release

Simply create and push a tag and GitHub Actions will run the pipeline in Release Mode. This mode uses the tag as its version number instead of the git ref in normal mode.

### Signing and Notarizing on macOS

On macOS, Release Mode builds can be signed and sent to Apple for notarization if the necessary codesigning credentials are added as secrets to your repository. **You'll need a paid Apple Developer Account for this.**

- On your Apple Developer dashboard, go to "Certificates, IDs & Profiles" and create two signing certificates:
    - One of the "Developer ID Application" type. It will be used to sign the plugin's binaries
    - One of the "Developer ID Installer" type. It will be used to sign the plugin's installer
- Using the Keychain app on macOS, export these two certificates and keys into a .p12 file **protected with a strong password**
- Encode the .p12 file into its base64 representation by running `base64 YOUR_P12_FILE`
- Add the following secrets in your Github repository settings:
    - `MACOS_SIGNING_APPLICATION_IDENTITY`: Name of the "Developer ID Application" signing certificate generated earlier
    - `MACOS_SIGNING_INSTALLER_IDENTITY`: Name of "Developer ID Installer" signing certificate generated earlier
    - `MACOS_SIGNING_CERT`: Base64-encoded string generated above
    - `MACOS_SIGNING_CERT_PASSWORD`: Password used to generate the .p12 certificate
    - `MACOS_NOTARIZATION_USERNAME`: Your Apple Developer account's username
    - `MACOS_NOTARIZATION_PASSWORD`: Your Apple Developer account's password (use a generated "app password" for this)
