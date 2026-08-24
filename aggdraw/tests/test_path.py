"""Tests for the Path class."""

import aggdraw
import pytest

from aggdraw.tests._helpers import WHITE, ink_count, to_image

# The Draw methods that accept a Path
DRAW_METHODS = ["line", "polygon", "symbol", "path"]


def test_path_init():
    # A path can be created empty...
    assert aggdraw.Path().coords() == []

    # ...or from a sequence of points, given as either a list or a tuple
    pts = [0, 0, 10, 0, 10, 10]
    from_list = aggdraw.Path(pts)
    from_tuple = aggdraw.Path(tuple(pts))
    assert len(from_list.coords()) == len(pts)
    assert from_list.coords() == from_tuple.coords()


def test_path_coords():
    # coords() returns a flat list of floats, even for a path built from
    # ints. Note that comparing against a list of floats doesn't check this
    # on its own, since 0.0 == 0.
    p = aggdraw.Path([0, 0, 10, 0])
    assert p.coords() == [0.0, 0.0, 10.0, 0.0]
    assert all(isinstance(c, float) for c in p.coords())

    # It also reflects points added after the path was created
    p.lineto(10, 10)
    assert p.coords() == [0.0, 0.0, 10.0, 0.0, 10.0, 10.0]


def test_path_close_connects_to_subpath_start():
    """close() connects to the current subpath's start, not the path's.

    The path below is drawn in two subpaths, the second started by the
    ``moveto(90, 20)``. Closing it therefore adds a diagonal running from
    (10, 90) back up to (90, 20), the start of that second subpath, rather
    than a vertical edge back to (10, 10) where the path itself began.

    Rendered by :func:`_render`, so the background is white and the path is
    drawn in black: a pixel equal to WHITE is blank and anything else is ink.
    """

    def build(close):
        p = aggdraw.Path()
        p.moveto(10, 10)
        p.lineto(90, 10)
        p.moveto(90, 20)
        p.lineto(90, 90)
        p.lineto(10, 90)
        if close:
            p.close()
        return p

    before = _render(build(False))
    after = _render(build(True))
    # Points along the expected diagonal are blank until the path is closed
    for x, y in [(21, 80), (44, 60), (67, 40)]:
        assert before.getpixel((x, y)) == WHITE
        assert after.getpixel((x, y)) != WHITE
    # Closing back to (10, 10) (the path's start) instead would draw a full
    # vertical edge down column 10. Only the endpoints of the two segments
    # that touch that column leave ink in it, near y=10 and y=90, so the
    # whole stretch between them stays blank.
    assert all(after.getpixel((10, y)) == WHITE for y in range(20, 80))


def test_path_close_erases_degenerate_subpath():
    # A subpath needs at least three distinct points to be closed. Closing one
    # with fewer removes it from the drawing entirely, taking the segments
    # already added by lineto with it. This is a "bug" in upstream AGG C++.
    p = aggdraw.Path([10.0, 10.0, 90.0, 10.0])
    coords = p.coords()
    assert ink_count(_render(p)) > 0

    p.close()
    # close() sets a flag rather than appending a vertex, so the coordinates
    # are unchanged and only the rendered pixels reveal what happened
    assert p.coords() == coords
    assert ink_count(_render(p)) == 0


def test_path_curveto():
    p = aggdraw.Path()
    p.moveto(5, 5)
    p.curveto(10, 10, 15, 0, 20, 5)
    pts = p.coords()
    # Check that bezier goes down, then up, then down
    assert pts[1] == pts[-1]
    assert pts[2] > pts[0]
    assert pts[3] > pts[1]
    assert pts[-1] > pts[-3]
    assert pts[-2] > pts[-4]
    assert pts[-1] == 5
    assert pts[-2] == 20


def test_path_lineto():
    p = aggdraw.Path()
    p.lineto(10, 15)
    p.lineto(5, 10)
    pts = p.coords()
    assert pts == [10, 15, 5, 10]


def test_path_moveto():
    # NOTE: lineto and moveto have same effect on p.coords() but are
    # still drawn differently
    p = aggdraw.Path()
    p.moveto(20, 20)
    assert p.coords() == [20, 20]


def test_path_rcurveto():
    absolute = aggdraw.Path()
    absolute.moveto(5, 5)
    absolute.curveto(10, 10, 15, 0, 20, 5)

    relative = aggdraw.Path()
    relative.moveto(5, 5)
    relative.rcurveto(5, 5, 10, -5, 15, 0)

    # The relative arguments describe the same curve as the absolute ones
    assert relative.coords() == absolute.coords()


def test_path_rlineto():
    p = aggdraw.Path()
    p.moveto(10, 10)
    p.rlineto(-5, 5)
    assert p.coords() == [10, 10, 5, 15]


def test_path_rmoveto():
    p = aggdraw.Path()
    p.moveto(10, 10)
    p.rlineto(10, 10)
    p.rmoveto(-5, 5)
    p.rlineto(10, 10)
    pts = p.coords()
    assert pts == [10, 10, 20, 20, 15, 25, 25, 35]


@pytest.mark.parametrize("method", DRAW_METHODS)
def test_path_draw(method):
    im = _draw_with(method, _sample_path(), aggdraw.Pen("black", width=1))
    assert ink_count(im) > 0
    # The top edge of the path is drawn (antialiased, so not pure black)
    assert im.getpixel((50, 10)) != WHITE
    # A point well away from the path is left alone
    assert im.getpixel((5, 5)) == WHITE


@pytest.mark.parametrize("method", DRAW_METHODS)
def test_path_draw_without_pen(method):
    # Drawing without a pen is allowed and simply draws nothing
    im = _draw_with(method, _sample_path())
    assert ink_count(im) == 0


def _render(path):
    """Draw a path in black on a white 100x100 surface.

    Drawing black on white means any pixel that isn't WHITE is part of the
    path. Antialiasing makes the edges gray rather than pure black, so tests
    check for "not the background" instead of for an exact ink color.
    """
    draw = aggdraw.Draw("RGB", (100, 100), "white")
    draw.path(path, aggdraw.Pen("black", 1))
    return to_image(draw)


def _draw_with(method, path, pen=None):
    """Draw a path on a fresh white surface with one of the Draw methods.

    ``symbol`` takes a position as well; (0, 0) draws the path where it was
    defined, matching what the other methods do.
    """
    draw = aggdraw.Draw("RGB", (100, 100), "white")
    if method == "symbol":
        draw.symbol((0, 0), path, pen)
    else:
        getattr(draw, method)(path, pen)
    return to_image(draw)


def _sample_path():
    p = aggdraw.Path()
    p.moveto(10, 10)
    p.lineto(90, 10)
    p.curveto(50, 50, 60, 40, 90, 90)
    p.rlineto(-80, 0)
    p.close()
    return p
