@ECHO OFF
SETLOCAL

IF /I "%BUILDMODE"=="CI-HELPERS" (
  %CMD_IN_ENV% pip install -e .
  %CMD_IN_ENV% python selftest.py
  IF %ERRORLEVEL% EQU 0 (
      %CMD_IN_ENV% python setup.py bdist_wheel bdist_wininst
  )
) ELSE (
  cibuildwheel --output-dir dist
)
