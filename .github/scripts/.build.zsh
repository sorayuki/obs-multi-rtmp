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
  print -u2 -PR "${CI:+::error::}%F{1}${funcstack[1]##*/}:%f Running on Zsh version %B${ZSH_VERSION}%b, but Zsh %B5.2%b is the minimum supported version. Upgrade Zsh to fix this issue."
  exit 1
fi

TRAPEXIT() {
  local return_value=$?

  if (( ${+CI} )) unset NSUnbufferedIO

  return ${return_value}
}

TRAPZERR() {
  if (( ${_loglevel:-3} > 2 )) {
    print -u2 -PR "${CI:+::error::}%F{1}    ✖︎ script execution error%f"
    print -PR -e "
    Callstack:
    ${(j:\n     :)funcfiletrace}
    "
  }

  exit 2
}

build() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os=${${(s:-:)ZSH_ARGZERO:t:r}[2]}
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file=${project_root}/buildspec.json

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz log_group log_info log_error log_output set_loglevel check_${host_os} setup_ccache

  if [[ ! -r ${buildspec_file} ]] {
    log_error \
      'No buildspec.json found. Please create a build specification for your project.'
    return 2
  }

  typeset -g -a skips=()
  local -i verbosity=1
  local -r _version='2.0.0'
  local -r -a _valid_targets=(
    macos-universal
    linux-x86_64
    linux-aarch64
  )
  local target
  local config='RelWithDebInfo'
  local -r -a _valid_configs=(Debug RelWithDebInfo Release MinSizeRel)
  local -i codesign=0

  if [[ ${host_os} == linux ]] {
    local -r -a _valid_generators=(Ninja 'Unix Makefiles')
    local generator='Ninja'
    local -r _usage_host="
%F{yellow} Additional options for Linux builds%f
 -----------------------------------------------------------------------------
  %B--generator%b                       Specify build system to generate
                                    Available generators:
                                      - Ninja
                                      - Unix Makefiles"
  } elif [[ ${host_os} == macos ]] {
    local -r _usage_host="
%F{yellow} Additional options for macOS builds%f
 -----------------------------------------------------------------------------
  %B-s | --codesign%b                   Enable codesigning (macOS only)"
  }

  local -i print_config=0
  local -r _usage="
Usage: %B${functrace[1]%:*}%b <option> [<options>]

%BOptions%b:

%F{yellow} Build configuration options%f
 -----------------------------------------------------------------------------
  %B-t | --target%b                     Specify target
  %B-c | --config%b                     Build configuration
  %B--skip-[all|build|deps]%b           Skip all|building|checking for dependencies

%F{yellow} Output options%f
 -----------------------------------------------------------------------------
  %B-q | --quiet%b                      Quiet (error output only)
  %B-v | --verbose%b                    Verbose (more detailed output)
  %B--debug%b                           Debug (very detailed and added output)

%F{yellow} General options%f
 -----------------------------------------------------------------------------
  %B-h | --help%b                       Print this usage help
  %B-V | --version%b                    Print script version information
${_usage_host:-}"

  local -a args
  while (( # )) {
    case ${1} {
      -t|--target|-c|--config|--generator)
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
        config=${2}
        shift 2
        ;;
      -s|--codesign) codesign=1; shift ;;
      -q|--quiet) (( verbosity -= 1 )) || true; shift ;;
      -v|--verbose) (( verbosity += 1 )); shift ;;
      -h|--help) log_output ${_usage}; exit 0 ;;
      -V|--version) print -Pr "${_version}"; exit 0 ;;
      --debug) verbosity=3; shift ;;
      --generator)
        if [[ ${host_os} == linux ]] {
          if (( ! ${_valid_generators[(Ie)${2}]} )) {
            log_error "Invalid value %B${2}%b for option %B${1}%b"
            log_output ${_usage}
            exit 2
          }
          generator=${2}
        }
        shift 2
        ;;
      --print-config) print_config=1; skips+=(deps); shift ;;
      --skip-*)
        local -r _skip="${${(s:-:)1}[-1]}"
        local -r -a _check=(all build deps)
        (( ${_check[(Ie)${_skip}]} )) || log_warning "Invalid skip mode %B${_skip}%b supplied"
        typeset -g -a skips=(${skips} ${_skip})
        shift
        ;;
      *) log_error "Unknown option: %B${1}%b"; log_output ${_usage}; exit 2 ;;
    }
  }

  : "${target:="${host_os}-${CPUTYPE}"}"

  set -- ${(@)args}
  set_loglevel ${verbosity}

  if (( ! (${skips[(Ie)all]} + ${skips[(Ie)deps]}) )) {
    check_${host_os}
    setup_ccache
  }

  if [[ ${host_os} == linux ]] {
    autoload -Uz setup_linux && setup_linux
  }

  local product_name
  local product_version
  read -r product_name product_version <<< \
    "$(jq -r '. | {name, version} | join(" ")' ${buildspec_file})"

  pushd ${project_root}
  if (( ! (${skips[(Ie)all]} + ${skips[(Ie)build]}) )) {
    log_group "Configuring ${product_name}..."

    local -a cmake_args=()
    local -a cmake_build_args=(--build)
    local -a cmake_install_args=(--install)

    case ${_loglevel} {
      0) cmake_args+=(-Wno_deprecated -Wno-dev --log-level=ERROR) ;;
      1) ;;
      2) cmake_build_args+=(--verbose) ;;
      *) cmake_args+=(--debug-output) ;;
    }

    local -r _preset="${target%%-*}${CI:+-ci}"
    case ${target} {
      macos-*)
        if (( ${+CI} )) typeset -gx NSUnbufferedIO=YES

        cmake_args+=(
          --preset ${_preset}
        )

        if (( codesign )) {
          autoload -Uz read_codesign_team && read_codesign_team

          if [[ -z ${CODESIGN_TEAM} ]] {
            autoload -Uz read_codesign && read_codesign
          }
        }

        cmake_args+=(
          -DCODESIGN_TEAM=${CODESIGN_TEAM:-}
          -DCODESIGN_IDENTITY=${CODESIGN_IDENT:--}
        )

        cmake_build_args+=(--preset ${_preset} --parallel --config ${config} -- ONLY_ACTIVE_ARCH=NO -arch arm64 -arch x86_64)
        cmake_install_args+=(build_macos --config ${config} --prefix "${project_root}/release/${config}")

        local -a xcbeautify_opts=()
        if (( _loglevel == 0 )) xcbeautify_opts+=(--quiet)
        ;;
      linux-*)
        cmake_args+=(
          --preset ${_preset}-${target##*-}
          -G "${generator}"
          -DQT_VERSION=${QT_VERSION:-6}
          -DCMAKE_BUILD_TYPE=${config}
          -DCMAKE_INSTALL_PREFIX=/usr
        )

        local cmake_version
        read -r _ _ cmake_version <<< "$(cmake --version)"

        if [[ ${CPUTYPE} != ${target##*-} ]] {
          if is-at-least 3.21.0 ${cmake_version}; then
            cmake_args+=(--toolchain "${project_root}/cmake/linux/toolchains/${target##*-}-linux-gcc.cmake")
          else
            cmake_args+=(-D"CMAKE_TOOLCHAIN_FILE=${project_root}/cmake/linux/toolchains/${target##*-}-linux-gcc.cmake")
          fi
        }

        cmake_build_args+=(--preset ${_preset}-${target##*-} --config ${config})
        if [[ ${generator} == 'Unix Makefiles' ]] {
          cmake_build_args+=(--parallel $(( $(nproc) + 1 )))
        } else {
          cmake_build_args+=(--parallel)
        }

        cmake_install_args+=(build_${target##*-} --prefix ${project_root}/release/${config})
        ;;
    }

    log_debug "Attempting to configure with CMake arguments: ${cmake_args}"

    cmake ${cmake_args}

    log_group "Building ${product_name}..."
    if [[ ${host_os} == macos ]] {
      if (( _loglevel > 1 )) {
        cmake ${cmake_build_args}
      } else {
        cmake ${cmake_build_args} 2>&1 | xcbeautify ${xcbeautify_opts}
      }
    } else {
      cmake ${cmake_build_args}
    }
  }

  log_group "Installing ${product_name}..."
  if (( _loglevel > 1 )) cmake_install_args+=(--verbose)
  cmake ${cmake_install_args}
  popd
  log_group
}

build ${@}
