"""Tests for the aggdraw package itself."""


def test_module_init():
    import aggdraw

    assert hasattr(aggdraw, "VERSION")
    assert isinstance(aggdraw.VERSION, str)
    assert hasattr(aggdraw, "__version__")
    assert isinstance(aggdraw.__version__, str)
