"""Tests for the Pen class."""

from aggdraw.tests._helpers import to_image


def test_pen():
    from aggdraw import Pen, Draw

    Pen("black")
    Pen("black", 1)
    Pen("black", width=1.5)
    Pen("black", 1, opacity=128)

    Pen(0)
    Pen((0, 0, 0))
    Pen("rgb(0,0,0)")
    Pen("gold")

    # Check that pens work as expected
    pens = {
        "black": Pen("black", 1),
        "red": Pen((255, 0, 0), 5),
        "black_50": Pen((0, 0, 0), 3, opacity=128),
        "crimson": Pen("#DC143C", 3),
    }
    surf = Draw("RGB", (50, 50), "white")
    surf.line((1, 1.5, 50, 1.5), pen=pens["black"])
    surf.line((1, 10.5, 50, 10.5), pen=pens["red"])
    surf.line((1, 20.5, 50, 20.5), pen=pens["black_50"])
    surf.line((1, 30.5, 50, 30.5), pen=pens["crimson"])
    im = to_image(surf)
    assert im.getpixel((1, 1)) == (0, 0, 0)
    assert im.getpixel((1, 10)) == (255, 0, 0)
    assert im.getpixel((1, 20)) == (127, 127, 127)
    assert im.getpixel((1, 30)) == (220, 20, 60)
    # Check widths of lines
    assert im.getpixel((1, 8)) == (255, 0, 0)
    assert im.getpixel((1, 12)) == (255, 0, 0)
    assert im.getpixel((1, 21)) == (127, 127, 127)
    # Check outside of lines to make sure it's white
    non_line = [(0, 1), (1, 0), (1, 2), (2, 7), (20, 13)]
    for loc in non_line:
        assert im.getpixel(loc) == (255, 255, 255)


def test_graphics3():
    """See issue #22."""
    from aggdraw import Draw, Pen
    from PIL import Image

    main = Image.new("RGB", (480, 1024), "white")
    d = Draw(main)
    p = Pen((90,) * 3, 0.5)


def test_pen_type_is_readable():
    """The C type objects must be readied at import; see issue in _aggdraw.cxx.

    Before the Python 2 removal the type objects were never passed to
    PyType_Ready, leaving ob_type NULL, so type() on one segfaulted.
    """
    from aggdraw import Pen

    assert type(Pen("black")._pen).__name__ == "Pen"


def test_pen_non_ascii_color():
    """A non-ASCII color name must not crash; it falls back to black.

    getcolor() used to reach strcmp() with a NULL pointer whenever
    PyUnicode_AsASCIIString failed on the color string.
    """
    from aggdraw import Draw, Brush

    surf = Draw("RGB", (10, 10), "white")
    surf.rectangle((0, 0, 9, 9), Brush("café"))
    assert to_image(surf).getpixel((5, 5)) == (0, 0, 0)


def test_pen_exposes_color_and_width():
    """Pen.color reports the resolved color, Pen.width the pen width.

    Neither was reachable from Python before the C types became real classes.
    """
    from aggdraw import Pen

    pen = Pen("crimson", 2.5)
    assert pen.color == (220, 20, 60, 255)
    assert pen.width == 2.5

    # a 4-tuple overrides opacity entirely
    assert Pen((1, 2, 3, 4), opacity=200).color == (1, 2, 3, 4)
    # an unrecognized color silently becomes black
    assert Pen("not a color").color == (0, 0, 0, 255)


def test_pen_is_subclassable():
    """The C types set Py_TPFLAGS_BASETYPE and are usable as base classes.

    The dispatcher inside draw_adaptor::draw checks with PyObject_TypeCheck,
    so a subclass must still be recognized as a pen.
    """
    from aggdraw import Draw, _aggdraw
    from PIL import Image

    class MyPen(_aggdraw.Pen):
        pass

    assert isinstance(MyPen("red", 1), _aggdraw.Pen)

    surf = _aggdraw.Draw("RGB", (20, 20), "white")
    surf.line((0, 5.5, 20, 5.5), MyPen("red", 1))
    im = Image.frombytes(surf.mode, surf.size, surf.tobytes())
    assert im.getpixel((10, 5)) == (255, 0, 0)
