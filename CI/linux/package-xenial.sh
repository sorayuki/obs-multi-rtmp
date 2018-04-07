#!/bin/sh

set -e

cd /root/$PROJECT_NAME

export GIT_HASH=$(git rev-parse --short HEAD)
export PKG_VERSION="1-$GIT_HASH-$TRAVIS_BRANCH-git"

if [ -n "${TRAVIS_TAG}" ]; then
	export PKG_VERSION="$TRAVIS_TAG"
fi

cd /root/$PROJECT_NAME/build

PAGER=cat checkinstall -y --type=debian --fstrans=no --nodoc \
	--backup=no --deldoc=yes --install=no \
	--pkgname=$PROJECT_NAME --pkgversion="$PKG_VERSION" \
	--pkglicense="GPLv2.0" --maintainer="developer@change.me" \
	--pkggroup="general" \
	--pkgsource="https://www.mywebsite.com" \
	--pakdir="/package"

chmod ao+r /package/*
