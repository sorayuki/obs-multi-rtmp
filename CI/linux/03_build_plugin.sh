#!/bin/bash

##############################################################################
# Linux libobs plugin build function
##############################################################################
#
# This script file can be included in build scripts for Linux or run directly
#
##############################################################################

# Halt on errors
set -eE

build_obs_plugin() {
    status "Build plugin ${PRODUCT_NAME}"
    trap "caught_error 'builds_obs_plugin'" ERR

    ensure_dir "${CHECKOUT_DIR}"

    step "Configuring OBS plugin build system"
    check_ccache

    cmake -S . -B ${BUILD_DIR} -G Ninja ${CMAKE_CCACHE_OPTIONS} ${QUIET:+-Wno-deprecated -Wno-dev --log-level=ERROR}

    step "Building OBS plugin"
    cmake --build ${BUILD_DIR}
}

build-plugin-standalone() {
    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
        source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
    fi
    PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

    build_obs_plugin
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

build-plugin-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        build-plugin-standalone
    fi
}

build-plugin-main $*
