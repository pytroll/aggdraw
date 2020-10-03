#!/usr/bin/env bash

if [[ "${BUILDMODE}" == "ASTROPY" ]]; then
    git clone --depth 1 https://github.com/astropy/ci-helpers.git
    echo "Done cloning ci-helpers"
    source ci-helpers/travis/setup_conda.sh
    echo "Done sourcing conda environment"
elif [[ "${BUILDMODE}" == "CIBUILDWHEEL" ]]; then
  export PIP=pip
  if [[ $(uname) == "Darwin" ]]; then
    export PIP=pip2
  fi
  $PIP install cibuildwheel
fi
