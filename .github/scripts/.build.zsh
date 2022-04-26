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
#setopt WARN_CREATE_GLOBAL
#setopt WARN_NESTED_VAR
#setopt XTRACE

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

build() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os=${${(s:-:)ZSH_ARGZERO:t:r}[2]}
  local target="${host_os}-${CPUTYPE}"
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file="${project_root}/buildspec.json"
  trap '_trap_error' ZERR

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz log_info log_error log_output set_loglevel check_${host_os} setup_${host_os} setup_obs setup_ccache

  local -i _verbosity=1
  local -r _version='0.0.1'
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

%F{yellow} Build configuration options%f
 -----------------------------------------------------------------------------
  %B-t | --target%b                     Specify target - default: %B%F{green}${host_os}-${CPUTYPE}%f%b
  %B-c | --config%b                     Build configuration - default: %B%F{green}RelWithDebInfo%f%b
  %B-s | --codesign%b                   Enable codesigning (macOS only)

%F{yellow} Output options%f
 -----------------------------------------------------------------------------
  %B-q | --quiet%b                      Quiet (error output only)
  %B-v | --verbose%b                    Verbose (more detailed output)

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
      -s|--codesign) CODESIGN=1; shift ;;
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
  setup_${host_os}

  read -r product_name product_version <<< \
    "$(jq -r '. | {name, version} | join(" ")' ${project_root}/buildspec.json)"

  case ${host_os} {
    macos-*)
      sed -i '' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/" \
        "${project_root}"/CMakeLists.txt
      ;;
    linux-*)
      sed -i'' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/"\
        "${project_root}"/CMakeLists.txt
      ;;
  }

  setup_ccache
  setup_obs

  pushd ${project_root}
  log_info "Configuring ${product_name}..."

  local -a cmake_args=(
    -DCMAKE_BUILD_TYPE=${BUILD_CONFIG:-RelWithDebInfo}
    -DCMAKE_PREFIX_PATH="${project_root:h}/obs-build-dependencies/obs-plugin-deps"
  )

  if (( _loglevel == 0 )) cmake_args+=(-Wno_deprecated -Wno-dev --log-level=ERROR)

  case ${target} {
    macos-*)
      autoload -Uz read_codesign
      if (( ${+CODESIGN} )) {
        read_codesign
      }

      cmake_args+=(
        -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-10.15}
        -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}"
        -DCMAKE_FRAMEWORK_PATH="${project_root:h}/obs-build-dependencies/obs-plugin-deps/Frameworks"
      )
      ;;
    linux-*)
      if (( ${+CI} )) {
        cmake_args+=(-DCMAKE_INSTALL_PREFIX=/usr)
      }
      ;;
  }

  cmake -S . -B build_${target} -G Ninja ${cmake_args}

  local -a cmake_args=()
  if (( _loglevel > 1 )) cmake_args+=(--verbose)

  log_info "Building ${product_name}"
  cmake --build build_${target} --config ${BUILD_CONFIG:-RelWithDebInfo} ${cmake_args}

  log_info "Install ${product_name}"
  cmake --install build_${target} --config ${BUILD_CONFIG:-RelWithDebInfo} --prefix "${project_root}"/release ${cmake_args}
  popd
}

build ${@}
