#!/usr/bin/env bash

if [[ "${BUILDMODE}" == "ASTROPY" ]]; then
#    git clone --depth 1 git://github.com/astropy/ci-helpers.git
    git clone --depth 1 -b all-the-fixes git://github.com/djhoese/ci-helpers.git
    source ci-helpers/travis/setup_conda.sh
elif [[ "${BUILDMODE}" == "CIBUILDWHEEL" ]]; then
  export PIP=pip
  if [[ $(uname) == "Darwin" ]]; then
    export PIP=pip2
  fi
  $PIP install cibuildwheel
fi