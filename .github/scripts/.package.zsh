#!/usr/bin/env zsh

builtin emulate -L zsh
setopt EXTENDED_GLOB
setopt PUSHD_SILENT
setopt ERR_EXIT
setopt ERR_RETURN
setopt NO_UNSET
setopt PIPE_FAIL
setopt NO_AUTO_PUSHD
setopt NO_PUSHD_IGNORE_DUPS
setopt FUNCTION_ARGZERO

## Enable for script debugging
# setopt WARN_CREATE_GLOBAL
# setopt WARN_NESTED_VAR
# setopt XTRACE

autoload -Uz is-at-least && if ! is-at-least 5.2; then
  print -u2 -PR "%F{1}${funcstack[1]##*/}:%f Running on Zsh version %B${ZSH_VERSION}%b, but Zsh %B5.2%b is the minimum supported version. Upgrade Zsh to fix this issue."
  exit 1
fi

_trap_error() {
  print -u2 -PR '%F{1}    ✖︎ script execution error%f'
  print -PR -e "
    Callstack:
    ${(j:\n     :)funcfiletrace}
  "
  exit 2
}

package() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os=${${(s:-:)ZSH_ARGZERO:t:r}[2]}
  local target="${host_os}-${CPUTYPE}"
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file="${project_root}/buildspec.json"
  trap '_trap_error' ZERR

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz set_loglevel log_info log_error log_output check_${host_os}

  local -i _verbosity=1
  local -r _version='1.0.0'
  local -r -a _valid_targets=(
    macos-x86_64
    macos-arm64
    macos-universal
    linux-x86_64
  )
  local -r -a _valid_configs=(Debug RelWithDebInfo Release MinSizeRel)
  local -r _usage="
Usage: %B${functrace[1]%:*}%b <option> [<options>]

%BOptions%b:

%F{yellow} Package configuration options%f
 -----------------------------------------------------------------------------
  %B-t | --target%b                     Specify target - default: %B%F{green}${host_os}-${CPUTYPE}%f%b
  %B-c | --config%b                     Build configuration - default: %B%F{green}RelWithDebInfo%f%b
  %B-s | --codesign%b                   Enable codesigning (macOS only)
  %B-n | --notarize%b                   Enable notarization (macOS only)

%F{yellow} Output options%f
 -----------------------------------------------------------------------------
  %B-q | --quiet%b                      Quiet (error output only)
  %B-v | --verbose%b                    Verbose (more detailed output)
  %B--debug%b                           Debug (very detailed and added output)

%F{yellow} General options%f
 -----------------------------------------------------------------------------
  %B-h | --help%b                       Print this usage help
  %B-V | --version%b                    Print script version information"

  local -a args
  while (( # )) {
    case ${1} {
      -t|--target|-c|--config)
        if (( # == 1 )) || [[ ${2:0:1} == '-' ]] {
          log_error "Missing value for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        ;;
    }
    case ${1} {
      --)
        shift
        args+=($@)
        break
        ;;
      -t|--target)
        if (( ! ${_valid_targets[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        target=${2}
        shift 2
        ;;
      -c|--config)
        if (( ! ${_valid_configs[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        BUILD_CONFIG=${2}
        shift 2
        ;;
      -s|--codesign) typeset -g CODESIGN=1; shift ;;
      -n|--notarize) typeset -g NOTARIZE=1; typeset -g CODESIGN=1; shift ;;
      -q|--quiet) (( _verbosity -= 1 )) || true; shift ;;
      -v|--verbose) (( _verbosity += 1 )); shift ;;
      -h|--help) log_output ${_usage}; exit 0 ;;
      -V|--version) print -Pr "${_version}"; exit 0 ;;
      --debug) _verbosity=3; shift ;;
      *) log_error "Unknown option: %B${1}%b"; log_output ${_usage}; exit 2 ;;
    }
  }

  set -- ${(@)args}
  set_loglevel ${_verbosity}

  check_${host_os}

  local product_name
  local product_version
  read -r product_name product_version <<< \
    "$(jq -r '. | {name, version} | join(" ")' ${project_root}/buildspec.json)"

  if [[ ${host_os} == 'macos' ]] {
    autoload -Uz check_packages read_codesign read_codesign_installer read_codesign_pass

    local output_name="${product_name}-${product_version}-${host_os}-${target##*-}.pkg"

    if [[ ! -d ${project_root}/release/${product_name}.plugin ]] {
      log_error 'No release artifact found. Run the build script or the CMake install procedure first.'
      return 2
    }

    if [[ ! -f ${project_root}/build_${target##*-}/installer-macos.generated.pkgproj ]] {
      log_error 'Packages project file not found. Run the build script or the CMake build and install procedures first.'
      return 2
    }

    check_packages

    log_info "Packaging ${product_name}..."
    pushd ${project_root}
    packagesbuild \
      --build-folder ${project_root}/release \
      ${project_root}/build_${target##*-}/installer-macos.generated.pkgproj

    if (( ${+CODESIGN} )) {
      read_codesign_installer
      productsign \
        --sign "${CODESIGN_IDENT_INSTALLER}" \
        "${project_root}/release/${product_name}.pkg" \
        "${project_root}/release/${output_name}"

      rm "${project_root}/release/${product_name}.pkg"
    } else {
      mv "${project_root}/release/${product_name}.pkg" \
        "${project_root}/release/${output_name}"
    }

    if (( ${+CODESIGN} && ${+NOTARIZE} )) {
      if [[ ! -f "${project_root}/release/${output_name}" ]] {
        log_error "No package for notarization found."
        return 2
      }

      read_codesign_installer
      read_codesign_pass

      xcrun notarytool submit "${project_root}/release/${output_name}" \
        --keychain-profile "OBS-Codesign-Password" --wait
      xcrun stapler staple "${project_root}/release/${output_name}"
    }
    popd
  } elif [[ ${host_os} == 'linux' ]] {
    local -a cmake_args=()
    if (( _loglevel > 1 )) cmake_args+=(--verbose)

    pushd ${project_root}
    cmake --build build_${target##*-} --config ${BUILD_CONFIG:-RelWithDebInfo} -t package ${cmake_args}
    popd
  }
}

package ${@}
