import platform

import aggdraw._aggdraw as _aggdraw
from .core import Draw


class Dib(Draw):
    """Creates a drawing interface object that can be copied to a window.

    Windows-only.
    
    This object has the same methods as Draw, plus an expose method that copies
    the contents to a given window or device context.

    Args:
        mode (str): The pixel mode of the draw surface. Currently only supports
            "RGB".
        size (tuple): The size (width, height) of the drawing surface in pixels.
        color (tuple, optional): A default background fill color for the surface.

    """
    def __init__(self, mode, size, color=None):
        if platform.system() != "Windows":
            e = "Dib class only available on Windows."
            raise RuntimeError(e)
        self._draw = _aggdraw.Dib(mode, size, color)

    def expose(self, hwnd=0, hwc=0):
        """Copies the contents of the drawing object to the given window or context.
        
        You must provide either a hwnd or a hdc keyword argument.

        Args:
            hwnd (int): An HWND handle cast to an integer.
            hdc (int): An HDC handle cast to an integer.

        """
        self._draw.expose(hwnd, hwc)
