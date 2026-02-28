import aggdraw._aggdraw as _aggdraw


class Brush():
    """Creates a brush object.

    The brush color can be an RGB tuple (e.g. `(255, 255, 255)`), a CSS-style color
    name, or a color integer (0xAARRGGBB).
    
    Args:
        color: The brush color.
        opacity (int, optional): The opacity of the brush (from 0 to 255). Defaults to
            a solid brush.

    """
    def __init__(self, color, opacity=255):
        self._brush = _aggdraw.Brush(color, opacity)


class Pen():
    """Creates a pen object.

    The pen color can be a color tuple (e.g. `(255, 255, 255)`), a CSS-style color
    name, or a color integer (0xAARRGGBB).
    
    Args:
        color: The pen color.
        width (int, optional): The width of the pen.
        opacity (int, optional): The opacity of the pen (from 0 to 255). Defaults to
            a solid pen.

    """
    def __init__(self, color, width=1, opacity=255):
        self._pen = _aggdraw.Pen(color, width, opacity)


class Font():
    """Creates a font object.

    This creates a font object for use with :meth:`aggdraw.Draw.text` and
    :meth:`aggdraw.Draw.textsize` from a TrueType font file.

    The font color can be a color tuple (e.g. `(255, 255, 255)`), a CSS-style color
    name, or a color integer (0xAARRGGBB).
    
    Args:
        color: The font color.
        file: Path to a valid TrueType font file.
        size (optional): The font size (in pixels). Defaults to 12.
        opacity (int, optional): The opacity of the font (from 0 to 255). Defaults
            to solid.

    """
    def __init__(self, color, file, size=12, opacity=255):
        # NOTE: Only available if compiled with FreeType support
        self._font = _aggdraw.Font(color, file, size, opacity)


class Symbol():
    """Symbol factory.

    This creates a symbol object from an SVG-style path descriptor for use with
    :meth:`aggdraw.Draw.symbol`.

    The following operators are supported:
     * M (move)
     * L (line)
     * H (horizontal line)
     * V (vertical line)
     * C (cubic bezier)
     * S (smooth cubic bezier)
     * Q (quadratic bezier)
     * T (smooth quadratic bezier)
     * Z (close path)

    Use lower-case operators for relative coordinates, upper-case for absolute
    coordinates.
    
    Args:
        path (str): An SVG-style path descriptor. 

    """
    def __init__(self, path, scale=1.0):
        # NOTE: 'scale' param is undocumented
        self._path = _aggdraw.Symbol(path, scale)


class Path():
    """Path factory.

    This creates a path object for use with :meth:`aggdraw.Draw.path`.

    """
    def __init__(self, path=None):
        # NOTE: 'path' param is undocumented but defines a initial set
        # of points to connect with lines
        if path:
            self._path = _aggdraw.Path(path)
        else:
            self._path = _aggdraw.Path()

    def close(self):
        """Closes the current path."""
        self._path.close()

    def coords(self):
        """Returns the coordinates for the path.

        Curves are flattened before being returned.

        Returns:
            list: A sequence in (x, y, x, y, ...) format.

        """
        return self._path.coords()

    def curveto(self, x1, y1, x2, y2, x, y):
        """Adds a bezier curve segment to the path."""
        self._path.curveto(x1, y1, x2, y2, x, y)

    def lineto(self, x, y):
        """Adds a line segment to the path."""
        self._path.lineto(x, y)

    def moveto(self, x, y):
        """Moves the path pointer to the given location."""
        self._path.moveto(x, y)

    def rcurveto(self, x1, y1, x2, y2, x, y):
        """Adds a bezier curve segment to the path using relative coordinates.
        
        Same as :meth:`~curveto`, but the coordinates are relative to the current
        position.
        
        """
        self._path.rcurveto(x1, y1, x2, y2, x, y)

    def rlineto(self, x, y):
        """Adds a line segment to the path using relative coordinates.
        
        Same as :meth:`~lineto`, but the coordinates are relative to the current
        position.
        
        """
        self._path.rlineto(x, y)

    def rmoveto(self, x, y):
        """Moves the path pointer relative to the current position."""
        self._path.rmoveto(x, y)


class Draw():
    """Creates a drawing interface object.
    
    The constructor can either take a PIL Image object, or mode and size specifiers.

    Examples::
       d = aggdraw.Draw(im)
       d = aggdraw.Draw("RGB", (800, 600), "white")

    Args:
        image_or_mode: A PIL image or a mode string. The following modes are
            supported: “L”, “RGB”, “RGBA”, “BGR”, “BGRA”.
        size (tuple, optional): The size of the image (width, height).
        color (optional): An optional background color. If omitted, defaults
            to white with full alpha.

    """
    def __init__(self, image_or_mode, size=None, color="white"):
        if size:
            self._draw = _aggdraw.Draw(image_or_mode, size, color)
        else:
            self._draw = _aggdraw.Draw(image_or_mode)

    @property
    def size(self):
        """tuple: The size in pixels of the draw surface (width, height)."""
        return self._draw.size

    @property
    def mode(self):
        """str: The mode of the draw surface (e.g. "RGBA")."""
        return self._draw.mode

    def _parse_args(self, brush=None, pen=None):
        # Allow order of brush and pen to be reversed, matching C++ API
        # NOTE: This segfaults for some reason if pen and brush are swapped here
        # instead of leaving it to the C++ extension to handle
        if brush:
            brush = brush._pen if isinstance(brush, Pen) else brush._brush
        if pen:
            pen = pen._brush if isinstance(pen, Brush) else pen._pen
        return (brush, pen)

    def arc(self, xy, start, end, pen=None):
        """Draws an arc.

        Args:
            xy: A 4-element Python sequence (x, y, x, y) with the upper-left corner
                given first.
            start (float): The start angle of the arc.
            end (float): The end angle of the arc.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing the arc.

        """
        # NOTE: Why is pen optional?
        if pen:
            pen = pen._pen
        self._draw.arc(xy, start, end, pen)

    def chord(self, xy, start, end, pen=None, brush=None):
        """Draws a chord.
        
        If a brush is given, it is used to fill the chord. If a pen is given,
        it is used to draw an outline around the chord. Either one (or both)
        can be left out.

        Args:
            xy: A 4-element Python sequence (x, y, x, y) with the upper-left corner
                given first.
            start (float): The start angle of the chord.
            end (float): The end angle of the chord.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the chord.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the chord.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.chord(xy, start, end, pen, brush)

    def ellipse(self, xy, pen=None, brush=None):
        """Draws an ellipse.
        
        If a brush is given, it is used to fill the ellipse. If a pen is given,
        it is used to draw an outline around the ellipse. Either one (or both)
        can be left out.

        To draw a circle, make sure the coordinates form a square.

        Args:
            xy: A bounding rectangle as a 4-element Python sequence (x, y, x, y),
                with the upper-left corner given first.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the ellipse.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the ellipse.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.ellipse(xy, brush, pen)

    def flush(self):
        """Updates the associated image.
        
        If the drawing area is attached to a PIL Image object, this method must
        be called to make sure that the image updated.

        """
        return self._draw.flush()

    def frombytes(self, data):
        """Copies data from a bytes object to the drawing area.

        Args:
            data (bytes): Packed image data compatible with PIL's tobytes method.
        
        """
        self._draw.frombytes(data)

    def line(self, xy, pen=None):
        """Draws a line.

        If the sequence contains multiple x/y pairs, multiple connected lines
        will be drawn.

        Args:
            xy: A Python sequence in the format (x, y, x, y, ...)
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing the line.

        """
        if isinstance(xy, Path):
            xy = xy._path
        if pen:
            pen = pen._pen
        self._draw.line(xy, pen)

    def path(self, xy, path, pen=None, brush=None):
        """Draws a path at the given positions.
        
        If a brush is given, it is used to fill the path. If a pen is given,
        it is used to draw an outline around the path. Either one (or both)
        can be left out.

        Args:
            xy: A Python sequence in the format (x, y, x, y, ...)
            path (:obj:`aggdraw.Path`): The Path object to draw.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the path.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the path.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.path(xy, path._path, brush, pen)

    def pieslice(self, xy, start, end, pen=None, brush=None):
        """Draws a pie slice.
        
        If a brush is given, it is used to fill the pie slice. If a pen is given,
        it is used to draw an outline around the pie slice. Either one (or both)
        can be left out.

        Args:
            xy: A 4-element Python sequence (x, y, x, y) with the upper-left corner
                given first.
            start (float): The start angle of the pie slice.
            end (float): The end angle of the pie slice.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the pie slice.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the pie slice.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.pieslice(xy, start, end, pen, brush)

    def polygon(self, xy, pen=None, brush=None):
        """Draws a polygon.
        
        If a brush is given, it is used to fill the polygon. If a pen is given,
        it is used to draw an outline around the polygon. Either one (or both)
        can be left out.

        Args:
            xy: A Python sequence (x, y, x, y, ...).
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the polygon.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the polygon.
        
        """
        if isinstance(xy, Path):
            xy = xy._path
        brush, pen = self._parse_args(brush, pen)
        self._draw.polygon(xy, brush, pen)

    def rectangle(self, xy, pen=None, brush=None):
        """Draws a rectangle.
        
        If a brush is given, it is used to fill the rectangle. If a pen is given,
        it is used to draw an outline around the rectangle. Either one (or both)
        can be left out.

        Args:
            xy: A 4-element Python sequence (x, y, x, y), with the upper left corner
                given first.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the rectangle.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the rectangle.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.rectangle(xy, brush, pen)

    def rounded_rectangle(self, xy, radius, pen=None, brush=None):
        """Draws a rounded rectangle.
        
        If a brush is given, it is used to fill the rectangle. If a pen is given,
        it is used to draw an outline around the rectangle. Either one (or both)
        can be left out.

        Args:
            xy: A 4-element Python sequence (x, y, x, y), with the upper left corner
                given first.
            radius (float): The corner radius.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the rectangle.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the rectangle.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.rounded_rectangle(xy, radius, brush, pen)

    def setantialias(self, flag):
        """Controls anti-aliasing.
        
        Args:
            flag (bool): True to enable anti-aliasing, False to disable it.

        """
        self._draw.setantialias(flag)

    def settransform(self, transform=None):
        """Replaces the current drawing transform.

        The transform must either be a (dx, dy) translation tuple, or a PIL-style
        (a, b, c, d, e, f) affine transform tuple. If the transform is omitted,
        it is reset.

        Example::
           draw.settransform((dx, dy))
        
        Args:
            transform (tuple, optional): The new transform, or None to reset.

        """
        if transform:
            self._draw.settransform(transform)
        else:
            self._draw.settransform()

    def symbol(self, xy, symbol, pen=None, brush=None):
        """Draws a symbol at the given positions.
        
        If a brush is given, it is used to fill the symbol. If a pen is given,
        it is used to draw an outline around the symbol. Either one (or both)
        can be left out.

        Args:
            xy: A Python sequence in the format (x, y, x, y, ...)
            symbol (:obj:`aggdraw.Symbol`): The Symbol object to draw.
            pen (:obj:`aggdraw.Pen`, optional): A pen to use for drawing an outline
                around the symbol.
            brush (:obj:`aggdraw.Brush`, optional): A brush to use for filling
                the symbol.
        
        """
        brush, pen = self._parse_args(brush, pen)
        self._draw.symbol(xy, symbol._path, brush, pen)

    def text(self, xy, text, font):
        """Draws a text string at a given position using a given font.

        Example::
           font = aggdraw.Font(black, times)
           draw.text((100, 100), "hello, world", font)

        Args:
            xy: A 2-element Python sequence (x, y).
            text (str): A string of text to render.
            font (:obj:`aggdraw.Font`): The font object to render with.

        Returns:
            tuple: A (width, height) tuple.
        
        """
        self._draw.text(xy, text, font._font)

    def textsize(self, text, font):
        """Determines the size of a text string.

        Args:
            text (str): A string of text to measure.
            font (:obj:`aggdraw.Font`): The font object to render with.

        Returns:
            tuple: A (width, height) tuple.
        
        """
        return self._draw.textsize(text, font._font)

    def tobytes(self):
        """Copies data from the drawing area to a bytes object.

        Returns:
            bytes: Packed image data compatible with PIL's frombytes method.
        
        """
        return self._draw.tobytes()
