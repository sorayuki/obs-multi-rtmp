#!/bin/bash

##############################################################################
# macOS libobs plugin build function
##############################################################################
#
# This script file can be included in build scripts for macOS or run directly
#
##############################################################################

# Halt on errors
set -eE

build_obs_plugin() {
    status "Build plugin ${PRODUCT_NAME}"
    trap "caught_error 'builds_obs_plugin'" ERR

    if [ "${CODESIGN}" ]; then
        read_codesign_ident
    fi

    ensure_dir "${CHECKOUT_DIR}"
    step "Configuring OBS plugin build system"
    check_ccache

    cmake -S . -B ${BUILD_DIR} -G Ninja ${CMAKE_CCACHE_OPTIONS} \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-${CI_MACOSX_DEPLOYMENT_TARGET}} \
        -DCMAKE_OSX_ARCHITECTURES="${CMAKE_ARCHS}" \
        -DOBS_CODESIGN_LINKER=${CODESIGN_LINKER:-OFF} \
        -DCMAKE_BUILD_TYPE=${BUILD_CONFIG} \
        -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
        -DCMAKE_PREFIX_PATH="${DEPS_BUILD_DIR}/obs-deps" \
        ${QUIET:+-Wno-deprecated -Wno-dev --log-level=ERROR}

    step "Building OBS plugin"
    cmake --build ${BUILD_DIR}

}

build-plugin-standalone() {
    CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
    if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
        source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
    fi
    PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_macos.sh"

    check_macos_version
    check_archs

    build_obs_plugin
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-a, --architecture             : Specify build architecture (default: x86_64, alternative: arm64)\n" \
            "-c, --codesign                 : Codesign OBS and all libraries (default: ad-hoc only)\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

build-plugin-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -c | --codesign ) CODESIGN=TRUE; shift ;;
                -a | --architecture ) ARCH="${2}"; shift 2 ;;
                --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        build-plugin-standalone
    fi
}

build-plugin-main $*
