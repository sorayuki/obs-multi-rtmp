#!/bin/bash

##############################################################################
# macOS support functions
##############################################################################
#
# This script file can be included in build scripts for macOS.
#
##############################################################################

# Setup build environment

CI_DEPS_VERSION=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+DEPS_VERSION_MAC: '([0-9\-]+)'/\1/p")
CI_DEPS_HASH=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+DEPS_HASH_MAC: '([0-9a-f]+)'/\1/p")
CI_QT_VERSION=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+QT_VERSION_MAC: '([0-9\.]+)'/\1/p" | /usr/bin/head -1)
CI_QT_HASH=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+QT_HASH_MAC: '([0-9a-f]+)'/\1/p")
CI_MACOSX_DEPLOYMENT_TARGET=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+MACOSX_DEPLOYMENT_TARGET: '([0-9\.]+)'/\1/p")
CI_OBS_VERSION=$(/bin/cat "${CI_WORKFLOW}" | /usr/bin/sed -En "s/[ ]+OBS_VERSION: '([0-9\.]+)'/\1/p")

MACOS_VERSION="$(/usr/bin/sw_vers -productVersion)"
MACOS_MAJOR="$(echo ${MACOS_VERSION} | /usr/bin/cut -d '.' -f 1)"
MACOS_MINOR="$(echo ${MACOS_VERSION} | /usr/bin/cut -d '.' -f 2)"

if [ "${TERM-}" -a -z "${CI}" ]; then
    COLOR_RED=$(/usr/bin/tput setaf 1)
    COLOR_GREEN=$(/usr/bin/tput setaf 2)
    COLOR_BLUE=$(/usr/bin/tput setaf 4)
    COLOR_ORANGE=$(/usr/bin/tput setaf 3)
    COLOR_RESET=$(/usr/bin/tput sgr0)
else
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_BLUE=""
    COLOR_ORANGE=""
    COLOR_RESET=""
fi

## DEFINE UTILITIES ##
check_macos_version() {
    step "Check macOS version..."
    MIN_VERSION=${MACOSX_DEPLOYMENT_TARGET:-${CI_MACOSX_DEPLOYMENT_TARGET}}
    MIN_MAJOR=$(echo ${MIN_VERSION} | /usr/bin/cut -d '.' -f 1)
    MIN_MINOR=$(echo ${MIN_VERSION} | /usr/bin/cut -d '.' -f 2)

    if [ "${MACOS_MAJOR}" -lt "11" ] && [ "${MACOS_MINOR}" -lt "${MIN_MINOR}" ]; then
        error "WARNING: Minimum required macOS version is ${MIN_VERSION}, but running on ${MACOS_VERSION}"
    fi

    if [ "${MACOS_MAJOR}" -ge "11" ]; then
        export CODESIGN_LINKER="ON"
    fi
}

install_homebrew_deps() {
    if ! exists brew; then
        error "Homebrew not found - please install homebrew (https://brew.sh)"
        exit 1
    fi

    brew bundle --file "${CHECKOUT_DIR}/CI/include/Brewfile" ${QUIET:+--quiet}

    check_curl
}

check_curl() {
    if [ "${MACOS_MAJOR}" -lt "11" ] && [ "${MACOS_MINOR}" -lt "15" ]; then
        if [ ! -d /usr/local/opt/curl ]; then
            step "Installing Homebrew curl.."
            brew install curl
        fi

        CURLCMD="/usr/local/opt/curl/bin/curl"
    else
        CURLCMD="curl"
    fi

    if [ "${CI}" -o "${QUIET}" ]; then
        export CURLCMD="${CURLCMD} --silent --show-error --location -O"
    else
        export CURLCMD="${CURLCMD} --progress-bar --location --continue-at - -O"
    fi
}

check_archs() {
    step "Check Architecture..."
    ARCH="${ARCH:-universal}"
    if [ "${ARCH}" = "universal" ]; then
        CMAKE_ARCHS="x86_64;arm64"
    elif [ "${ARCH}" != "x86_64" -a "${ARCH}" != "arm64" ]; then
        caught_error "Unsupported architecture '${ARCH}' provided"
    else
        CMAKE_ARCHS="${ARCH}"
    fi
}

## SET UP CODE SIGNING AND NOTARIZATION CREDENTIALS ##
##############################################################################
# Apple Developer Identity needed:
#
#    + Signing the code requires a developer identity in the system's keychain
#    + codesign will look up and find the identity automatically
#
##############################################################################
read_codesign_ident() {
    if [ ! -n "${CODESIGN_IDENT}" ]; then
        step "Code-signing Setup"
        read -p "${COLOR_ORANGE}  + Apple developer application identity: ${COLOR_RESET}" CODESIGN_IDENT
    fi
}

read_codesign_ident_installer() {
    if [ ! -n "${CODESIGN_IDENT_INSTALLER}" ]; then
        step "Code-signing Setup for Installer"
        read -p "${COLOR_ORANGE}  + Apple developer installer identity: ${COLOR_RESET}" CODESIGN_IDENT_INSTALLER
    fi
}

##############################################################################
# Apple Developer credentials necessary:
#
#   + Signing for distribution and notarization require an active Apple
#     Developer membership
#   + An Apple Development identity is needed for code signing
#     (i.e. 'Apple Development: YOUR APPLE ID (PROVIDER)')
#   + Your Apple developer ID is needed for notarization
#   + An app-specific password is necessary for notarization from CLI
#   + This password will be stored in your macOS keychain under the identifier
#     'OBS-Codesign-Password'with access Apple's 'altool' only.
##############################################################################

read_codesign_pass() {
    step "Notarization Setup"

    if [ -z "${CODESIGN_IDENT_USER}" ]; then
        read -p "${COLOR_ORANGE}  + Apple account id: ${COLOR_RESET}" CODESIGN_IDENT_USER
    fi

    if [ -z "${CODESIGN_IDENT_PASS}" ]; then
        CODESIGN_IDENT_PASS=$(stty -echo; read -p "${COLOR_ORANGE}  + Apple developer password: ${COLOR_RESET}" secret; stty echo; echo $secret)
        echo ""
    fi

    step "Updating notarization keychain"

    echo -n "${COLOR_ORANGE}"
    /usr/bin/xcrun altool --store-password-in-keychain-item "OBS-Codesign-Password" -u "${CODESIGN_IDENT_USER}" -p "${CODESIGN_IDENT_PASS}"
    echo -n "${COLOR_RESET}"
    CODESIGN_IDENT_SHORT=$(echo "${CODESIGN_IDENT}" | /usr/bin/sed -En "s/.+\((.+)\)/\1/p")
}
