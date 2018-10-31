#!/usr/bin/env bash

set -ex

if [ "${BUILDMODE}" = "ASTROPY" ]; then
    
    pip install -e .
    python selftest.py

elif [ "${BUILDMODE}" = "CIBUILDWHEEL" ]; then

    export PIP=pip
    if [ $(uname) = "Darwin" ]; then
      export PIP=pip2
    fi
    cibuildwheel --output-dir wheelhouse
    if [[ $TRAVIS_TAG ]]; then
        python -m pip install twine
        python -m twine upload wheelhouse/*.whl
    fi
fi