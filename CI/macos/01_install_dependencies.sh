#!/bin/bash

##############################################################################
# macOS dependency management function
##############################################################################
#
# This script file can be included in build scripts for macOS or run directly
#
##############################################################################

# Halt on errors
set -eE

install_obs-deps() {
    status "Set up precompiled macOS OBS dependencies v${1}"
    ensure_dir "${DEPS_BUILD_DIR}"
    step "Download..."
    check_and_fetch "https://github.com/obsproject/obs-deps/releases/download/${1}/macos-deps-${1}-universal.tar.xz" "${2}"
    mkdir -p obs-deps
    step "Unpack..."
    /usr/bin/tar -xf "./macos-deps-${1}-universal.tar.xz" -C ./obs-deps
    /usr/bin/xattr -r -d com.apple.quarantine ./obs-deps
}

install_qt-deps() {
    status "Set up precompiled dependency Qt v${1}"
    ensure_dir "${DEPS_BUILD_DIR}"
    step "Download..."
    check_and_fetch "https://github.com/obsproject/obs-deps/releases/download/${1}/macos-deps-qt-${1}-universal.tar.xz" "${2}"
    mkdir -p obs-deps
    step "Unpack..."
    /usr/bin/tar -xf "./macos-deps-qt-${1}-universal.tar.xz" -C ./obs-deps
    /usr/bin/xattr -r -d com.apple.quarantine ./obs-deps
}

install_obs-studio() {
    if [ "${OBS_BRANCH}" ]; then
        CHECKOUT_REF="${OBS_BRANCH}"
    else
        CHECKOUT_REF="tags/${OBS_VERSION:-${CI_OBS_VERSION}}"
    fi

    ensure_dir "${OBS_BUILD_DIR}"

    if [ ! -d "${OBS_BUILD_DIR}/.git" ]; then
        /usr/bin/git clone --recursive https://github.com/obsproject/obs-studio "$(pwd)"
        /usr/bin/git fetch origin --tags
        /usr/bin/git checkout ${CHECKOUT_REF} -b obs-plugin-build
    else
        if ! /usr/bin/git show-ref --verify --quiet refs/heads/obs-plugin-build; then
            /usr/bin/git checkout ${CHECKOUT_REF} -b obs-plugin-build
        else
            /usr/bin/git checkout obs-plugin-build
        fi
    fi
}

install_dependencies() {
    status "Installing Homebrew dependencies"
    trap "caught_error 'install_dependencies'" ERR

    BUILD_DEPS=(
        "obs-deps ${MACOS_DEPS_VERSION:-${CI_DEPS_VERSION}} ${MACOS_DEPS_HASH:-${CI_DEPS_HASH}}"
        "qt-deps ${MACOS_DEPS_VERSION:-${CI_DEPS_VERSION}} ${QT_HASH:-${CI_QT_HASH}}"
        "obs-studio ${OBS_VERSION:-${CI_OBS_VERSION}}"
    )

    install_homebrew_deps

    for DEPENDENCY in "${BUILD_DEPS[@]}"; do
        set -- ${DEPENDENCY}
        trap "caught_error ${DEPENDENCY}" ERR
        FUNC_NAME="install_${1}"
        ${FUNC_NAME} ${2} ${3}
    done
}

install-dependencies-standalone() {
    CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
    if [ -f "${CHECKOUT_DIR}/CI/include/build_environment.sh" ]; then
        source "${CHECKOUT_DIR}/CI/include/build_environment.sh"
    fi
    PRODUCT_NAME="${PRODUCT_NAME:-obs-plugin}"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    OBS_BUILD_DIR="${CHECKOUT_DIR}/../obs-studio"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_macos.sh"

    status "Setting up plugin build dependencies"
    check_macos_version
    check_archs
    install_dependencies
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-a, --architecture             : Specify build architecture (default: universal, alternative: x86_64, arm64)\n"
}

install-dependencies-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -a | --architecture ) ARCH="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        install-dependencies-standalone
    fi
}

install-dependencies-main $*
