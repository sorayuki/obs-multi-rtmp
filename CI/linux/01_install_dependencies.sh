#!/bin/bash

##############################################################################
# Linux dependency management function
##############################################################################
#
# This script file can be included in build scripts for Linux or run directly
#
##############################################################################

# Halt on errors
set -eE

install_obs-studio() {
    if [ -n "${OBS_BRANCH}" ]; then
        CHECKOUT_REF="${OBS_BRANCH}"
    else
        CHECKOUT_REF="tags/${OBS_VERSION:-${CI_OBS_VERSION}}"
    fi

    ensure_dir "${OBS_BUILD_DIR}"

    if [ ! -d "${OBS_BUILD_DIR}/.git" ]; then
        git clone --recursive https://github.com/obsproject/obs-studio "$(pwd)"
        git fetch origin --tags
        git checkout ${CHECKOUT_REF} -b obs-plugin-build
    else
        if ! git show-ref --verify --quiet refs/heads/obs-plugin-build; then
            git checkout ${CHECKOUT_REF} -b obs-plugin-build
        else
            git checkout obs-plugin-build
        fi
    fi
}

install_linux_dependencies() {
    sudo dpkg --add-architecture amd64
    sudo apt-get -qq update
    sudo apt-get install -y \
        build-essential \
        ninja-build \
        clang \
        clang-format \
        qtbase5-dev \
        libqt5svg5-dev \
        libqt5x11extras5-dev \
        qtbase5-private-dev \
        libwayland-dev \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswresample-dev \
        libswscale-dev \
        libx264-dev \
        libjansson-dev \
        libpulse-dev \
        libx11-dev \
        libx11-xcb-dev \
        libmbedtls-dev \
        libgl1-mesa-dev \
        pkg-config \
        libcurl4-openssl-dev

    if ! type cmake &>/dev/null; then
        sudo apt-get install -y cmake
    fi
}

install_dependencies() {
    status "Installing build dependencies"
    trap "caught_error 'install_dependencies'" ERR

    BUILD_DEPS=(
        "obs-studio ${OBS_VERSION:-${CI_OBS_VERSION}}"
    )

    install_linux_dependencies

    for DEPENDENCY in "${BUILD_DEPS[@]}"; do
        set -- ${DEPENDENCY}
        trap "caught_error ${DEPENDENCY}" ERR
        FUNC_NAME="install_${1}"
        ${FUNC_NAME} ${2} ${3}
    done
}

install-dependencies-standalone() {
    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
        source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
    fi
    PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    OBS_BUILD_DIR="${CHECKOUT_DIR}/../obs-studio"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

    status "Setting up plugin build dependencies"
    install_dependencies
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n"
}

install-dependencies-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        install-dependencies-standalone
    fi
}

install-dependencies-main $*
