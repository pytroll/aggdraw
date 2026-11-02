# The aggdraw Library

## Unreleased

- Fix incorrect and missing docstrings in the C extension (`help()` output)
- Declare Pillow as a runtime dependency (`install_requires`)
- Remove all Python 2 support code from the C extension. The minimum supported
  Python has been 3.11 since 1.4.0, so every `#ifdef IS_PY3K` fallback was dead
  code.
- Fix a segfault in `type()` on any `aggdraw._aggdraw` object. The type objects
  were never passed to `PyType_Ready`, so they kept a NULL `ob_type`.
- Fix a segfault when a color is a non-ASCII string, e.g. `Pen("café")`. Such a
  color is now treated as unrecognized (black), like any other one aggdraw and
  Pillow cannot resolve.
- **Breaking:** `Font.family` and `Font.style` now return `str` instead of
  `bytes`. These are only reachable on the underlying `aggdraw._aggdraw.Font`
  object, not through the documented `aggdraw.Font` wrapper.
- **Breaking:** `bytes` are no longer accepted where a `str` is expected -- as a
  color, as text to draw, or as a PIL image's `mode`. `Pen(b"#ff0000")` used to
  render red and now renders black, since an unrecognized color silently becomes
  black; `Pen(b"black")` is unchanged only by coincidence.
- Setting an attribute on a `Pen` or `Brush` now raises `AttributeError` rather
  than `TypeError`, a side effect of the types being readied properly.
- Fix a memory leak in `Path.coords()`, which leaked one float object per
  coordinate on every call. `PyList_Append` takes its own reference, so the one
  returned by `PyFloat_FromDouble` was never released.
- Fix a memory leak of the `agg::trans_affine` set by `Draw.settransform()`.
  The transform was freed when replaced but never when the `Draw` itself was
  deallocated.
- Fix memory leaks on several constructor error paths, previously marked
  `FIXME`. A rejected `Symbol()` path descriptor, a `Font()` whose file cannot
  be loaded, and a `Draw()` whose image data is the wrong size all leaked the
  half-built object.
- Fix a window in `Draw(image)` where the object held a borrowed reference to
  the PIL image; the reference is now taken at the point of assignment.
- The dictionary backing the internal color-resolution helper is no longer
  leaked at import.
- `Pen`, `Brush`, `Font`, `Path`, `Symbol` and `Draw` in the C extension are now
  real classes rather than factory functions returning opaque objects. They are
  heap types built with `PyType_FromSpec`, exposed on `aggdraw._aggdraw` as
  types, and can be subclassed. `aggdraw._aggdraw` no longer exposes any
  module-level functions.
- New: `Path.from_svg(path, scale=1.0)` builds a path from an SVG-style path
  descriptor. This is the canonical replacement for `Symbol`. It is a
  classmethod, so calling it on a subclass returns an instance of that subclass.
- **Deprecated:** `aggdraw.Symbol`. Constructing one now emits a `UserWarning`;
  use `aggdraw.Path.from_svg()` instead. `UserWarning` rather than
  `DeprecationWarning` so that it is visible by default. See
  [#145](https://github.com/pytroll/aggdraw/issues/145).
- **Breaking:** `Symbol` is now a subclass of `Path` in both the Python wrapper
  and the C extension, rather than a factory function that returned a `Path`.
  `isinstance(Symbol(...), Path)` is now `True`; a `Symbol` has all the `Path`
  methods (`lineto`, `coords`, ...), where previously it had none; `Draw.line`
  and `Draw.polygon` now accept a `Symbol`, having rejected it with a
  `TypeError` from their `isinstance` guard; and `type()` of the underlying
  `aggdraw._aggdraw.Symbol` object is now `Symbol` rather than `Path`.
- New: `Pen.color`, `Pen.width` and `Brush.color` report the resolved color and
  width. The color is given as `(R, G, B, A)` after parsing, so an unrecognized
  color reads back as black.
- New: `Font.family`, `Font.style`, `Font.ascent` and `Font.descent` are now
  available on the documented `aggdraw.Font` wrapper, not only on the
  underlying C object.

## Version 1.4.1

- Fix point arrays not being deleted as arrays

## Version 1.4.0

- Add Python 3.14 wheels
- Add initial free-threading (3.14t) compatibility and wheels
- Drop Python 3.9 and 3.10 support
- Add `Draw.rounded_rectangle` method

## Version 1.3.19

- Add Python 3.13 wheels
- Remove Mac x86_64 wheels (pull requests welcome)
- Fix typo in documentation for tobytes usage

## Version 1.3.18

- Remove "register" keyword from AGG C++ for compiler compatibility

## Version 1.3.17

- Fix Python 3.12 compatibility

## Version 1.3.16

- Build changes to support for Python 3.12
- Drop 32-bit wheel builds

## Version 1.3.15

- Fix Python 3.10 compatibility
- Remove explicit support for Python <3.7

## Version 1.3.14

- Rebuild for missing Windows wheels

## Version 1.3.13

- Rebuild for python 3.10 wheels and switch to GitHub Actions

## Version 1.3.12

- Rebuild for python 3.8 wheels

## Version 1.3.11

- Force rebuild to fix freetype linking in OSX wheels

## Version 1.3.10

- Fix Draw.path docstring mentioning unused x/y coordinates
- Fix compilation on OSX 10.9+

## Version 1.3.9

- Add docstrings to public functions from original effbot documentation

## Version 1.3.8

- Force rebuild to get working linux wheels

## Version 1.3.7

- Add binary wheel building

## Version 1.3.6

- Fix Freetype linking on Linux with no freetype-config

## Version 1.3.5

- Fix Freetype linking on Windows by using ctypes

## Version 1.3.4

- Fix Freetype linking on certain systems [#27]

## Version 1.3.3

- Fix Windows compatibility [#25]

## Version 1.3.2

- Fix segmentation fault with certain compilers [#22]

## Version 1.3.1

- Fix Python 2 compatibility when getting RGB from string colors [#21]
- Re-add ability to get colors from PIL [#21]

## Version 1.3.0

- Python 3 support added
- Use freetype-config to find root freetype directory
- REVIVE THE PROJECT!

## Changes from release 1.1 to 1.2

(1.2a3 released)

- Fixed crash when using type() or help() on aggdraw objects.

- Fixed crash in Path() constructor.

- Fixed some build issues under recent GCC versions.  The compiler
  still issues more warnings than it should; I'll have to fix that
  in a future release.

(1.2a2 released)

- Changed 'expose' method to require keyword arguments.  You can
  use 'hwnd' to pass in a window handle, or 'hdc' to pass in a
  device context:

	dib.expose(hwnd=window)
	dib.expose(hdc=dc)

- Added 'clear' method.  By default, it fills the entire image to
  the original background color.  If you pass in a color name, it
  uses the given color instead.

(1.2a1 released)

- Added experimental 'Dib' support (based on code from the Python
  Imaging Library).  The 'Dib' factory is similar to 'Draw', but
  allows the drawing context to be copied to the display.

	dib = Dib("RGB", size, background)

	... draw ...

	dib.expose(hwnd=wnd)

- Fixed a couple of gcc compiler nits.

## Changes from release 1.0 to 1.1

(1.1 released)

- Fixed rendering of symbols containing nested polygons (broken in
  1.1b3).

- Added 'coords' method to the Path type.  This returns the current
  path as a polyline.  If the path consists of multiple path fragments,
  the return value is undefined. (experimental)

(1.1b3 released)

- The Windows installer now uses Freetype 2.1.10.  This seems to fix
  the issue with irregular baselines reported for some fonts.

- Performance: changes to how and when drawing adapters are created,
  and proper clipping in the rasterizer can result in massive speedups
  for some applications.

- Added experimental 'setantialias' method to the drawing context.
  Pass in 0 to disable antialiasing, 1 to enable it.  Antialiasing
  is enabled by default.

- Adjust the size of filled objects (including polygons) depending
  on the pen width.  If no pen is used, filled antialiased objects
  are expanded by a half pixel, to avoid banding.  If a pen is used,
  the objects are shrunk by a half pen width. (experimental)

(1.1b2 released; internal release only)

- Fixed background color bug for non-RGBA images.  The third
  argument to the Draw constructor now works properly for all
  modes.

- Fixed big resource leak in the Draw(im) constructor.  The alternate
  form (Draw(mode, size)) does not leak (reported by H�kan Karlsson).

- Added Path object.  Path objects can be used instead of coordinates
  with the 'line' and 'polygon' primitives.  Path objects can also be
  used as symbols.

(1.1b1 released)

- Use ImageColor.getrgb to resolve colors, if available.

(1.0 final released)
