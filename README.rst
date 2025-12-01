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

For posterity, reference `the old documentation <http://www.effbot.org/zone/aggdraw.htm>`_.

Build instructions (all platforms)
----------------------------------

1. Check prerequisites.

   You need a C++ compiler to build this extension.

   The library comes with the necessary AGG sources included.

   The following additional libraries can be used:

   * OpenType/TrueType support - freetype2 (2.1.10 or later is recommended)
     See http://www.freetype.org and http://freetype.sourceforge.net for details.

2. Configure.

   To enable freetype, you need to build the library somewhere and
   make sure the `freetype-config` command is available on your PATH. The
   setup.py file will call `freetype-config --prefix` to locate
   all of the necessary libraries and headers as part of installation.

3. Build and Install

   The library uses a standard setup.py file. Install the library
   using ``pip`` from the root of the aggdraw repository::

        $ python -m pip3 install .

   Alternatively, it is possible to install the library in an "editable"
   manner where the python environment will point to the local development
   aggdraw directory.

   ::

        $ python -m pip3 install -e .

   However, since aggdraw depends on compiling extension code, it must be
   re-installed to re-build the extension.

4. Once aggdraw is installed run the tests::

        $ python selftest.py

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
