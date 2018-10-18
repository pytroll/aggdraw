@ECHO OFF
SETLOCAL

IF /I "%BUILDMODE"=="CI-HELPERS" (
  git clone --depth 1 git://github.com/astropy/ci-helpers.git
  powershell ci-helpers/appveyor/install-miniconda.ps1
  SET PATH=%PYTHON%;%PYTHON%\\Scripts;%PATH%
  activate test
) ELSE (
  pip install cibuildwheel==0.10.0
)
