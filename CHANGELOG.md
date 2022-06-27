# The aggdraw Library

## Version 1.3.15

- Fix Python 3.10 compatibility

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
