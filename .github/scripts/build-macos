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

if (( ! ${+CI} )) {
  print -u2 -PR "%F{1}    ✖︎ ${ZSH_ARGZERO:t:r} requires CI environment.%f"
  exit 1
}

autoload -Uz is-at-least && if ! is-at-least 5.9; then
  print -u2 -PR "${CI:+::error::}%F{1}${funcstack[1]##*/}:%f Running on Zsh version %B${ZSH_VERSION}%b, but Zsh %B5.2%b is the minimum supported version. Upgrade Zsh to fix this issue."
  exit 1
fi

TRAPZERR() {
  print -u2 -PR "::error::%F{1}    ✖︎ script execution error%f"
  print -PR -e "
  Callstack:
  ${(j:\n     :)funcfiletrace}
  "

  exit 2
}

build() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os='macos'
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file=${project_root}/buildspec.json

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz log_group log_info log_error log_output check_macos setup_ccache

  if [[ ! -r ${buildspec_file} ]] {
    log_error \
      'No buildspec.json found. Please create a build specification for your project.'
    return 2
  }

  local -i debug=0

  local config='RelWithDebInfo'
  local -r -a _valid_configs=(Debug RelWithDebInfo Release MinSizeRel)
  local -i codesign=0

  local -a args
  while (( # )) {
    case ${1} {
      -c|--config)
        if (( # == 1 )) || [[ ${2:0:1} == '-' ]] {
          log_error "Missing value for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        ;;
    }
    case ${1} {
      --) shift; args+=($@); break ;;
      -c|--config)
        if (( ! ${_valid_configs[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          exit 2
        }
        config=${2}
        shift 2
        ;;
      -s|--codesign) codesign=1; shift ;;
      --debug) debug=1; shift ;;
      *) log_error "Unknown option: %B${1}%b"; exit 2 ;;
    }
  }

  set -- ${(@)args}

  check_macos

  local product_name
  local product_version
  read -r product_name product_version <<< \
    "$(jq -r '. | {name, version} | join(" ")' ${buildspec_file})"

  pushd ${project_root}

  local -a cmake_args=()
  local -a cmake_build_args=(--build)
  local -a cmake_install_args=(--install)

  if (( debug )) cmake_args+=(--debug-output)

  cmake_args+=(--preset 'macos-ci')

  typeset -gx NSUnbufferedIO=YES

  typeset -gx CODESIGN_IDENT="${CODESIGN_IDENT:--}"
  if (( codesign )) && [[ -z ${CODESIGN_TEAM} ]] {
    typeset -gx CODESIGN_TEAM="$(print "${CODESIGN_IDENT}" | /usr/bin/sed -En 's/.+\((.+)\)/\1/p')"
  }

  log_group "Configuring ${product_name}..."
  cmake -S ${project_root} ${cmake_args}

  log_group "Building ${product_name}..."
  run_xcodebuild() {
    if (( debug )) {
      xcodebuild ${@}
    } else {
      if [[ ${GITHUB_EVENT_NAME} == push ]] {
        xcodebuild ${@} 2>&1 | xcbeautify --renderer terminal
      } else {
        xcodebuild ${@} 2>&1 | xcbeautify --renderer github-actions
      }
    }
  }

  local -a build_args=(
    ONLY_ACTIVE_ARCH=NO
    -arch arm64
    -arch x86_64
    -project ${product_name}.xcodeproj
    -target ${product_name}
    -destination "generic/platform=macOS,name=Any Mac"
    -configuration ${config}
    -parallelizeTargets
    -hideShellScriptEnvironment
    build
  )

  pushd build_macos
  run_xcodebuild ${build_args}
  popd

  log_group "Installing ${product_name}..."
  cmake --install build_macos --config ${config} --prefix "${project_root}/release/${config}"

  popd
  log_group
}

build ${@}
