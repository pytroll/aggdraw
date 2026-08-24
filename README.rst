==================
The aggdraw module
==================

.. image:: https://github.com/pytroll/aggdraw/actions/workflows/ci.yml/badge.svg?branch=main
    :target: https://github.com/pytroll/aggdraw/actions?query=workflow%3A%22CI%22

A high-quality graphics engine for PIL, based on Maxim Shemanarev's
Anti-Grain Geometry library (from http://antigrain.com).
The aggdraw module implements the basic WCK 2D Drawing Interface on
top of the AGG library. This library provides high-quality drawing,
with anti-aliasing and alpha compositing, while being fully compatible
with the WCK renderer.

The necessary AGG sources are included in the aggdraw source kit.

For posterity, reference
`the old documentation <https://web.archive.org/web/20190308154642/http://effbot.org/zone/aggdraw-index.htm>`_.
Note that this archived page is out of date with the current version of the
library and describes behaviour that has since changed. Prefer the current
documentation at https://aggdraw.readthedocs.io/en/stable/.

Build instructions (all platforms)
----------------------------------

1. Check prerequisites.

   You need a C++ compiler to build this extension.

   The library comes with the necessary AGG sources included.

   The following additional libraries can be used:

   * OpenType/TrueType support - freetype2 (2.1.10 or later is recommended)
     See http://www.freetype.org and http://freetype.sourceforge.net for details.

2. Configure.

   FreeType is optional. Without it aggdraw builds and draws normally, but
   ``Font`` cannot be created and the ``Draw.text`` and ``Draw.textsize``
   methods are not compiled into the extension at all.

   To enable freetype, install it (including its development headers) and
   let ``setup.py`` locate it. It looks in the following places, in order,
   and uses the first that succeeds:

   1. The ``AGGDRAW_FREETYPE_ROOT`` environment variable, if set.
   2. The ``freetype-config --prefix`` command, if ``freetype-config`` is on
      your PATH. Note this command is deprecated upstream and is absent from
      most modern freetype installations.
   3. ``ctypes.util.find_library('freetype')``.
   4. ``pkgconfig.variables('freetype2')``.

   If autodetection picks the wrong installation, or finds one you do not
   want used, set the environment variable explicitly::

        $ AGGDRAW_FREETYPE_ROOT=/usr python -m pip install .

   Setting it to an empty string is the supported way to deliberately build
   *without* freetype; autodetection is skipped entirely. This is how the
   Windows wheels are built.

   The build reports which of the three outcomes applied::

        === freetype found: '/usr'
        === freetype disabled by AGGDRAW_FREETYPE_ROOT
        === freetype not available

3. Build and Install

   The library uses a standard setup.py file. Install the library
   using ``pip`` from the root of the aggdraw repository::

        $ python -m pip install .

   Alternatively, it is possible to install the library in an "editable"
   manner where the python environment will point to the local development
   aggdraw directory.

   ::

        $ python -m pip install -e .

   However, since aggdraw depends on compiling extension code, it must be
   re-installed to re-build the extension.

4. Once aggdraw is installed run the tests::

        $ pytest -v aggdraw/tests

   To test an installed copy of aggdraw from outside a source checkout, run
   the tests that ship inside the package instead::

        $ pytest -v --pyargs aggdraw.tests

5. Enjoy!

Free-threading support
----------------------

See the documentation site for current information for free-threading
support: https://aggdraw.readthedocs.io/en/stable/

AGG2 License
------------

Anti-Grain Geometry - Version 2.0
Copyright (c) 2002 Maxim Shemanarev (McSeem)

Permission to copy, use, modify, sell and distribute this software
is granted provided this copyright notice appears in all copies.
This software is provided "as is" without express or implied
warranty, and with no claim as to its suitability for any purpose.

AggDraw License
---------------

The aggdraw interface, and associated modules and documentation are:

Copyright (c) 2011-2017 by AGGDraw Developers
Copyright (c) 2003-2006 by Secret Labs AB
Copyright (c) 2003-2006 by Fredrik Lundh

By obtaining, using, and/or copying this software and/or its
associated documentation, you agree that you have read, understood,
and will comply with the following terms and conditions:

Permission to use, copy, modify, and distribute this software and its
associated documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies, and that both that copyright notice and this permission notice
appear in supporting documentation, and that the name of Secret Labs
AB or the author not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Additional Patches
------------------

The AGG C++ vendored source code in this repository is no longer compatible
with some modern compilers and coding styles. The aggdraw project has had to
apply additional patches over time to fix compatibility or to retain backwards
compatibility with previous versions of AGG to get the same end result. Some
patches may be documented in README files, but all future patches should appear
in the ``patches/`` directory in the root of this repository and were applied with
commands such as ``patch -p0 patches/tags_pointer_type_fix.patch``.
