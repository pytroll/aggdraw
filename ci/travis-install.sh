#!/usr/bin/env bash

if [ "${BUILDMODE}" = "ASTROPY" ]; then

    git clone --depth 1 git://github.com/astropy/ci-helpers.git
    source ci-helpers/travis/setup_conda.sh

elif [ "${BUILDMODE}" = "CIBUILDWHEEL" ]; then

  export PIP=pip
  if [ $(uname) = "Darwin" ]; then
    export PIP=pip2
  fi
  $PIP install cibuildwheel==0.10.0
  $PIP install -U git+https://github.com/natefoo/delocate.git@top-level-fix-squash

fi