#!/bin/bash

##############################################################################
# macOS plugin build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build libobs and obs-frontend-api with all required dependencies
#   * Build your plugin
#   * Create macOS module bundle
#   * Create macOS plugin installer package
#   * Notarize macOS plugin installer package
#
# Parameters:
#   -h, --help                      : Print usage help
#   -q, --quiet                     : Suppress most build process output
#   -v, --verbose                   : Enable more verbose build process output
#   -d, --skip-dependency-checks    : Skip dependency checks
#   -p, --package                   : Create installer for plugin
#   -c, --codesign                  : Codesign plugin and installer
#   -n, --notarize                  : Notarize plugin installer
#                                     (implies --codesign)
#   -b, --build-dir                 : Specify alternative build directory
#                                     (default: build)
#
# Environment Variables (optional):
#   MACOS_DEPS_VERSION  : Pre-compiled macOS dependencies version
#   QT_VERSION          : Pre-compiled Qt version
#   OBS_VERSION         : OBS version
#
##############################################################################

# Halt on errors
set -eE

## SET UP ENVIRONMENT ##
_RUN_OBS_BUILD_SCRIPT=TRUE

CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
    source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
fi
PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
OBS_BUILD_DIR="${CHECKOUT_DIR}/../obs-studio"
source "${CHECKOUT_DIR}/CI/include/build_support.sh"
source "${CHECKOUT_DIR}/CI/include/build_support_macos.sh"

## DEPENDENCY INSTALLATION ##
source "${CHECKOUT_DIR}/CI/macos/01_install_dependencies.sh"

## OBS LIBRARY BUILD ##
source "${CHECKOUT_DIR}/CI/macos/02_build_obs_libs.sh"

## PLUGIN BUILD ##
source "${CHECKOUT_DIR}/CI/macos/03_build_plugin.sh"

## PLUGIN PACKAGE AND NOTARIZE ##
source "${CHECKOUT_DIR}/CI/macos/04_package_plugin.sh"

## MAIN SCRIPT FUNCTIONS ##
print_usage() {
    echo -e "build_macos.sh - Build script for ${PRODUCT_NAME}\n"
    echo -e "Usage: ${0}\n" \
        "-h, --help                     : Print this help\n" \
        "-q, --quiet                    : Suppress most build process output\n" \
        "-v, --verbose                  : Enable more verbose build process output\n" \
        "-a, --architecture             : Specify build architecture (default: universal, alternative: x86_64, arm64)\n" \
        "-d, --skip-dependency-checks   : Skip dependency checks\n" \
        "-p, --package                  : Create installer for plugin\n" \
        "-c, --codesign                 : Codesign plugin and installer\n" \
        "-n, --notarize                 : Notarize plugin installer (implies --codesign)\n" \
        "-b, --build-dir                : Specify alternative build directory (default: build)\n"
}

obs-build-main() {
    while true; do
        case "${1}" in
            -h | --help ) print_usage; exit 0 ;;
            -q | --quiet ) export QUIET=TRUE; shift ;;
            -v | --verbose ) export VERBOSE=TRUE; shift ;;
            -a | --architecture ) ARCH="${2}"; shift 2 ;;
            -d | --skip-dependency-checks ) SKIP_DEP_CHECKS=TRUE; shift ;;
            -p | --package ) PACKAGE=TRUE; shift ;;
            -c | --codesign ) CODESIGN=TRUE; shift ;;
            -n | --notarize ) NOTARIZE=TRUE; PACKAGE=TRUE CODESIGN=TRUE; shift ;;
            -b | --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    ensure_dir "${CHECKOUT_DIR}"
    check_macos_version
    check_archs
    step "Fetching version tags..."
    /usr/bin/git fetch origin --tags
    GIT_BRANCH=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(/usr/bin/git rev-parse --short HEAD)
    GIT_TAG=$(/usr/bin/git describe --tags --abbrev=0 2&>/dev/null || true)

    if [ "${ARCH}" = "arm64" ]; then
        FILE_NAME="${PRODUCT_NAME}-${GIT_TAG:-${PRODUCT_VERSION}}-${GIT_HASH}-macOS-Apple.pkg"
    elif [ "${ARCH}" = "x86_64" ]; then
        FILE_NAME="${PRODUCT_NAME}-${GIT_TAG:-${PRODUCT_VERSION}}-${GIT_HASH}-macOS-Intel.pkg"
    else
        FILE_NAME="${PRODUCT_NAME}-${GIT_TAG:-${PRODUCT_VERSION}}-${GIT_HASH}-macOS-Universal.pkg"
    fi

    if [ -z "${SKIP_DEP_CHECKS}" ]; then
        install_dependencies
    fi

    build_obs_libs
    build_obs_plugin

    if [ -n "${PACKAGE}" ]; then
        package_obs_plugin
    fi

    if [ -n "${NOTARIZE}" ]; then
        notarize_obs_plugin
    fi

    cleanup
}

obs-build-main $*
