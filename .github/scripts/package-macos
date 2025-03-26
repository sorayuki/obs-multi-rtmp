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
  print -u2 -PR "%F{1}    ✖︎ ${ZSH_ARGZERO:t:r} requires CI environment%f"
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

package() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os='macos'
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file=${project_root}/buildspec.json

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz log_group log_error log_output check_macos

  if [[ ! -r ${buildspec_file} ]] {
    log_error \
      'No buildspec.json found. Please create a build specification for your project.'
    return 2
  }

  local -i debug=0

  local config='RelWithDebInfo'
  local -r -a _valid_configs=(Debug RelWithDebInfo Release MinSizeRel)

  local -i codesign=0
  local -i notarize=0
  local -i package=0

  local -a args
  while (( # )) {
    case ${1} {
      -c|--config)
        if (( # == 1 )) || [[ ${2:0:1} == '-' ]] {
          log_error "Missing value for option %B${1}%b"
          exit 2
        }
        ;;
    }
    case ${1} {
      --) shift; args+=($@); break ;;
      -c|--config)
        if (( !${_valid_configs[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          exit 2
        }
        config=${2}
        shift 2
        ;;
      -s|--codesign) typeset -g codesign=1; shift ;;
      -n|--notarize) typeset -g notarize=1; typeset -g codesign=1; shift ;;
      -p|--package) typeset -g package=1; shift ;;
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

  local output_name="${product_name}-${product_version}-${host_os}-universal"

  if [[ ! -d ${project_root}/release/${config}/${product_name}.plugin ]] {
    log_error 'No release artifact found. Run the build script or the CMake install procedure first.'
    return 2
  }

  if (( package )) {
    if [[ ! -f ${project_root}/release/${config}/${product_name}.pkg ]] {
      log_error 'Installer Package not found. Run the build script or the CMake build and install procedures first.'
      return 2
    }

    log_group "Packaging ${product_name}..."
    pushd ${project_root}

    typeset -gx CODESIGN_IDENT="${CODESIGN_IDENT:--}"
    typeset -gx CODESIGN_IDENT_INSTALLER="${CODESIGN_IDENT_INSTALLER:--}"
    typeset -gx CODESIGN_TEAM="$(print "${CODESIGN_IDENT}" | /usr/bin/sed -En 's/.+\((.+)\)/\1/p')"

    if (( codesign )) {
      productsign \
        --sign "${CODESIGN_IDENT_INSTALLER}" \
        ${project_root}/release/${config}/${product_name}.pkg \
        ${project_root}/release/${output_name}.pkg

      rm ${project_root}/release/${config}/${product_name}.pkg
    } else {
      mv ${project_root}/release/${config}/${product_name}.pkg \
        ${project_root}/release/${output_name}.pkg
    }

    if (( codesign && notarize )) {
      if ! [[ ${CODESIGN_IDENT} != '-' && ${CODESIGN_TEAM} && {CODESIGN_IDENT_USER} && ${CODESIGN_IDENT_PASS} ]] {
        log_error "Notarization requires Apple ID and application password."
        return 2
      }

      if [[ ! -f ${project_root}/release/${output_name}.pkg ]] {
        log_error "No package for notarization found."
        return 2
      }

      xcrun notarytool store-credentials "${product_name}-Codesign-Password" --apple-id "${CODESIGN_IDENT_USER}" --team-id "${CODESIGN_TEAM}" --password "${CODESIGN_IDENT_PASS}"
      xcrun notarytool submit ${project_root}/release/${output_name}.pkg --keychain-profile "${product_name}-Codesign-Password" --wait

      local -i _status=0

      xcrun stapler staple ${project_root}/release/${output_name}.pkg || _status=1

      if (( _status )) {
        log_error "Notarization failed. Use 'xcrun notarytool log <submission ID>' to check for errors."
        return 2
      }
    }
    popd
  } else {
    log_group "Archiving ${product_name}..."
    pushd ${project_root}/release/${config}
    XZ_OPT=-T0 tar -cvJf ${project_root}/release/${output_name}.tar.xz ${product_name}.plugin
    popd
  }

  if [[ ${config} == Release ]] {
    log_group "Archiving ${product_name} Debug Symbols..."
    pushd ${project_root}/release/${config}
    XZ_OPT=-T0 tar -cvJf ${project_root}/release/${output_name}-dSYMs.tar.xz ${product_name}.plugin.dSYM
    popd
  }

  log_group
}

package ${@}
