# $Id$
# -*- coding: iso-8859-1 -*-
# sanity check

import numpy as np
from PIL import Image
import aggdraw
from aggdraw import Draw, Symbol, Brush, Pen, Path


def test_module_init():
    """

    >>> assert hasattr(aggdraw, 'VERSION')
    >>> assert isinstance(aggdraw.VERSION, str)
    >>> assert hasattr(aggdraw, '__version__')
    >>> assert isinstance(aggdraw.__version__, str)

    """


def test_draw():
    """

    >>> draw = Draw("RGB")
    Traceback (most recent call last):
    AttributeError: 'str' object has no attribute 'mode'

    >>> draw = Draw("RGB", (800, 600))
    >>> draw.mode, draw.size
    ('RGB', (800, 600))

    >>> draw = Draw("RGB", (800, 600), "white")
    >>> draw.mode, draw.size
    ('RGB', (800, 600))

    >>> im = Image.new("RGB", (600, 800))
    >>> draw = Draw(im)
    >>> draw.mode, draw.size
    ('RGB', (600, 800))


    """

def test_flush():
    """

    >>> im = Image.new("RGB", (600, 800))
    >>> draw = Draw(im)
    >>> draw.flush().mode
    'RGB'
    """


def test_pen():
    """

    >>> pen = Pen("black")
    >>> pen = Pen("black", 1)
    >>> pen = Pen("black", 1.5)
    >>> pen = Pen("black", 1, opacity=128)

    >>> pen = Pen(0)
    >>> pen = Pen((0,0,0))
    >>> pen = Pen("rgb(0,0,0)")
    >>> pen = Pen("gold")

    """


def test_brush():
    """

    >>> brush = Brush("black")
    >>> brush = Brush("black", opacity=128)

    >>> brush = Brush(0)
    >>> brush = Brush((0,0,0))
    >>> brush = Brush("rgb(0,0,0)")
    >>> brush = Brush("gold")

    """


def test_graphics():
    """

    >>> draw = Draw("RGB", (500, 500))

    >>> pen = Pen("black")
    >>> brush = Brush("black")

    >>> draw.line((50, 50, 100, 100), pen)

    >>> draw.rectangle((50, 150, 100, 200), pen)
    >>> draw.rectangle((50, 220, 100, 270), brush)
    >>> draw.rectangle((50, 290, 100, 340), brush, pen)
    >>> draw.rectangle((50, 360, 100, 410), pen, brush)

    >>> draw.ellipse((120, 150, 170, 200), pen)
    >>> draw.ellipse((120, 220, 170, 270), brush)
    >>> draw.ellipse((120, 290, 170, 340), brush, pen)
    >>> draw.ellipse((120, 360, 170, 410), pen, brush)

    >>> draw.polygon((190+25, 150, 190, 200, 190+50, 200), pen)
    >>> draw.polygon((190+25, 220, 190, 270, 190+50, 270), brush)
    >>> draw.polygon((190+25, 290, 190, 340, 190+50, 340), brush, pen)
    >>> draw.polygon((190+25, 360, 190, 410, 190+50, 410), pen, brush)

    """


def test_graphics2():
    """See issue #14

    >>> symbol = Symbol("M400 200 L400 400")
    >>> pen = Pen("red")
    >>> image = Image.fromarray(np.zeros((800, 600, 3)), mode="RGB")
    >>> canvas = Draw(image)
    >>> canvas.symbol((0, 0), symbol, pen)
    >>> image_pointer = canvas.flush()
    >>> assert np.asarray(image).sum() == 50800

    """


def test_graphics3():
    """See issue #22

    >>> main = Image.new('RGB', (480, 1024), 'white')
    >>> d = aggdraw.Draw(main)
    >>> p = aggdraw.Pen((90,) * 3, 0.5)

    """


def test_path():
    """

    >>> p = Path()
    >>> p = Path([0,0])
    >>> p = Path([0,0,0,0])

    >>> p = Path()
    >>> p.moveto(0, 0)
    >>> p.lineto(1, 1)
    >>> p.coords()
    [0.0, 0.0, 1.0, 1.0]

    >>> p.curveto(0, 0, 0, 0, 0, 0)
    >>> p.close()
    >>> p.coords()
    [0.0, 0.0, 1.0, 1.0, 0.125, 0.125, 0.0, 0.0]

    >>> draw = Draw("RGB", (800, 600))
    >>> draw.line(p)
    >>> draw.polygon(p)
    >>> draw.symbol((0, 0), p)

    """


def test_symbol():
    """

    >>> s = Symbol("M0,0L0,0L0,0L0,0Z")
    >>> s = Symbol("M0,0L0,0,0,0,0,0Z", 10)
    >>> s = Symbol("M0,0C0,0,0,0,0,0Z")
    >>> s = Symbol("M0,0S0,0,0,0,0,0Z")

    >>> s = Symbol("m0,0l0,0l0,0l0,0z")
    >>> s = Symbol("m0,0l0,0,0,0,0,0z", 10)
    >>> s = Symbol("m0,0c0,0,0,0,0,0z")
    >>> s = Symbol("m0,0s0,0,0,0,0,0z")

    """


def test_transform():
    """

    >>> draw = Draw("RGB", (500, 500))

    >>> draw.settransform()
    >>> draw.settransform((250, 250))
    >>> draw.settransform((1, 0, 250, 0, 1, 250))
    >>> draw.settransform((2.0, 0.5, 250, 0.5, 2.0, 250))
    >>> draw.settransform()

    """


if __name__ == "__main__":
    # use doctest to make sure the test program behaves as documented!
    import sys, doctest, selftest
    status = doctest.testmod(selftest)
    if status[0]:
        print("*** %s tests of %d failed." % status)
    else:
        print("%s tests passed." % status[1])
    sys.exit(int(status[0]))
