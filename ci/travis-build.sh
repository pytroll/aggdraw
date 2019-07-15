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
    if [ $(uname) = "Darwin" ]; then
        # Re-do delocate with patched version that actually works for aggdraw
        $PIP install -U git+https://github.com/natefoo/delocate.git@top-level-fix-squash
        export PATH="$PATH:/Library/Frameworks/Python.framework/Versions/2.7/bin"
        WHEELS=wheelhouse/*.whl
        for w in $WHEELS
        do
            delocate-wheel -v $w
        done
    fi

    if [[ $TRAVIS_TAG ]]; then
        python -m pip install twine
        python -m twine upload --skip-existing wheelhouse/*.whl
        if [ $(uname) = "Darwin" ]; then # so we only do this once
            python setup.py sdist
            python -m twine upload --skip-existing dist/*.tar.gz
        fi
    fi

fi
