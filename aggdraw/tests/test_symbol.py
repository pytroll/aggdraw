"""Tests for the deprecated Symbol class.

Symbol is a deprecated subclass of Path kept for backwards compatibility; the
SVG parser itself is tested through Path.from_svg in test_path.py. What is
tested here is the deprecation and the subclass relationship.
"""

import aggdraw
import pytest

from aggdraw import _aggdraw
from aggdraw.tests._helpers import WHITE, draw_with, ink_count, render

DEPRECATED = "Symbol is deprecated"

# A closed triangle, used wherever a Symbol and a Path.from_svg path are
# compared: it has enough distinct edges to tell the two renderings apart.
DESCRIPTOR = "M10,10 L90,10 L90,90 Z"

# The Draw methods that accept a Path, and therefore a Symbol
DRAW_METHODS = ["line", "polygon", "symbol", "path"]


@pytest.mark.parametrize(
    "construct",
    [
        pytest.param(lambda: aggdraw.Symbol("M0,0 L10,10"), id="positional"),
        pytest.param(lambda: aggdraw.Symbol(path="M0,0 L10,10", scale=2), id="keyword"),
        pytest.param(lambda: aggdraw.Symbol.from_svg("M0,0 L10,10"), id="inherited-from_svg"),
    ],
)
def test_symbol_construction_is_deprecated(construct):
    """Every door into Symbol warns.

    The warning lives in ``__new__`` rather than ``__init__`` so that the
    classmethod inherited from Path -- which calls ``cls.__new__`` and skips
    ``__init__`` -- warns as well.
    """
    with pytest.warns(UserWarning, match=DEPRECATED):
        construct()


def test_symbol_is_an_equivalent_path():
    # Symbol is a Path at both layers...
    assert issubclass(aggdraw.Symbol, aggdraw.Path)
    assert issubclass(_aggdraw.Symbol, _aggdraw.Path)

    with pytest.warns(UserWarning, match=DEPRECATED):
        sym = aggdraw.Symbol(DESCRIPTOR)
    path = aggdraw.Path.from_svg(DESCRIPTOR)

    assert isinstance(sym, aggdraw.Path)
    # ...and the wrapper holds a C Symbol, which is itself a C Path
    assert type(sym._path).__name__ == "Symbol"

    # Symbol and Path.from_svg are two spellings of the same C parser, so they
    # must produce the same geometry and the same pixels.
    assert sym.coords() == path.coords()
    assert render(sym).tobytes() == render(path).tobytes()


def test_symbol_inherits_path_methods():
    # Symbol used to define no methods at all, so this raised AttributeError
    # even though the underlying C object supported it.
    with pytest.warns(UserWarning, match=DEPRECATED):
        sym = aggdraw.Symbol("M10,10 L90,10")

    before = ink_count(render(sym))
    sym.lineto(90, 90)
    assert sym.coords() == [10, 10, 90, 10, 90, 90]
    assert ink_count(render(sym)) > before


@pytest.mark.parametrize("method", DRAW_METHODS)
def test_draw_methods_accept_a_symbol(method):
    """Every Draw method that takes a Path also takes a Symbol.

    Draw.line and Draw.polygon guard on ``isinstance(xy, Path)``. A Symbol used
    to fail that guard and reach the C layer as an unwrapped wrapper object,
    raising TypeError; as a Path subclass it is now drawn like any other path.
    """
    with pytest.warns(UserWarning, match=DEPRECATED):
        sym = aggdraw.Symbol(DESCRIPTOR)

    im = draw_with(method, sym, aggdraw.Pen("black", 1))
    # The top edge of the triangle is drawn (antialiased, so not pure black)
    assert im.getpixel((50, 10)) != WHITE
    # A point well away from it is left alone
    assert im.getpixel((5, 5)) == WHITE


def test_symbol_can_be_subclassed():
    class Marker(aggdraw.Symbol):
        pass

    with pytest.warns(UserWarning, match=DEPRECATED):
        marker = Marker("M0,0 L10,10")

    assert type(marker) is Marker
    assert isinstance(marker, aggdraw.Path)
    assert marker.coords() == [0, 0, 10, 10]

    # ...and so can the C type, which must allocate from the subclass
    class CMarker(_aggdraw.Symbol):
        pass

    c_marker = CMarker("M0,0 L10,10")
    assert type(c_marker) is CMarker
    assert c_marker.coords() == [0, 0, 10, 10]


def test_c_symbol_rejects_keyword_arguments():
    # As a module-level function this was rejected for free; as a tp_new the
    # keywords have to be turned away explicitly or scale is silently ignored.
    with pytest.raises(TypeError, match="no keyword arguments"):
        _aggdraw.Symbol("M0,0 L10,10", scale=2)


def test_graphics2():
    """See issue #14."""
    from PIL import Image
    import numpy as np

    def render_large(path):
        image = Image.fromarray(np.zeros((800, 600, 3), dtype=np.uint8), mode="RGB")
        canvas = aggdraw.Draw(image)
        canvas.symbol((0, 0), path, aggdraw.Pen("red"))
        canvas.flush()
        return np.asarray(image).sum()

    with pytest.warns(UserWarning, match=DEPRECATED):
        symbol = aggdraw.Symbol("M400 200 L400 400")

    # Asserted for Path.from_svg too, so the regression outlives Symbol
    assert render_large(symbol) == 50800
    assert render_large(aggdraw.Path.from_svg("M400 200 L400 400")) == 50800
