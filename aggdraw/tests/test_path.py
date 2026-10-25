"""Tests for the Path class."""

import sys

import aggdraw
import pytest

from aggdraw import _aggdraw
from aggdraw.tests._helpers import WHITE, draw_with, ink_count, render

# The Draw methods that accept a Path
DRAW_METHODS = ["line", "polygon", "symbol", "path"]

# One descriptor per operator the SVG parser supports, in both the absolute and
# the relative spelling, with the coordinates the parser should produce. All of
# them start at (10, 10) so the relative and absolute forms are comparable.
SVG_OPERATORS = [
    ("M10,10 L20,20", [10, 10, 20, 20]),
    ("m10,10 l10,10", [10, 10, 20, 20]),
    ("M10,10 H20", [10, 10, 20, 10]),
    ("m10,10 h10", [10, 10, 20, 10]),
    ("M10,10 V20", [10, 10, 10, 20]),
    ("m10,10 v10", [10, 10, 10, 20]),
]


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

    Rendered by :func:`render`, so the background is white and the path is
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

    before = render(build(False))
    after = render(build(True))
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
    assert ink_count(render(p)) > 0

    p.close()
    # close() sets a flag rather than appending a vertex, so the coordinates
    # are unchanged and only the rendered pixels reveal what happened
    assert p.coords() == coords
    assert ink_count(render(p)) == 0


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


@pytest.mark.parametrize(("descriptor", "expected"), SVG_OPERATORS)
def test_path_from_svg_operators(descriptor, expected):
    # Each operator produces the coordinates it says it does, and the relative
    # (lower-case) spelling matches the absolute one.
    assert aggdraw.Path.from_svg(descriptor).coords() == expected


def test_path_from_svg_matches_manual_path():
    # A descriptor and the equivalent hand-built path are the same path, both
    # in coordinates and in the pixels they paint.
    from_svg = aggdraw.Path.from_svg("M10,10 L90,10 L90,90 Z")
    manual = aggdraw.Path()
    manual.moveto(10, 10)
    manual.lineto(90, 10)
    manual.lineto(90, 90)
    manual.close()

    assert from_svg.coords() == manual.coords()
    assert render(from_svg).tobytes() == render(manual).tobytes()


def test_path_from_svg_curves_are_flattened():
    # Curves are expanded to line segments while parsing, so coords() returns
    # more points than the descriptor names.
    p = aggdraw.Path.from_svg("M10,10 C20,20 30,20 40,10")
    pts = p.coords()
    assert len(pts) > 8
    assert pts[:2] == [10, 10]
    assert pts[-2:] == [40, 10]


def test_path_from_svg_scale():
    # scale multiplies every coordinate as the descriptor is parsed.
    assert aggdraw.Path.from_svg("M10,10 L20,10", 2).coords() == [20, 20, 40, 20]
    # ...and it is applied before rendering, not after, so a scaled path paints
    # the same pixels as the equivalent unscaled one.
    scaled = aggdraw.Path.from_svg("M5,5 L45,5 L45,45 Z", 2)
    manual = aggdraw.Path.from_svg("M10,10 L90,10 L90,90 Z")
    assert render(scaled).tobytes() == render(manual).tobytes()


def test_path_from_svg_returns_a_usable_path():
    # The result is an ordinary Path, not an opaque handle: it can be extended
    # and the extension is drawn.
    p = aggdraw.Path.from_svg("M10,10 L90,10")
    before = ink_count(render(p))
    p.lineto(90, 90)
    assert p.coords() == [10, 10, 90, 10, 90, 90]
    assert ink_count(render(p)) > before


def test_path_from_svg_does_not_warn(recwarn):
    # Only the deprecated Symbol spelling warns; the replacement must not.
    aggdraw.Path.from_svg("M0,0 L10,10")
    assert len(recwarn) == 0


@pytest.mark.parametrize(
    ("descriptor", "message"),
    [
        ("0,0", "no command at start of path"),
        ("A10,10", "unknown path command 'A'"),
        ("M-", "invalid arguments for command 'M'"),
    ],
)
def test_path_from_svg_invalid_descriptor(descriptor, message):
    with pytest.raises(ValueError, match=message):
        aggdraw.Path.from_svg(descriptor)


def test_path_from_svg_on_a_subclass():
    # from_svg is a classmethod, so it builds an instance of whatever class it
    # was called on -- at both layers.
    class MyPath(aggdraw.Path):
        pass

    assert type(MyPath.from_svg("M0,0 L1,1")) is MyPath

    class MyCPath(_aggdraw.Path):
        pass

    assert type(MyCPath.from_svg("M0,0 L1,1")) is MyCPath
    # The deprecated C Symbol type inherits it too, and stays a Symbol.
    assert type(_aggdraw.Symbol.from_svg("M0,0 L1,1")) is _aggdraw.Symbol


def test_path_from_svg_rejects_a_foreign_class():
    # The classmethod descriptor guards this for us; check it really does,
    # since path_from_svg_impl allocates from whatever it is handed.
    with pytest.raises(TypeError, match="requires a subtype"):
        _aggdraw.Path.__dict__["from_svg"].__get__(None, int)


@pytest.mark.parametrize("method", DRAW_METHODS)
def test_path_draw(method):
    im = draw_with(method, _sample_path(), aggdraw.Pen("black", width=1))
    assert ink_count(im) > 0
    # The top edge of the path is drawn (antialiased, so not pure black)
    assert im.getpixel((50, 10)) != WHITE
    # A point well away from the path is left alone
    assert im.getpixel((5, 5)) == WHITE


@pytest.mark.parametrize("method", DRAW_METHODS)
def test_path_draw_without_pen(method):
    # Drawing without a pen is allowed and simply draws nothing
    im = draw_with(method, _sample_path())
    assert ink_count(im) == 0


def _sample_path():
    p = aggdraw.Path()
    p.moveto(10, 10)
    p.lineto(90, 10)
    p.curveto(50, 50, 60, 40, 90, 90)
    p.rlineto(-80, 0)
    p.close()
    return p


def test_coords_does_not_leak():
    """coords() must release the floats it appends to the result list.

    PyList_Append takes its own reference, so the one returned by
    PyFloat_FromDouble used to leak -- one object per coordinate, per call.
    """
    if not sys.getallocatedblocks():
        pytest.skip("needs pymalloc to count allocated blocks")

    path = aggdraw.Path([0, 0, 10, 10, 20, 5, 30, 30])
    for _ in range(100):  # let any one-time caches settle
        path.coords()

    before = sys.getallocatedblocks()
    for _ in range(2000):
        path.coords()
    growth = sys.getallocatedblocks() - before

    # 8 coordinates per call: a leak shows up as ~16000 blocks, not a handful.
    assert growth < 100, f"coords() leaked {growth} blocks over 2000 calls"


@pytest.mark.parametrize("factory", [aggdraw.Path.from_svg, _aggdraw.Symbol])
def test_from_svg_error_paths_do_not_leak(factory):
    """A rejected path descriptor must release the half-built object.

    The parser's error paths used to be marked "FIXME: cleanup" and returned
    without freeing the object or its agg::path_storage. Both entry points
    share one C helper, but they allocate from different types, so both
    deallocation paths are worth measuring.

    Do not route this through ``aggdraw.Symbol``: it emits a deprecation
    warning, and under ``-W always`` each of the 2200 calls would record a
    WarningMessage, adding thousands of blocks and swamping the measurement.
    """
    if not sys.getallocatedblocks():
        pytest.skip("needs pymalloc to count allocated blocks")

    def build():
        # plain try/except, not pytest.raises: ExceptionInfo objects accumulate
        # and would swamp the measurement.
        try:
            factory("M-")
        except ValueError:
            return
        raise AssertionError("expected ValueError")

    for _ in range(200):  # let any one-time caches settle
        build()

    before = sys.getallocatedblocks()
    for _ in range(2000):
        build()
    growth = sys.getallocatedblocks() - before

    assert growth < 100, f"failed parse leaked {growth} blocks over 2000 calls"
