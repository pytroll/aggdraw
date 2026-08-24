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
