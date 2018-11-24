# Small script for uploading wheels to pypi with twine

if($env:appveyor_repo_tag -eq "true") {
    python -m pip install twine
    python -m twine upload --skip-existing dist/*.whl
}