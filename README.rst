==================
The aggdraw module
==================

.. image:: https://github.com/pytroll/aggdraw/workflows/CI/badge.svg?branch=main
    :target: https://github.com/pytroll/aggdraw/actions?query=workflow%3A%22CI%22

A high-quality graphics engine for PIL, based on Maxim Shemanarev's
Anti-Grain Geometry library (from http://antigrain.com).

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
   all of the necessary libraries and headers.

3. Build.

   The library uses a standard setup.py file, and you can use all
   standard setup.py commands.   I recommend the following steps::

        $ python setup.py build_ext -i
        $ python selftest.py

   (if you're lazy, you can skip the above and just install the
   library; setup.py will make sure the right stuff is built before
   it's installed).

4. Install.

   If the selftest succeeds, you can install the library::

        $ python setup.py install

5. Enjoy!

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
