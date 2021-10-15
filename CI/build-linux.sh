#!/bin/bash

##############################################################################
# Linux plugin build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build libobs and obs-frontend-api with all required dependencies
#   * Build your plugin
#   * Create debian package
#
# Parameters:
#   -h, --help                      : Print usage help
#   -q, --quiet                     : Suppress most build process output
#   -v, --verbose                   : Enable more verbose build process output
#   -p, --package                   : Create installer for plugin
#   -b, --build-dir                 : Specify alternative build directory
#                                     (default: build)
#
# Environment Variables (optional):
#   OBS_VERSION         : OBS Version
#
##############################################################################

# Halt on errors
set -eE

## SET UP ENVIRONMENT ##
_RUN_OBS_BUILD_SCRIPT=TRUE

CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
    source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
fi
PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
OBS_BUILD_DIR="${CHECKOUT_DIR}/../obs-studio"
source "${CHECKOUT_DIR}/CI/include/build_support.sh"
source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

## DEPENDENCY INSTALLATION ##
source "${CHECKOUT_DIR}/CI/linux/01_install_dependencies.sh"

## OBS LIBRARY BUILD ##
source "${CHECKOUT_DIR}/CI/linux/02_build_obs_libs.sh"

## PLUGIN BUILD ##
source "${CHECKOUT_DIR}/CI/linux/03_build_plugin.sh"

## PLUGIN PACKAGE AND NOTARIZE ##
source "${CHECKOUT_DIR}/CI/linux/04_package_plugin.sh"

## MAIN SCRIPT FUNCTIONS ##
print_usage() {
    echo -e "build_linux.sh - Build script for ${PRODUCT_NAME}\n"
    echo -e "Usage: ${0}\n" \
        "-h, --help                     : Print this help\n" \
        "-q, --quiet                    : Suppress most build process output\n" \
        "-v, --verbose                  : Enable more verbose build process output\n" \
        "-d, --skip-dependency-checks   : Skip dependency checks\n" \
        "-p, --package                  : Create installer for plugin\n" \
        "-b, --build-dir                : Specify alternative build directory (default: build)\n"

}

obs-build-main() {
    while true; do
        case "${1}" in
            -h | --help ) print_usage; exit 0 ;;
            -d | --skip-dependency-checks ) SKIP_DEP_CHECKS=TRUE; shift ;;
            -q | --quiet ) export QUIET=TRUE; shift ;;
            -v | --verbose ) export VERBOSE=TRUE; shift ;;
            -p | --package ) PACKAGE=TRUE; shift ;;
            -b | --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    ensure_dir "${CHECKOUT_DIR}"
    step "Fetching version tags..."
    git fetch origin --tags
    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(git rev-parse --short HEAD)
    GIT_TAG=$(git describe --tags --abbrev=0 2&>/dev/null || true)
    FILE_NAME="${PRODUCT_NAME}-${GIT_TAG:-${PRODUCT_VERSION}}-${GIT_HASH}-linux"

    if [ -z "${SKIP_DEP_CHECKS}" ]; then
        install_dependencies
    fi

    build_obs_libs
    build_obs_plugin

    if [ -n "${PACKAGE}" ]; then
        package_obs_plugin
    fi

    cleanup
}

obs-build-main $*
