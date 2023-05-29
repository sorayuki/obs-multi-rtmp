# OBS Plugin Template

## Introduction

The plugin template is meant to be used as a starting point for OBS Studio plugin development. It includes:

* Boilerplate plugin source code
* A CMake project file
* GitHub Actions workflows and repository actions

## Set Up

The plugin project is set up using the included `buildspec.json` file. The following fields should be customized for an actual plugin:

* `name`: The plugin name
* `version`: The plugin version
* `author`: Actual name or nickname of the plugin's author
* `website`: URL of a website associated with the plugin
* `email`: Contact email address associated with the plugin
* `uuids`
    * `macosPackage`: Unique (**!**) identifier for the macOS plugin package
    * `macosInstaller`: Unique (**!**) identifier for the macOS plugin installer
    * `windowsApp`: Unique (**!**) identifier for the Windows plugin installer

These values are read and processed automatically by the CMake build scripts, so no further adjustments in other files are needed.

### Platform Configuration

Platform-specific settings are set up in the `platformConfig` section of the buildspec file:

* `bundleId`: macOS bundle identifier for the plugin. Should be unique and follow reverse domain name notation.

### Set Up Build Dependencies

Just like OBS Studio itself, plugins need to be built using dependencies available either via the `obs-deps` repository (Windows and macOS) or via a distribution's package system (Linux).

#### Choose An OBS Studio Version

By default the plugin template specifies the most current official OBS Studio version in the `buildspec.json` file, which makes most sense for plugins at the start of development. As far as updating the targeted OBS Studio version is concerned, a few things need to be considered:

* Plugins targeting _older_ versions of OBS Studio should _generally_ also work in newer versions, with the exception of breaking changes to specific APIs which would also be explicitly called out in release notes
* Plugins targeting the _latest_ version of OBS Studio might not work in older versions because the internal data structures used by `libobs` might not be compatible
* Users are encouraged to always update to the most recent version of OBS Studio available within a reasonable time after release - plugin authors have to choose for themselves if they'd rather keep up with OBS Studio releases or stay with an older version as their baseline (which might of course preclude the plugin from using functionality introduced in a newer version)

On Linux, the version used for development might be decided by the specific version available via a distribution's package management system, so OBS Studio compatibility for plugins might be determined by those versions instead.

#### Windows and macOS

Windows and macOS dependency downloads are configured in the `buildspec.json` file:

* `dependencies`:
    * `obs-studio`: Version of OBS Studio to build plugin with (needed for `libobs` and `obs-frontend-api`)
    * `prebuilt`: Prebuilt OBS Studio dependencies
    * `qt6`: Prebuilt version of Qt6 as used by OBS Studio
* `tools`: Contains additional build tools used by CI

The values should be kept in sync with OBS Studio releases and the `buildspec.json` file in use by the main project to ensure that the plugin is developed and built in sync with its target environment.

To update a dependency, change the `version` and associated `hashes` entries to match the new version. The used hash algorithm is `sha256`.

#### Linux

Linux dependencies need to be resolved using the package management tools appropriate for the local distribution. As an example, building on Ubuntu requires the following packages to be installed:

* Build System Dependencies:
    * `cmake`
    * `ninja-build`
    * `pkg-config`
* Build Dependencies:
    * `build-essential`
    * `libobs-dev`
* Qt6 Dependencies:
    * `qt6-base-dev`
    * `libqt6svg6-dev`
    * `qt6-base-private-dev`

## Build System Configuration

To create a build configuration, `cmake` needs to be installed on the system. The plugin template supports CMake presets using the `CMakePresets.json` file and ships with default presets:

* `macos`
    * Universal architecture (supports Intel-based CPUs as Apple Silicon)
    * Defaults to Qt version `6`
    * Defaults to macOS deployment target `11.0`
* `macos-ci`
    * Inherits from `macos`
    * Enables compile warnings as error
* `windows-x64`
    * Windows 64-bit architecture
    * Defaults to Qt version `6`
    * Defaults to Visual Studio 17 2022
    * Defaults to Windows SDK version `10.0.18363.657`
* `windows-ci-x64`
    * Inherits from `windows-x64`
    * Enables compile warnings as error
* `linux-x86_64`
    * Linux x86_64 architecture
    * Defaults to Qt version `6`
    * Defaults to Ninja as build tool
    * Defaults to `RelWithDebInfo` build configuration
* `linux-ci-x86_64`
    * Inherits from `linux-x86_64`
    * Enables compile warnings as error
* `linux-aarch64`
    * Provided as an experimental preview feature
    * Linux aarch64 (ARM64) architecture
    * Defaults to Qt version `6`
    * Defaults to Ninja as build tool
    * Defaults to `RelWithDebInfo` build configuration
* `linux-ci-aarch64`
    * Inherits from `linux-aarch64`
    * Enables compile warnings as error

Presets can be either specified on the command line (`cmake --preset <PRESET>`) or via the associated select field in the CMake Windows GUI. Only presets appropriate for the current build host are available for selection.

Additional build system options are available to developers:

* `ENABLE_CCACHE`: Enables support for compilation speed-ups via ccache (enabled by default on macOS and Linux)
* `ENABLE_FRONTEND_API`: Adds OBS Frontend API support for interactions with OBS Studio frontend functionality (disabled by default)
* `ENABLE_QT`: Adds Qt6 support for custom user interface elements (disabled by default)
* `CODESIGN_IDENTITY`: Name of the Apple Developer certificate that should be used for code signing
* `CODESIGN_TEAM`: Apple Developer team ID that should be used for code signing

## GitHub Actions & CI

Default GitHub Actions workflows are available for the following repository actions:

* `push`: Run for commits or tags pushed to `master` or `main` branches.
* `pr-pull`: Run when a Pull Request has been pushed or synchronized.
* `dispatch`: Run when triggered by the workflow dispatch in GitHub's user interface.
* `build-project`: Builds the actual project and is triggered by other workflows.
* `check-format`: Checks CMake and plugin source code formatting and is triggered by other workflows.

The workflows make use of GitHub repository actions (contained in `.github/actions`) and build scripts (contained in `.github/scripts`) which are not needed for local development, but might need to be adjusted if additional/different steps are required to build the plugin.

### Retrieving build artifacts

Successful builds on GitHub Actions will produce build artifacts that can be downloaded for testing. These artifacts are commonly simple archives and will not contain package installers or installation programs.

### Building a Release

To create a release, an appropriately named tag needs to be pushed to the `main`/`master` branch using semantic versioning (e.g., `12.3.4`, `23.4.5-beta2`). A draft release will be created on the associated repository with generated installer packages or installation programs attached as release artifacts.

## Signing and Notarizing on macOS

Plugins released for macOS should be codesigned and notarized with a valid Apple Developer ID for best user experience. To set this up, the private and personal key of a **paid Apple Developer ID** need to be downloaded from the Apple Developer portal:

* On your Apple Developer dashboard, go to "Certificates, IDs & Profiles" and create two signing certificates:
    * One of the "Developer ID Application" type. It will be used to sign the plugin's binaries
    * One of the "Developer ID Installer" type. It will be used to sign the plugin's installer

The developer certificate will usually carry a name similar in form to

`Developer ID Application: <FIRSTNAME> <LASTNAME> (<LETTERS_AND_NUMBERS>)`

This entire string should be specified as `CODESIGN_IDENTITY`, the `LETTERS_AND_NUMBERS` part as `CODESIGN_TEAM` to CMake to set up codesigning properly.

### GitHub Actions Set Up

To use code signing on GitHub Actions, the certificate and associated information need to be set up as _repository secrets_ in the GitHub repository's settings.

* First, the locally stored developer certificate needs to be exported from the macOS keychain:
    * Using the Keychain app on macOS, export these your certificates (Application and Installer) public _and_ private keys into a single .p12 file **protected with a strong password**
    * Encode the .p12 file into its base64 representation by running `base64 <NAME_OF_YOUR_P12_FILE>`
* Next, the certificate data and the password used to export it need to be set up as repository secrets:
    * `MACOS_SIGNING_APPLICATION_IDENTITY`: Name of the "Developer ID Application" signing certificate
    * `MACOS_SIGNING_INSTALLER_IDENTITY`: Name of "Developer ID Installer" signing certificate
    * `MACOS_SIGNING_CERT`: The base64 encoded `.p12` file
    * `MACOS_SIGNING_CERT_PASSWORD`: Password used to generate the .p12 certificate
* To also enable notarization on GitHub Action runners, the following repository secrets are required:
    * `MACOS_NOTARIZATION_USERNAME`: Your Apple Developer account's _Apple ID_
    * `MACOS_NOTARIZATION_PASSWORD`: Your Apple Developer account's _generated app password_
