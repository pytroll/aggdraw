# AGENTS.md

Guidance for AI coding agents (and new human contributors) working in this repository.

## What this project is

`aggdraw` is a Python wrapper around [Anti-Grain Geometry](https://agg.sourceforge.net/antigrain.com/index.html)
(AGG) 2.x, providing a high-quality anti-aliased 2D drawing interface for PIL/Pillow images.
It was written by Fredrik Lundh (effbot) around 2003 and is now maintained by the PyTroll
group. It is old code that has been dragged forward through many Python versions; expect
inconsistent style and latent bugs.

## Architecture — three layers

```
agg2/                     vendored AGG 2 C++ sources  (DO NOT EDIT — see "Vendored AGG" below)
aggdraw/_aggdraw.cxx      hand-written CPython C extension, built as `aggdraw._aggdraw`
aggdraw/core.py           thin pure-Python wrapper — the public, documented API
aggdraw/__init__.py       re-exports Draw, Pen, Brush, Path, Symbol, Font + VERSION
```

- `aggdraw/_aggdraw.cxx` (~2500 lines) uses the low-level Python C API. `Pen`, `Brush`, `Font`,
  `Path`, and `Draw` are **heap types**, built with `PyType_FromSpec` in `aggdraw_init` and
  exposed on the module as real, subclassable classes. Each type's `PyType_Spec` sits next to
  that type's own methods. Two consequences to respect:
  - The deallocators must go through `tp_free` and then `Py_DECREF(Py_TYPE(self))`. Never call
    `PyObject_DEL` on one of these objects, and never call a `*_dealloc` directly — use
    `Py_DECREF`.
  - The `Pen_Check` / `Brush_Check` / `Font_Check` / `Path_Check` macros use `PyObject_TypeCheck`,
    not an identity test, because `Py_TPFLAGS_BASETYPE` is set. An identity test would make
    `draw_adaptor::draw` silently ignore a subclass.
- **`Symbol` is a subclass of `Path`**, at both layers. The C module exposes no functions at
  all now — `moduledef.m_methods` is `NULL`. Because a spec's `Py_tp_base` slot cannot name a
  type that does not exist until `aggdraw_init` runs, `Symbol` is built with
  `PyType_FromSpecWithBases`, so the `types[]` registration table carries a `base` column and
  **the `Symbol` row must come after the `Path` row**. `symbol_spec` defines only `tp_new` and
  `tp_doc` — `tp_doc` is not inherited, and giving it `tp_methods` would build a second
  descriptor set bound to `SymbolType`, making `Symbol.lineto()` reject a plain `Path`.
  `Path.from_svg()` (`METH_VARARGS | METH_CLASS`) and `Symbol.__new__` both call one shared
  parser, `path_from_svg_impl`, which allocates from the type it is handed.
- **`Draw.symbol()` and `Draw.path()` both type-check against `PathType`**, so each accepts a
  `Path` or a `Symbol`. Do not narrow `Draw.symbol()` to `SymbolType` — passing a plain `Path`
  to it is documented, tested behaviour.
- `aggdraw/core.py` is a recent addition. Each wrapper class holds a handle to the C object
  (`self._pen`, `self._brush`, `self._font`, `self._path`, `self._draw`) and forwards calls.
  Its purpose is documentation, IDE discoverability, and a place to put Python-side niceties.
- Anything that is not re-exported by `aggdraw/__init__.py` is not public API.

## Documentation lives in `core.py` docstrings

`doc/source/index.rst` is the **only** documentation page, and it renders the entire API from a
single bare `.. automodule:: aggdraw`. Consequences:

- A new public method must live in `core.py` **with a Google/napoleon-style docstring**, or it
  will not appear in the docs at all.
- Read the Docs sets `fail_on_warning: true` (`.readthedocs.yml`), so a broken cross-reference
  or malformed docstring fails the docs build. **This is not a safety net for a role written
  without backticks**: `:meth:foo` is never parsed as a role, so it renders as literal text and
  emits no warning. `sphinx-build -W` passing does not mean the docstrings render correctly —
  read the generated HTML for anything you change.
- `doc/source/conf.py` sets `autodoc_member_order = 'bysource'`, so methods appear in the docs
  in the order they are defined in `core.py`. Adding a method puts it wherever you place it.
- The C-level docstrings in `_aggdraw.cxx` use numpydoc style and are **not** rendered anywhere.
  They are reachable only via `help()` on `aggdraw._aggdraw` objects. Several of them are known
  to be stale or wrong. If you change behaviour, update both.
- The archived effbot documentation is linked from the README and the docs index. **It is out of
  date and has been the source of multiple documentation errors.** Do not use it as a reference
  for current behaviour — verify against `_aggdraw.cxx` or by running the code.

## Build and test

The C extension must be rebuilt after **any** change to `aggdraw/_aggdraw.cxx` or `agg2/`.
Editing `core.py` alone needs no rebuild.

```bash
# Build in place / editable install
python -m pip install -e .

# Force a specific freetype prefix if autodetection picks the wrong one
AGGDRAW_FREETYPE_ROOT=/usr python -m pip install -e .

# Run the tests. In a source checkout, pass the path. `[tool.pytest.ini_options]`
# in pyproject.toml sets `testpaths = ["aggdraw"]`, so a bare `pytest` from the repo
# root is scoped to the package too and will not collect stray test_*.py files.
pytest -v aggdraw/tests

# Use --pyargs only to test an INSTALLED aggdraw with no source tree present -- it
# resolves the tests that ship inside the package. This is what cibuildwheel runs.
pytest -v --pyargs aggdraw.tests

# Docs
python -m pip install -e ".[docs]"
sphinx-build -W -b html doc/source doc/_build/html    # -W matches RTD's fail_on_warning

# A dev environment matching CI
conda env create -f ci/environment.yaml && conda activate test-environment
```

CI (`.github/workflows/ci.yml`) runs the test matrix on conda across ubuntu/macos/windows for
Python 3.11, 3.12, and 3.14, then builds wheels with `cibuildwheel` and publishes on `v*` tags.

## FreeType is optional

`setup.py` looks for FreeType in three ways, in order: `freetype-config --prefix`,
`ctypes.util.find_library('freetype')`, then `pkgconfig.variables('freetype2')`. Override with
the `AGGDRAW_FREETYPE_ROOT` environment variable.

When FreeType is **not** found, `HAVE_FREETYPE2` is undefined and `Draw.text` / `Draw.textsize`
are not compiled into the extension at all — calling them raises `AttributeError`, not a
friendly error. `Font()` raises `IOError("cannot load font (no text renderer)")`. Windows
wheels are deliberately built without FreeType (`pyproject.toml`), so never assume text
rendering is available.

## Vendored AGG (`agg2/`)

`agg2/` is upstream AGG source, vendored. **Do not edit it directly.** It no longer compiles
cleanly with modern compilers, so the project carries patches:

- Put new patches in `patches/` and record the command used to apply them, e.g.
  `patch -p0 patches/tags_pointer_type_fix.patch` (see the "Additional Patches" section of
  `README.rst`).
- Only 9 of the `agg2/src/*.cpp` files are actually compiled; the list is hardcoded in
  `setup.py`. `agg_vcgen_dash.cpp` is deliberately commented out, which is why `Pen` has no
  dash support.
- Understanding `agg2/` is genuinely hard. Prefer to solve problems in `_aggdraw.cxx` or
  `core.py` first.

## Behaviours that surprise people

These are verified facts about the current code. Do not "fix" them without a deliberate
decision, and do not document them wrongly.

- **Colors.** An integer color is a **gray level** (`rgba8(ink, ink, ink, opacity)`), *not*
  `0xAARRGGBB`. Out-of-range integers wrap silently (`300` → 44). A 3-tuple takes its alpha
  from the `opacity` argument; a **4-tuple overrides `opacity` entirely**. `"#rrggbb"` is parsed
  in C; every other string goes to `PIL.ImageColor.getrgb` **first**, with a 17-name built-in
  table only as a fallback — so `"#rgb"`, `"rgb(...)"`, `"hsl(...)"` and full CSS names all
  work. Strings PIL resolves to *four* components (`"rgba(...)"`) do **not**: the C side parses
  the result with `"iii"`. **An unrecognised color silently becomes black** — no exception is
  raised.
- **Pen/brush argument order is cosmetic.** All shape methods funnel into a C dispatcher that
  picks the pen and brush **by type**, not by position. `Draw.arc(xy, start, end, brush)` will
  fill. Any argument that is neither a `Pen` nor a `Brush` (including `None`) is silently
  ignored. This is why `core.Draw._parse_args` can accept a `Pen` in the `brush=` slot.
- **`Pen` width straddles the path.** A width-1 pen on integer coordinates paints two half-lit
  pixel columns. Draw on half-pixel coordinates (`x.5`) when you want crisp odd-width lines —
  and when writing tests that assert exact pixel values.
- **`Path.close()` closes the current subpath**, returning to the start of the most recent
  `moveto`, not to the first point of the whole path. And **closing a subpath with fewer than
  three distinct points deletes it** — it encloses no area, and AGG strokes closed paths as
  polygon outlines, so nothing is drawn. `Path([100,100, 400,100]).close()` renders nothing.
  `coords()` is identical before and after `close()`, so this is only observable in pixels.
- **`Symbol` is deprecated** in favour of `Path.from_svg()`, and constructing one emits a
  **`UserWarning`** — deliberately not a `DeprecationWarning`, which is ignored by default
  outside `__main__`. The warning lives in `core.Symbol.__new__`, not `__init__`, so that the
  inherited `Path.from_svg` (which calls `cls.__new__` and skips `__init__`) warns too. The C
  layer never warns: `_aggdraw` is not public API, and `PyErr_WarnEx` has no `stack_level` that
  is correct for both entry points. See
  [pytroll/aggdraw#145](https://github.com/pytroll/aggdraw/issues/145).
- **`Draw('BGRA', ...)` reports `.mode == 'RGBA'`.** Anything that round-trips through
  `Image.frombytes(draw.mode, draw.size, draw.tobytes())` will get the channel order wrong for
  BGRA surfaces.
- **`Draw(pil_image)` takes exactly one argument.** The C constructor tries the 1-argument form
  first, so `Draw(image, size)` raises `TypeError`. `Draw("RGB")` without a size raises a
  confusing `AttributeError: 'str' object has no attribute 'mode'`.
- Coordinate sequences must have an even length. Conversion errors inside `getpoints` are
  swallowed, so non-numeric coordinates become `-1.0` instead of raising.

## Testing conventions

- Plain pytest. No fixtures and no `conftest.py`; `@pytest.mark.parametrize` is used, but only
  for the `Draw`-method matrix in `test_path.py`. No golden or reference images anywhere.
- Tests live in `aggdraw/tests/` and ship inside the installed package (cibuildwheel runs
  `pytest --pyargs aggdraw.tests` against the built wheel).
- One module per drawing type — put a new test in the one that matches what it draws:
  `test_pen.py`, `test_brush.py`, `test_draw.py`, `test_path.py`, `test_symbol.py`.
  `test_aggdraw.py` is only for package-level checks (`VERSION`, `__version__`).
- Shared helpers live in `aggdraw/tests/_helpers.py`, not in any test module: `to_image(draw)`
  wraps `Image.frombytes(draw.mode, draw.size, draw.tobytes())`, `ink_count(im)` counts
  non-background pixels, and `WHITE` is the background constant. Import them; do not
  re-implement them. Note `to_image` gets the channel order wrong for BGRA surfaces, because
  `Draw('BGRA', ...)` reports `.mode == 'RGBA'` (see "Behaviours that surprise people").
- Assert `!= WHITE` rather than an exact ink color when a shape is drawn with a `Pen` on integer
  coordinates — the pen straddles the path, so edges come out antialiased gray. Exact-color
  assertions are for half-pixel coordinates (`x.5`) or brush fills.
- A deprecation warning inside a tight `sys.getallocatedblocks()` loop inflates the
  measurement (~8000 blocks per 2200 warns) whenever the active filter is `always` — which
  `-W always` sets, and which pytest 8 and older set for every test. Allocation-measuring tests
  must call non-deprecated APIs; see `test_from_svg_error_paths_do_not_leak`.
- Prefer asserting pixels over merely calling the API. Several existing tests are pure smoke
  tests with no assertions; don't add more.

## Releases

1. Bump `VERSION` in `aggdraw/__init__.py`. That is the single source of truth, read two ways:
   `pyproject.toml` declares `version` as dynamic and picks it up via
   `[tool.setuptools.dynamic] version = {attr = "aggdraw.VERSION"}` (a static AST parse, so the
   extension need not be built), and `setup.py` regex-reads it to pass into C as a `-DVERSION`
   macro.
2. Add a `## Version X.Y.Z` section at the top of `CHANGELOG.md`.
3. Tag `vX.Y.Z` and push; the `publish` CI job uploads to PyPI.


## House rules

- **Linting and formatting are handled by pre-commit** (`.pre-commit-config.yaml`), which runs
  `ruff-check --fix` and `ruff-format`. Ruff is configured in `pyproject.toml`. Run
  `pre-commit install` once, or `pre-commit run --all-files` to check the whole tree. The
  vendored `agg2/` sources are excluded and must stay that way. Do not reformat code unrelated
  to your change as a side effect of another change.
- Keep diffs minimal and focused. This codebase is old and lightly tested; large mechanical
  changes are hard to review.
- Verify claims about behaviour by running code, not by reading the archived docs or trusting
  existing docstrings.
