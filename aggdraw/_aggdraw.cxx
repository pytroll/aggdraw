/*
 * AGG Draw Library
 *
 * WCK-style drawing using the AGG library.
 *
 * history:
 * 2004-09-14 fl   created, based on experimental code
 * 2004-09-15 fl   added pen/brush objects (from ironpil), multiple modes
 * 2004-09-16 fl   added text, arc/ellipse support
 * 2005-03-25 fl   added BGRA support
 * 2005-05-02 fl   added (experimental) symbol support
 * 2005-05-12 fl   added image constructor and flush method
 * 2005-05-18 fl   fixed possible image constructor crash
 * 2005-05-18 fl   make sure to keep a reference to the image
 * 2005-05-19 fl   improved symbol path support
 * 2005-06-12 fl   added support for S and T path operators
 * 2005-06-15 fl   added support for outline fonts
 * 2005-06-15 fl   support settransform for basic primitives and text
 * 2005-06-19 fl   use ImageColor.getrgb to resolve colors
 * 2005-06-30 fl   added Path object (stub)
 * 2005-07-04 fl   added Path methods (moveto, lineto, etc)
 * 2005-07-05 fl   added Path support to the line and polygon primitives
 * 2005-08-10 fl   fixed Draw(im) buffer memory leak (ouch!)
 * 2005-08-20 fl   fixed background color setting for RGB modes
 * 2005-08-30 fl   expand polygons by 0.5 pixels by default (experimental)
 * 2005-08-30 fl   fixed proper clipping in rasterizer
 * 2005-09-23 fl   added antialias setting
 * 2005-09-24 fl   don't recreate draw adaptor for each operation
 * 2005-09-26 fl   added coords method to Path type
 * 2005-10-10 fl   fixed broken add_path calls in symbol renderer (1.1)
 * 2005-10-19 fl   added native Windows support (via the Dib factory)
 * 2005-10-20 fl   added clear method
 * 2005-10-23 fl   support either hdc or hwnd in expose
 * 2006-02-12 fl   fixed crashes in type(obj) and path constructor
 *
 * Copyright (c) 2003-2006 by Secret Labs AB
 *
 * 2015-07-15 ej   fixed broken paths
 * 2017-01-03 ej   added support for python 3
 * 2017-01-03 ej   tostring() -> tobytes(), fromstring() -> frombytes()
 * 2017-08-18 dh   fixed mode to be python str instead of bytes
 * 2017-08-18 dh   fixed a couple compiler warnings (specifically clang)
 * 2018-04-21 dh   fixed python 2 compatibility in getcolor
 * 2026-08-24 dh   dropped Python 2 support code; ready types at import
 *
 * Copyright (c) 2011-2017 by AggDraw Developers
 *
 */

#define Q(x) #x
#define QUOTE(x) Q(x)

#if defined(_MSC_VER)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.1415926535897931
#endif

#define PY_SSIZE_T_CLEAN 1

#include "Python.h"

/* agg2 components */
#include "agg_arc.h"
#include "agg_conv_contour.h"
#include "agg_conv_curve.h"
// #include "agg_conv_dash.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_ellipse.h"
#include "agg_rounded_rect.h"
#if defined(HAVE_FREETYPE2)
#include "agg_font_freetype.h"
#endif
#include "agg_path_storage.h"
#include "agg_pixfmt_gray8.h"
#include "agg_pixfmt_rgb24.h"
#include "agg_pixfmt_rgba32.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "platform/agg_platform_support.h" // agg::pix_format_*

/* -------------------------------------------------------------------- */
/* AGG Drawing Surface */

#if defined(HAVE_FREETYPE2)
typedef agg::font_engine_freetype_int32 font_engine_type;
typedef agg::font_cache_manager<font_engine_type> font_manager_type;

static font_engine_type font_engine;
static font_manager_type font_manager(font_engine);
#endif

/* forward declaration */
class draw_adaptor_base;

template<class PixFmt> class draw_adaptor;

typedef struct {
    PyObject_HEAD
    draw_adaptor_base *draw;
    agg::rendering_buffer* buffer;
    agg::trans_affine* transform;
    unsigned char* buffer_data;
    int mode; // agg::pix_format_*
    int xsize, ysize;
    int buffer_size;
    PyObject* image;
    PyObject* background;
} DrawObject;

/* glue functions (see the init function for details) */
static PyObject* aggdraw_getcolor_obj;

/* All six types are heap types, created with PyType_FromSpec in aggdraw_init
   and exposed on the module as real classes. Each PyType_Spec is defined next
   to that type's own methods, further down the file. Symbol is a subclass of
   Path, so it is built with PyType_FromSpecWithBases instead. */
static PyTypeObject* DrawType;
static PyTypeObject* PenType;
static PyTypeObject* BrushType;
static PyTypeObject* FontType;
static PyTypeObject* PathType;
static PyTypeObject* SymbolType;

/* The _Check macros use PyObject_TypeCheck rather than an identity test: the
   types set Py_TPFLAGS_BASETYPE, and an identity test would make
   draw_adaptor::draw silently ignore a subclass of Pen or Brush. */

typedef struct {
    PyObject_HEAD
    agg::rgba8 color;
    float width;
} PenObject;

#define Pen_Check(op) ((op) != NULL && PyObject_TypeCheck(op, PenType))

typedef struct {
    PyObject_HEAD
    agg::rgba8 color;
} BrushObject;

#define Brush_Check(op) ((op) != NULL && PyObject_TypeCheck(op, BrushType))

typedef struct {
    PyObject_HEAD
    char* filename;
    float height;
    agg::rgba8 color;
} FontObject;

#if defined(HAVE_FREETYPE2)
static FT_Face font_load(FontObject* font, bool outline=false);
#endif

/* Defined below the getset block, which the Font spec needs to name first. */
static void font_dealloc(FontObject* self);

#define Font_Check(op) ((op) != NULL && PyObject_TypeCheck(op, FontType))

typedef struct {
    PyObject_HEAD
    agg::path_storage* path;
} PathObject;

#define Path_Check(op) ((op) != NULL && PyObject_TypeCheck(op, PathType))

static agg::rgba8 getcolor(PyObject* color, int opacity=255);

/* -------------------------------------------------------------------- */

#if defined(HAVE_FREETYPE2)
static int
text_getchar(PyObject* string, int index, unsigned long* char_out)
{
    if (PyUnicode_Check(string)) {
        Py_ssize_t str_len = PyUnicode_GetLength(string);
        if (index >= str_len)
            return 0;
        Py_UCS4 this_char = PyUnicode_READ_CHAR(string, index);
        *char_out = this_char;
        return 1;
    }
    return 0;
}
#endif

/* This template class is used to automagically instantiate drawing
   code for all pixel formats used by the library. */

class draw_adaptor_base
{
public:
    const char* mode;
    virtual ~draw_adaptor_base() {};
    virtual void setantialias(bool flag) = 0;
    virtual void draw(agg::path_storage &path, PyObject* obj1,
                      PyObject* obj2=NULL) = 0;
    virtual void drawtext(float xy[2], PyObject* text, FontObject* font) {};
};

template<class PixFmt> class draw_adaptor : public draw_adaptor_base {

    DrawObject* self;

    typedef agg::renderer_base<PixFmt> renderer_base;
    typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_aa;

    agg::rasterizer_scanline_aa<> rasterizer;
    agg::scanline_p8 scanline;

public:
    draw_adaptor(DrawObject* self_, const char* mode_)
    {
        self = self_;
        mode = mode_;

        setantialias(true);

        rasterizer.clip_box(0,0, self->xsize, self->ysize);
    }

    void setantialias(bool flag)
    {
        if (flag)
            rasterizer.gamma(agg::gamma_linear());
        else
            rasterizer.gamma(agg::gamma_threshold(0.5));
    };

    void draw(agg::path_storage &path, PyObject* obj1, PyObject* obj2=NULL)
    {
        PixFmt pf(*self->buffer);
        renderer_base rb(pf);
        renderer_aa renderer(rb);

        agg::path_storage* p;

        PenObject* pen;
        if (Pen_Check(obj1))
            pen = (PenObject*) obj1;
        else if (Pen_Check(obj2))
            pen = (PenObject*) obj2;
        else
            pen = NULL;

        BrushObject* brush;
        if (Brush_Check(obj2))
            brush = (BrushObject*) obj2;
        else if (Brush_Check(obj1))
            brush = (BrushObject*) obj1;
        else
            brush = NULL;

        if (self->transform) {
            p = new agg::path_storage();
            agg::conv_transform<agg::path_storage, agg::trans_affine>
                tp(path, *self->transform);
            p->add_path(tp, 0, false);
        } else
            p = &path;

        if (brush) {
            /* interior */
            agg::conv_contour<agg::path_storage> contour(*p);
            contour.auto_detect_orientation(true);
            if (pen)
                contour.width(pen->width / 2.0);
            else
                contour.width(0.5);
            rasterizer.reset();
            rasterizer.add_path(contour);
            renderer.color(brush->color);
            agg::render_scanlines(rasterizer, scanline, renderer);
        }

        if (pen) {
            /* outline */
            /* FIXME: add path for dashed lines */
            agg::conv_stroke<agg::path_storage> stroke(*p);
            stroke.width(pen->width);
            rasterizer.reset();
            rasterizer.add_path(stroke);
            renderer.color(pen->color);
            agg::render_scanlines(rasterizer, scanline, renderer);
        }
        if (self->transform)
            delete p;
    }

#if defined(HAVE_FREETYPE2)
    void drawtext(float xy[2], PyObject* text, FontObject* font)
    {
        PixFmt pf(*self->buffer);
        renderer_base rb(pf);
        renderer_aa renderer(rb);

        typedef agg::conv_curve<font_manager_type::path_adaptor_type> curve_t;
        curve_t curves(font_manager.path_adaptor());

        bool outline = (self->transform != NULL);

        FT_Face face = font_load(font, outline);
        if (!face)
            return;

        double x = xy[0];
        double y = xy[1] + face->size->metrics.ascender/64.0;

        renderer.color(font->color);
        curves.approximation_scale(1);

        unsigned long ch;
        int index = 0;

        while (text_getchar(text, index, &ch)) {
            index++;
            const agg::glyph_cache* glyph;
            glyph = font_manager.glyph(ch);
            if (!glyph)
                continue;
            font_manager.add_kerning(&x, &y);
            font_manager.init_embedded_adaptors(glyph, x, y);
            if (outline) {
                rasterizer.reset();
                if (self->transform) {
                    agg::conv_transform<curve_t, agg::trans_affine>
                        tp(curves, *self->transform);
                    rasterizer.add_path(tp);
                } else
                    rasterizer.add_path(curves);
                agg::render_scanlines(rasterizer, scanline, renderer);
            } else {
                agg::render_scanlines(
                    font_manager.gray8_adaptor(),
                    font_manager.gray8_scanline(), renderer
                    );
            }
            x += glyph->advance_x;
            y += glyph->advance_y;
        }
    }
#endif
};

/* -------------------------------------------------------------------- */

static void clear(DrawObject* self, PyObject* background)
{
    if (background && background != Py_None) {
        agg::rgba8 ink = getcolor(background);
        unsigned char* p = self->buffer_data;
        int c, i;
        switch (self->mode) {
            case agg::pix_format_gray8:
                c = (ink.r*299 + ink.g*587 + ink.b*114) / 1000;
                memset(self->buffer_data, c, self->buffer_size);
                break;
            case agg::pix_format_rgb24:
                for (i = 0; i < self->buffer_size; i += 3) {
                    p[i+0] = ink.r;
                    p[i+1] = ink.g;
                    p[i+2] = ink.b;
                }
                break;
            case agg::pix_format_bgr24:
                for (i = 0; i < self->buffer_size; i += 3) {
                    p[i+0] = ink.b;
                    p[i+1] = ink.g;
                    p[i+2] = ink.r;
                }
                break;
            case agg::pix_format_rgba32:
                for (i = 0; i < self->buffer_size; i += 4) {
                    p[i+0] = ink.r;
                    p[i+1] = ink.g;
                    p[i+2] = ink.b;
                    p[i+3] = ink.a;
                }
                break;
            case agg::pix_format_bgra32:
                for (i = 0; i < self->buffer_size; i += 4) {
                    p[i+0] = ink.b;
                    p[i+1] = ink.g;
                    p[i+2] = ink.r;
                    p[i+3] = ink.a;
                }
                break;
        }
    } else
        memset(self->buffer_data, 255, self->buffer_size);
}

static void draw_setup(DrawObject* self)
{
    switch (self->mode) {
    case agg::pix_format_gray8:
        self->draw = new draw_adaptor<agg::pixfmt_gray8>(self, "L");
        break;
    case agg::pix_format_rgb24:
        self->draw = new draw_adaptor<agg::pixfmt_rgb24>(self, "RGB");
        break;
    case agg::pix_format_bgr24:
        self->draw = new draw_adaptor<agg::pixfmt_bgr24>(self, "BGR");
        break;
    default:
        self->draw = new draw_adaptor<agg::pixfmt_rgba32>(self, "RGBA");
        break;
    }
}

const char *draw_doc = "Creates a drawing interface object.\n"
                       "\n"
                       "Parameters\n"
                       "----------\n"
                       "image_or_mode : PIL.Image.Image or str\n"
                       "    A PIL Image or a mode string. The following modes\n"
                       "    are supported: \"L\", \"RGB\", \"RGBA\", \"BGR\", \"BGRA\".\n"
                       "size : tuple\n"
                       "    If a mode string was given, this argument gives the image size\n"
                       "    as a 2-element tuple.\n"
                       "color\n"
                       "    An optional background color specifier.\n"
                       "    If a mode string was given, this is used to initialize the image memory.\n"
                       "    If omitted, it defaults to white with full alpha.\n"
                       "\n"
                       "Examples\n"
                       "--------\n"
                       "\n"
                       "    >>> d = aggdraw.Draw(im)\n"
                       "    >>> d = aggdraw.Draw(\"RGB\", (800, 600), \"white\")\n";

static PyObject*
draw_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    /* tp_new is handed keywords whether or not it wants them; the old
       METH_VARARGS entry point rejected them, so keep doing that rather than
       accepting and ignoring them. */
    if (kw != NULL && PyDict_GET_SIZE(kw) != 0) {
        PyErr_SetString(PyExc_TypeError, "Draw() takes no keyword arguments");
        return NULL;
    }

    char buffer[10];
    int ok;

    PyObject* image;
    char* mode;
    int xsize, ysize;
    PyObject* background = NULL;

    if (PyArg_ParseTuple(args, "O:Draw", &image)) {

        /* get mode (use a local buffer to avoid GC issues) */
        PyObject* mode_obj = PyObject_GetAttrString(image, "mode");
        if (!mode_obj)
            return NULL;
        if (PyUnicode_Check(mode_obj)) {
            PyObject* ascii_mode = PyUnicode_AsASCIIString(mode_obj);
            if (ascii_mode == NULL) {
                mode = NULL;
            } else {
                strncpy(buffer, PyBytes_AsString(ascii_mode), sizeof buffer);
                buffer[sizeof(buffer)-1] = '\0'; /* to be on the safe side */
                mode = buffer;
                Py_XDECREF(ascii_mode);
            }
        } else
            mode = NULL;
        Py_DECREF(mode_obj);
        if (!mode) {
            PyErr_SetString(
                PyExc_TypeError,
                "bad 'mode' attribute (expected string)"
                );
            return NULL;
        }

        PyObject* size_obj = PyObject_GetAttrString(image, "size");
        if (!size_obj)
            return NULL;
        if (PyTuple_Check(size_obj))
            ok = PyArg_ParseTuple(size_obj, "ii", &xsize, &ysize);
        else {
            PyErr_SetString(
                PyExc_TypeError,
                "bad 'size' attribute (expected 2-tuple)"
                );
            ok = 0;
        }
        Py_DECREF(size_obj);
        if (!ok)
            return NULL;

    } else {
        PyErr_Clear();
        if (!PyArg_ParseTuple(args, "s(ii)|O:Draw",
                             &mode, &xsize, &ysize, &background))
            return NULL;
        image = NULL;
    }

    DrawObject* self = (DrawObject*) type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    /* PyObject_NEW does not zero the allocation and draw_dealloc deletes every
       pointer field, so they must all be valid before any error path below can
       Py_DECREF(self). */
    self->draw = NULL;
    self->buffer = NULL;
    self->transform = NULL;
    self->buffer_data = NULL;
    self->image = NULL;
    self->background = NULL;

    int stride;
    if (!strcmp(mode, "L")) {
        self->mode = agg::pix_format_gray8;
        stride = xsize;
    } else if (!strcmp(mode, "RGB")) {
        self->mode = agg::pix_format_rgb24;
        stride = xsize * 3;
    } else if (!strcmp(mode, "BGR")) {
        self->mode = agg::pix_format_bgr24;
        stride = xsize * 3;
    } else if (!strcmp(mode, "RGBA")) {
        self->mode = agg::pix_format_rgba32;
        stride = xsize * 4;
    } else if (!strcmp(mode, "BGRA")) {
        self->mode = agg::pix_format_bgra32;
        stride = xsize * 4;
    } else {
        PyErr_SetString(PyExc_ValueError, "bad mode");
        Py_DECREF(self);
        return NULL;
    }

    self->buffer_size = ysize * stride;
    self->buffer_data = new unsigned char[self->buffer_size];

    Py_XINCREF(background);
    self->background = background;

    clear(self, background);

    self->buffer = new agg::rendering_buffer(
        self->buffer_data, xsize, ysize, stride
        );

    self->xsize = xsize;
    self->ysize = ysize;

    /* Take the reference at the point of assignment, not after the round-trip
       below: until this incref runs the struct holds a borrowed pointer. */
    Py_XINCREF(image);
    self->image = image;
    if (image) {
        PyObject* buffer = PyObject_CallMethod(image, "tobytes", NULL);
        if (!buffer) {
            Py_DECREF(self);
            return NULL;
        }
        if (!PyBytes_Check(buffer)) {
            PyErr_SetString(
                PyExc_TypeError,
                "bad 'tobytes' return value (expected string)"
                );
            Py_DECREF(buffer);
            Py_DECREF(self);
            return NULL;
        }
        char* data = PyBytes_AS_STRING(buffer);
        int data_size = PyBytes_GET_SIZE(buffer);
        if (data_size >= self->buffer_size)
            memcpy(self->buffer_data, data, self->buffer_size);
        else {
            PyErr_SetString(PyExc_ValueError, "not enough data");
            Py_DECREF(buffer);
            Py_DECREF(self);
            return NULL;
        }
        Py_DECREF(buffer);
    }

    draw_setup(self);

    return (PyObject*) self;
}

struct PointF {
    float X;
    float Y;
};

#define GETFLOAT(op)                                    \
    (PyLong_Check(op) ? (float) PyLong_AS_LONG((op)) :\
     PyFloat_Check(op) ? (float) PyFloat_AS_DOUBLE((op)) :\
     (float) PyFloat_AsDouble(op))

static PointF*
getpoints(PyObject* xyIn, int* count)
{
    PointF *xy;
    int i, n;

    /* FIXME: use local buffer (provided by caller) for short
       sequences */

    if (!PySequence_Check(xyIn)) {
        PyErr_SetString(PyExc_TypeError, "argument must be a sequence");
        return NULL;
    }

    n = PyObject_Length(xyIn);
    if (PyErr_Occurred())
        return NULL;

    if (n & 1) {
        PyErr_SetString(PyExc_TypeError,
                        "expected even number of coordinates");
        return NULL;
        }

    n /= 2;

    xy = new PointF[n+1];
    if (!xy) {
        PyErr_NoMemory();
        *count = -1;
        return NULL;
    }

    if (PyList_Check(xyIn))
        for (i = 0; i < n; i++) {
            xy[i].X = GETFLOAT(PyList_GET_ITEM(xyIn, i+i));
            xy[i].Y = GETFLOAT(PyList_GET_ITEM(xyIn, i+i+1));
        }
    else if (PyTuple_Check(xyIn))
        for (i = 0; i < n; i++) {
            xy[i].X = GETFLOAT(PyTuple_GET_ITEM(xyIn, i+i));
            xy[i].Y = GETFLOAT(PyTuple_GET_ITEM(xyIn, i+i+1));
        }
    else
        for (i = 0; i < n; i++) {
            PyObject *op;
            op = PySequence_GetItem(xyIn, i+i);
            xy[i].X = GETFLOAT(op);
            Py_DECREF(op);
            op = PySequence_GetItem(xyIn, i+i+1);
            xy[i].Y = GETFLOAT(op);
            Py_DECREF(op);
        }

    PyErr_Clear();

    *count = n;

    return xy;
}

static agg::rgba8
getcolor(PyObject* color, int opacity)
{
    if (PyLong_Check(color)) {
        int ink = PyLong_AsLong(color);
        return agg::rgba8(ink, ink, ink, opacity);
    }

    char buffer[10];
    char* ink = NULL;
    if (PyUnicode_Check(color)) {
        PyObject* ascii_color = PyUnicode_AsASCIIString(color);
        if (ascii_color == NULL) {
            /* not ASCII: leave ink NULL and let the lookups below fall
               through to black. Clear the error so that the calls that
               follow do not run with a live exception set. */
            PyErr_Clear();
        } else {
            strncpy(buffer, PyBytes_AsString(ascii_color), sizeof buffer);
            buffer[sizeof(buffer)-1] = '\0'; /* to be on the safe side */
            ink = buffer;
            Py_XDECREF(ascii_color);
        }
    }
    /* hex colors */
    if (ink && ink[0] == '#' && strlen(ink) == 7) {
        int i = strtol(ink+1, NULL, 16); /* FIXME: rough parsing */
        return agg::rgba8((i>>16)&255,(i>>8)&255,i&255,opacity);
    }

    int red, green, blue, alpha = opacity;
    if (PyArg_ParseTuple(color, "iii|i", &red, &green, &blue, &alpha))
        return agg::rgba8(red, green, blue, alpha);
    PyErr_Clear();
    /* unknown color: pass it to the Python layer */
    if (aggdraw_getcolor_obj) {
        PyObject* result;
        result = PyObject_CallFunction(aggdraw_getcolor_obj, "O", color);
        if (result) {
            int ok = PyArg_ParseTuple(result, "iii", &red, &green, &blue);
            Py_DECREF(result);
            if (ok)
                return agg::rgba8(red, green, blue, opacity);
        }
        PyErr_Clear();
    }
    /* check for well-known color names (HTML) */
    if (ink) {
        if (!strcmp(ink, "aqua"))
            return agg::rgba8(0x00,0xFF,0xFF,opacity);
        if (!strcmp(ink, "black"))
            return agg::rgba8(0x00,0x00,0x00,opacity);
        if (!strcmp(ink, "blue"))
            return agg::rgba8(0x00,0x00,0xFF,opacity);
        if (!strcmp(ink, "fuchsia"))
            return agg::rgba8(0xFF,0x00,0xFF,opacity);
        if (!strcmp(ink, "gray"))
            return agg::rgba8(0x80,0x80,0x80,opacity);
        if (!strcmp(ink, "green"))
            return agg::rgba8(0x00,0x80,0x00,opacity);
        if (!strcmp(ink, "lime"))
            return agg::rgba8(0x00,0xFF,0x00,opacity);
        if (!strcmp(ink, "maroon"))
            return agg::rgba8(0x80,0x00,0x00,opacity);
        if (!strcmp(ink, "navy"))
            return agg::rgba8(0x00,0x00,0x80,opacity);
        if (!strcmp(ink, "olive"))
            return agg::rgba8(0x80,0x80,0x00,opacity);
        if (!strcmp(ink, "purple"))
            return agg::rgba8(0x80,0x00,0x80,opacity);
        if (!strcmp(ink, "red"))
            return agg::rgba8(0xFF,0x00,0x00,opacity);
        if (!strcmp(ink, "silver"))
            return agg::rgba8(0xC0,0xC0,0xC0,opacity);
        if (!strcmp(ink, "teal"))
            return agg::rgba8(0x00,0x80,0x80,opacity);
        if (!strcmp(ink, "white"))
            return agg::rgba8(0xFF,0xFF,0xFF,opacity);
        if (!strcmp(ink, "yellow"))
            return agg::rgba8(0xFF,0xFF,0x00,opacity);
        /* extra colors (used by test2d.py) */
        if (!strcmp(ink, "gold"))
            return agg::rgba8(0xFF,0xD7,0x00,opacity);

    }
    /* default to black (FIXME: raise an exception instead?) */
    return agg::rgba8(0, 0, 0, opacity);
}

/* -------------------------------------------------------------------- */

const char *draw_arc_doc = "Draw a arc.\n"
                           "\n"
                           "A single optional pen or brush may be given; it is selected by\n"
                           "type, not by position. With a pen the arc is stroked as a curved\n"
                           "line. With a brush the arc is filled instead, giving the same\n"
                           "result as a chord with that brush.\n"
                           "\n"
                           "Parameters\n"
                           "----------\n"
                           "xy : iterable\n"
                           "    A 4-element Python sequence (x, y, x, y), with the\n"
                           "    upper left corner given first.\n"
                           "start : float\n"
                           "    Start angle in degrees.\n"
                           "end : float\n"
                           "    End angle in degrees.\n"
                           "pen : Pen or Brush, optional\n"
                           "    An optional pen object created by the `Pen` factory, or a\n"
                           "    brush object created by the `Brush` factory.\n";

static PyObject*
draw_arc(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1;
    float start, end;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)ff|O:arc",
                          &x0, &y0, &x1, &y1, &start, &end, &pen))
        return NULL;

    agg::path_storage path;
    agg::arc arc(
        (x1+x0)/2, (y1+y0)/2, (x1-x0)/2, (y1-y0)/2,
        -start * (float) (M_PI / 180.0), -end * (float) (M_PI / 180.0),
        false
        );
    arc.approximation_scale(1);
    path.add_path(arc);

    self->draw->draw(path, pen);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_chord_doc = "Draw a chord.\n"
                             "\n"
                             "If a brush is given, it is used to fill the chord.\n"
                             "If a pen is given, it is used to draw an outline around the chord.\n"
                             "Either one (or both) can be left out.\n"
                             "The pen and brush arguments are selected by type, so the order in\n"
                             "which they are given does not matter.\n"
                             "\n"
                             "Parameters\n"
                             "----------\n"
                             "xy : iterable\n"
                             "    A 4-element Python sequence (x, y, x, y), with the\n"
                             "    upper left corner given first.\n"
                             "start : float\n"
                             "    Start angle in degrees.\n"
                             "end : float\n"
                             "    End angle in degrees.\n"
                             "pen : Pen, optional\n"
                             "    Optional pen object created by the `Pen` factory.\n"
                             "brush : Brush, optional\n"
                             "    Optional brush object created by the `Brush` factory.\n";

static PyObject*
draw_chord(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1;
    float start, end;
    PyObject* pen = NULL;
    PyObject* brush = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)ff|OO:chord",
                          &x0, &y0, &x1, &y1, &start, &end, &pen, &brush))
        return NULL;

    agg::path_storage path;
    agg::arc arc(
        (x1+x0)/2, (y1+y0)/2, (x1-x0)/2, (y1-y0)/2,
        -start * (float) (M_PI / 180.0), -end * (float) (M_PI / 180.0),
        false
        );
    arc.approximation_scale(1);
    path.add_path(arc);
    path.close_polygon();

    self->draw->draw(path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_ellipse_doc = "Draw a ellipse.\n"
                               "\n"
                               "If a brush is given, it is used to fill the ellipse.\n"
                               "If a pen is given, it is used to draw an outline around the ellipse.\n"
                               "Either one (or both) can be left out.\n"
                               "The pen and brush arguments are selected by type, so the order in\n"
                               "which they are given does not matter.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "xy : iterable\n"
                               "    A 4-element Python sequence (x, y, x, y), with the\n"
                               "    upper left corner given first. To draw a circle, make sure the\n"
                               "    coordinates form a square.\n"
                               "brush : Brush, optional\n"
                               "    Optional brush object created by the `Brush` factory.\n"
                               "pen : Pen, optional\n"
                               "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_ellipse(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)|OO:ellipse",
                          &x0, &y0, &x1, &y1, &brush, &pen))
        return NULL;

    agg::path_storage path;
    agg::ellipse ellipse((x1+x0)/2, (y1+y0)/2, (x1-x0)/2, (y1-y0)/2, 8);
    ellipse.approximation_scale(1);
    path.add_path(ellipse);

    self->draw->draw(path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_line_doc = "Draw a line.\n"
                            "\n"
                            "Parameters\n"
                            "----------\n"
                            "xy : iterable\n"
                            "    An iterable (x, y, x, y, ...). If more\n"
                            "    than two coordinate pairs are given, they are connected in order,\n"
                            "    to form a polyline.\n"
                            "pen : Pen\n"
                            "    A pen object created by the Pen factory method.\n"
                            "\n"
                            "Examples\n"
                            "--------\n"
                            "\n"
                            "    >>> # a cross\n"
                            "    >>> pen = aggdraw.Pen(\"red\")\n"
                            "    >>> draw.line((x0, y0, x1, y1), pen)\n"
                            "    >>> draw.line((x0, y1, x1, y0), pen)\n"
                            "\n"
                            "    >>> # a character\n"
                            "    >>> draw.line((0, 0, 20, 100, 30, 50, 40, 100, 60, 0), pen)\n";

static PyObject*
draw_line(DrawObject* self, PyObject* args)
{
    PyObject* xyIn;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "O|O:line", &xyIn, &pen))
        return NULL;

    if (Path_Check(xyIn)) {
        self->draw->draw(*((PathObject*) xyIn)->path, pen);
    } else {
        int count;
        PointF *xy = getpoints(xyIn, &count);
        if (!xy)
            return NULL;
        agg::path_storage path;
        path.move_to(xy[0].X, xy[0].Y);
        for (int i = 1; i < count; i++)
            path.line_to(xy[i].X, xy[i].Y);
        delete [] xy;
        self->draw->draw(path, pen);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


const char *draw_pieslice_doc = "Draw a pieslice.\n"
                             "\n"
                             "If a brush is given, it is used to fill the pieslice.\n"
                             "If a pen is given, it is used to draw an outline around the pieslice.\n"
                             "Either one (or both) can be left out.\n"
                             "The pen and brush arguments are selected by type, so the order in\n"
                             "which they are given does not matter.\n"
                             "\n"
                             "Parameters\n"
                             "----------\n"
                             "xy : iterable\n"
                             "    A 4-element Python sequence (x, y, x, y), with the\n"
                             "    upper left corner given first.\n"
                             "start : float\n"
                             "    Start angle in degrees.\n"
                             "end : float\n"
                             "    End angle in degrees.\n"
                             "pen : Pen, optional\n"
                             "    Optional pen object created by the `Pen` factory.\n"
                             "brush : Brush, optional\n"
                             "    Optional brush object created by the `Brush` factory.\n";

static PyObject*
draw_pieslice(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1;
    float start, end;
    PyObject* pen = NULL;
    PyObject* brush = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)ff|OO:pieslice",
                          &x0, &y0, &x1, &y1, &start, &end, &pen, &brush))
        return NULL;

    float x = (x1+x0)/2;
    float y = (y1+y0)/2;

    agg::path_storage path;
    agg::arc arc(
        x, y, (x1-x0)/2, (y1-y0)/2,
        -start * (float) (M_PI / 180.0), -end * (float) (M_PI / 180.0),
        false
        );
    arc.approximation_scale(1);
    path.add_path(arc);
    path.line_to(x, y);
    path.close_polygon();

    self->draw->draw(path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_polygon_doc = "Draw a polygon.\n"
                               "\n"
                               "If a brush is given, it is used to fill the polygon.\n"
                               "If a pen is given, it is used to draw an outline around the polygon.\n"
                               "Either one (or both) can be left out.\n"
                               "The pen and brush arguments are selected by type, so the order in\n"
                               "which they are given does not matter.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "xy : iterable\n"
                               "    A Python sequence (x, y, x, y, …).\n"
                               "brush : Brush, optional\n"
                               "    Optional brush object created by the `Brush` factory.\n"
                               "pen : Pen, optional\n"
                               "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_polygon(DrawObject* self, PyObject* args)
{
    PyObject* xyIn;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "O|OO:polygon", &xyIn, &brush, &pen))
        return NULL;

    if (Path_Check(xyIn)) {
        self->draw->draw(*((PathObject*) xyIn)->path, pen, brush);
    } else {
        int count;
        PointF *xy = getpoints(xyIn, &count);
        if (!xy)
            return NULL;
        agg::path_storage path;
        path.move_to(xy[0].X, xy[0].Y);
        for (int i = 1; i < count; i++)
            path.line_to(xy[i].X, xy[i].Y);
        path.close_polygon();
        delete [] xy;
        self->draw->draw(path, pen, brush);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_rectangle_doc = "Draw a rectangle.\n"
                                 "\n"
                                 "If a brush is given, it is used to fill the rectangle.\n"
                                 "If a pen is given, it is used to draw an outline around the rectangle.\n"
                                 "Either one (or both) can be left out.\n"
                                 "The pen and brush arguments are selected by type, so the order in\n"
                                 "which they are given does not matter.\n"
                                 "\n"
                                 "Parameters\n"
                                 "----------\n"
                                 "xy : iterable\n"
                                 "    A 4-element Python sequence (x, y, x, y), with the\n"
                                 "    upper left corner given first.\n"
                                 "brush : Brush, optional\n"
                                 "    Optional brush object created by the `Brush` factory.\n"
                                 "pen : Pen, optional\n"
                                 "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_rectangle(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)|OO:rectangle",
                          &x0, &y0, &x1, &y1, &brush, &pen))
        return NULL;

    agg::path_storage path;
    path.move_to(x0, y0);
    path.line_to(x1, y0);
    path.line_to(x1, y1);
    path.line_to(x0, y1);
    path.close_polygon();

    self->draw->draw(path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_rounded_rectangle_doc = "Draw a rounded rectangle.\n"
                               "\n"
                               "If a brush is given, it is used to fill the rounded rectangle.\n"
                               "If a pen is given, it is used to draw an outline around the rounded rectangle.\n"
                               "Either one (or both) can be left out.\n"
                               "The pen and brush arguments are selected by type, so the order in\n"
                               "which they are given does not matter.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "xy : iterable\n"
                               "    A 4-element Python sequence (x, y, x, y), with the\n"
                               "    upper left corner given first.\n"
                               "radius : float\n"
                               "    The corner radius\n"
                               "brush : Brush, optional\n"
                               "    Optional brush object created by the `Brush` factory.\n"
                               "pen : Pen, optional\n"
                               "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_rounded_rectangle(DrawObject* self, PyObject* args)
{
    float x0, y0, x1, y1, r;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "(ffff)f|OO:rounded_rectangle",
                          &x0, &y0, &x1, &y1, &r, &brush, &pen))
        return NULL;

    agg::path_storage path;
    agg::rounded_rect rr(x0, y0, x1, y1, r);
    rr.approximation_scale(1);
    path.add_path(rr);

    self->draw->draw(path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_path_doc = "Draw the given path.\n"
                            "\n"
                            "If a brush is given, it is used to fill the path.\n"
                            "If a pen is given, it is used to draw an outline around the path.\n"
                            "Either one (or both) can be left out.\n"
                            "The pen and brush arguments are selected by type, so the order in\n"
                            "which they are given does not matter.\n"
                            "\n"
                            "Parameters\n"
                            "----------\n"
                            "path : Path\n"
                            "    The path to draw. A `Symbol` is a `Path`, so either is\n"
                            "    accepted.\n"
                            "brush : Brush, optional\n"
                            "    Optional brush object created by the `Brush` factory.\n"
                            "pen : Pen, optional\n"
                            "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_path(DrawObject* self, PyObject* args){
    PathObject* path;
    PyObject*   brush = NULL;
    PyObject*   pen   = NULL;

    if (!PyArg_ParseTuple(args, "O!|OO:path", PathType, &path, &brush, &pen)){
        return NULL;
    }

    //agg::trans_affine_translation transform(xy[i].X,xy[i].Y);
    //agg::conv_transform<agg::path_storage, agg::trans_affine>
    //  tp(*symbol->path, transform);
    //agg::path_storage p;
    //p.add_path(tp, 0, false);
    self->draw->draw(*path->path, pen, brush);

    Py_INCREF(Py_None);
    return Py_None;
};

const char *draw_symbol_doc = "Draw a symbol at the given positions (experimental).\n"
                              "\n"
                              "If a brush is given, it is used to fill the symbol.\n"
                              "If a pen is given, it is used to draw an outline around the symbol.\n"
                              "Either one (or both) can be left out.\n"
                              "The pen and brush arguments are selected by type, so the order in\n"
                              "which they are given does not matter.\n"
                              "\n"
                              "Parameters\n"
                              "----------\n"
                              "xy : iterable\n"
                              "    A Python sequence (x, y, x, y, …).\n"
                              "symbol : Path\n"
                              "    The path to stamp at each position. Any `Path` works; a\n"
                              "    `Symbol` is a `Path`, so either is accepted.\n"
                              "brush : Brush, optional\n"
                              "    Optional brush object created by the `Brush` factory.\n"
                              "pen : Pen, optional\n"
                              "    Optional pen object created by the `Pen` factory.\n";

static PyObject*
draw_symbol(DrawObject* self, PyObject* args)
{
    PyObject* xyIn;
    PathObject* symbol;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "OO!|OO:symbol",
                          &xyIn, PathType, &symbol, &brush, &pen))
        return NULL;

    int count;
    PointF *xy = getpoints(xyIn, &count);
    if (!xy)
        return NULL;

    for (int i = 0; i < count; i++) {
        agg::trans_affine_translation transform(xy[i].X,xy[i].Y);
        agg::conv_transform<agg::path_storage, agg::trans_affine>
            tp(*symbol->path, transform);
        agg::path_storage p;
        p.add_path(tp, 0, false);
        self->draw->draw(p, pen, brush);
    }

    delete [] xy;

    Py_INCREF(Py_None);
    return Py_None;
}

#if defined(HAVE_FREETYPE2)

const char *draw_text_doc = "Draws a text string at the given position, using the given font.\n"
                            "\n"
                            "Parameters\n"
                            "----------\n"
                            "xy : tuple\n"
                            "    A two element tuple (x, y).\n"
                            "text : str\n"
                            "    String to draw.\n"
                            "font : Font\n"
                            "    A font object created by the Font factory.\n";

static PyObject*
draw_text(DrawObject* self, PyObject* args)
{
    float xy[2];
    PyObject* text;
    FontObject* font;
    if (!PyArg_ParseTuple(args, "(ff)OO!:text", xy+0, xy+1, &text,
                          FontType, &font))
        return NULL;

    self->draw->drawtext(xy, text, font);

    Py_INCREF(Py_None);
    return Py_None;
}
#endif

#if defined(HAVE_FREETYPE2)

const char *draw_textsize_doc = "Measure a text string as it would be drawn with the given font.\n"
                                "\n"
                                "Parameters\n"
                                "----------\n"
                                "text : str\n"
                                "    String to get the drawn size of.\n"
                                "font : Font\n"
                                "    A font object created by the Font factory.\n"
                                "\n"
                                "Returns\n"
                                "-------\n"
                                "tuple\n"
                                "    A (width, height) tuple, in pixels. The height is the\n"
                                "    font's line height, not the ink height of this string.\n";

static PyObject*
draw_textsize(DrawObject* self, PyObject* args)
{
    PyObject* text;
    FontObject* font;
    if (!PyArg_ParseTuple(args, "OO!:textsize", &text, FontType, &font))
        return NULL;

    FT_Face face = font_load(font);
    if (!face) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    int x, i;
    unsigned long ch;

    for (x = i = 0; text_getchar(text, i, &ch); i++) {
        int index = FT_Get_Char_Index(face, ch);
        if (index) {
            int error = FT_Load_Glyph(face, index, FT_LOAD_DEFAULT);
            if (!error)
                x += face->glyph->metrics.horiAdvance;
        }
    }

    return Py_BuildValue("ff", x/64.0, face->size->metrics.height/64.0);
}
#endif

const char *draw_setantialias_doc = "Control anti-aliasing (experimental).\n"
                            "\n"
                            "Parameters\n"
                            "----------\n"
                            "flag : bool\n"
                            "    True to enable anti-aliasing, false to disable.\n";

static PyObject*
draw_setantialias(DrawObject* self, PyObject* args)
{
    int i;
    if (!PyArg_ParseTuple(args, "i:setantialias", &i))
        return NULL;

    self->draw->setantialias(i != 0);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_settransform_doc = "Replace the current drawing transform (experimental).\n"
                            "\n"
                            "Parameters\n"
                            "----------\n"
                            "transform\n"
                            "    The new transform. In the current version, this must be either\n"
                            "    a (dx, dy) translation tuple, or a PIL-style (a, b, c, d, e, f)\n"
                            "    affine transform tuple. If the transform is omitted, it is reset.\n"
                            "\n"
                            "Examples\n"
                            "--------\n"
                            "\n"
                            "    >>> draw.settransform((dx, dy))\n";

static PyObject*
draw_settransform(DrawObject* self, PyObject* args)
{
    double a=1, b=0, c=0, d=0, e=1, f=0;
    if (!PyArg_ParseTuple(args, "|(dd):settransform", &c, &f)) {
        PyErr_Clear();
        if (!PyArg_ParseTuple(args, "(dddddd):settransform",
                              &a, &b, &c, &d, &e, &f))
            return NULL;
    }

    /* PIL order: x=ax+by+c y=dx+ey+f */
    /* AGG order: x=ax+cx+e y=bx+dy+f */
    agg::trans_affine* transform = new agg::trans_affine(a, d, b, e, c, f);
    if (!transform)
        return PyErr_NoMemory();

    delete self->transform;
    self->transform = transform;

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_frombytes_doc = "Copies data from a string buffer to the drawing area."
                                 "\n"
                                 "Parameters\n"
                                 "----------\n"
                                 "data : bytes\n"
                                 "    A string containing packed image data, compatible with PIL’s tobytes method.\n";

static PyObject*
draw_frombytes(DrawObject* self, PyObject* args)
{
    char* data = NULL;
    Py_ssize_t data_size;
    if (!PyArg_ParseTuple(args, "s#:frombytes", &data, &data_size))
        return NULL;

    if (data_size >= self->buffer_size)
        memcpy(self->buffer_data, data, self->buffer_size);
    else {
        PyErr_SetString(PyExc_ValueError, "not enough data");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_tobytes_doc = "Copies data from the drawing area to a string.\n"
                               "\n"
                               "Returns\n"
                               "-------\n"
                               "A string containing packed image data, compatible with PIL’s fromstring method.\n";

static PyObject*
draw_tobytes(DrawObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ":tobytes"))
        return NULL;

    return PyBytes_FromStringAndSize(
        (char*) self->buffer_data, self->buffer_size
        );
}

const char *draw_clear_doc = "Clear the image.\n"
                             "\n"
                             "Parameters\n"
                             "----------\n"
                             "color : tuple or str or int\n"
                             "    Background color. An (R, G, B) tuple takes its alpha from `opacity`; an\n"
                             "    (R, G, B, A) tuple sets alpha directly and overrides `opacity`.\n"
                             "    Strings are resolved by PIL.ImageColor.getrgb, so CSS color names,\n"
                             "    '#rgb', '#rrggbb' and 'rgb(...)' all work; forms resolving to four\n"
                             "    components such as 'rgba(...)' do not. An integer from 0 to 255 is a\n"
                             "    shade of gray, and out-of-range values wrap (300 gives the same gray\n"
                             "    as 44). An unrecognized color is silently treated as black.\n";

static PyObject*
draw_clear(DrawObject* self, PyObject* args)
{
    PyObject* background = self->background;
    if (!PyArg_ParseTuple(args, "|O:clear", &background))
        return NULL;

    clear(self, background);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *draw_flush_doc = "Updates the associated image.\n"
                             "\n"
                             "If the drawing area is attached to a PIL Image object, this method\n"
                             "must be called to make sure that the image updated.\n";
static PyObject*
draw_flush(DrawObject* self, PyObject* args)
{
    PyObject* result;

    if (!PyArg_ParseTuple(args, ":flush"))
        return NULL;

    if (!self->image) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject* buffer = draw_tobytes(self, args);
    if (!buffer)
        return NULL;

    result = PyObject_CallMethod(self->image, "frombytes", "N", buffer);
    if (!result)
        return NULL;

    Py_DECREF(result);

    Py_INCREF(self->image);
    return self->image;
}

static void
draw_dealloc(DrawObject* self)
{
    delete self->draw;
    delete self->buffer;
    delete self->transform;
    delete [] self->buffer_data;

    Py_XDECREF(self->background);
    Py_XDECREF(self->image);

    PyTypeObject* tp = Py_TYPE(self);
    tp->tp_free((PyObject*) self);
    Py_DECREF(tp);
}

static PyMethodDef draw_methods[] = {

    {"line", (PyCFunction) draw_line, METH_VARARGS, draw_line_doc},
    {"polygon", (PyCFunction) draw_polygon, METH_VARARGS, draw_polygon_doc},
    {"rectangle", (PyCFunction) draw_rectangle, METH_VARARGS, draw_rectangle_doc},
    {"rounded_rectangle", (PyCFunction) draw_rounded_rectangle, METH_VARARGS, draw_rounded_rectangle_doc},

#if defined(HAVE_FREETYPE2)
    {"text", (PyCFunction) draw_text, METH_VARARGS, draw_text_doc},
    {"textsize", (PyCFunction) draw_textsize, METH_VARARGS, draw_textsize_doc},
#endif

    {"path", (PyCFunction) draw_path, METH_VARARGS, draw_path_doc},
    {"symbol", (PyCFunction) draw_symbol, METH_VARARGS, draw_symbol_doc},

    {"arc", (PyCFunction) draw_arc, METH_VARARGS, draw_arc_doc},
    {"chord", (PyCFunction) draw_chord, METH_VARARGS, draw_chord_doc},
    {"ellipse", (PyCFunction) draw_ellipse, METH_VARARGS, draw_ellipse_doc},
    {"pieslice", (PyCFunction) draw_pieslice, METH_VARARGS, draw_pieslice_doc},

    {"settransform", (PyCFunction) draw_settransform, METH_VARARGS, draw_settransform_doc},
    {"setantialias", (PyCFunction) draw_setantialias, METH_VARARGS, draw_setantialias_doc},

    {"flush", (PyCFunction) draw_flush, METH_VARARGS, draw_flush_doc},

    {"clear", (PyCFunction) draw_clear, METH_VARARGS, draw_clear_doc},

    {"frombytes", (PyCFunction) draw_frombytes, METH_VARARGS, draw_frombytes_doc},
    {"tobytes", (PyCFunction) draw_tobytes, METH_VARARGS, draw_tobytes_doc},

    {NULL, NULL}
};

static PyObject*
draw_get_mode(DrawObject* self, void* closure)
{
    /* This is the adaptor's label, not the mode the surface was created with.
       A BGRA surface is rendered by the RGBA adaptor and so reports "RGBA". */
    return PyUnicode_FromString(self->draw->mode);
}

static PyObject*
draw_get_size(DrawObject* self, void* closure)
{
    return Py_BuildValue(
        "(ii)", self->buffer->width(), self->buffer->height()
        );
}

static PyGetSetDef draw_getset[] = {
    {"mode", (getter) draw_get_mode, NULL,
     (char*) "str: The mode of the drawing surface, e.g. \"RGB\".", NULL},
    {"size", (getter) draw_get_size, NULL,
     (char*) "tuple: The size of the drawing surface as (width, height).", NULL},
    {NULL}
};

/* PyType_Slot stores a void*, but the *_doc constants are const char*. */
#define DOC_SLOT(d) ((void*) const_cast<char*>(d))

static PyType_Slot draw_slots[] = {
    {Py_tp_new, (void*) draw_new},
    {Py_tp_dealloc, (void*) draw_dealloc},
    {Py_tp_methods, (void*) draw_methods},
    {Py_tp_getset, (void*) draw_getset},
    {Py_tp_doc, DOC_SLOT(draw_doc)},
    {0, NULL}
};

static PyType_Spec draw_spec = {
    "aggdraw._aggdraw.Draw",
    sizeof(DrawObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    draw_slots
};

/* -------------------------------------------------------------------- */

const char *pen_doc = "Creates a Pen object.\n"
                      "\n"
                      "Parameters\n"
                      "----------\n"
                      "color : tuple or str or int\n"
                      "    Pen color. An (R, G, B) tuple takes its alpha from `opacity`; an\n"
                      "    (R, G, B, A) tuple sets alpha directly and overrides `opacity`.\n"
                      "    Strings are resolved by PIL.ImageColor.getrgb, so CSS color names,\n"
                      "    '#rgb', '#rrggbb' and 'rgb(...)' all work; forms resolving to four\n"
                      "    components such as 'rgba(...)' do not. An integer from 0 to 255 is a\n"
                      "    shade of gray, and out-of-range values wrap (300 gives the same gray\n"
                      "    as 44). An unrecognized color is silently treated as black.\n"
                      "width : float, optional\n"
                      "    Pen width. Default 1.\n"
                      "opacity : int, optional\n"
                      "    Pen opacity. Default 255.\n";

static PyObject*
pen_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    PenObject* self;

    PyObject* color;
    float width = 1.0;
    int opacity = 255;
    static const char* const kwlist[] = { "color", "width", "opacity", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|fi:Pen", const_cast<char **>(kwlist),
                                     &color, &width, &opacity))
        return NULL;

    self = (PenObject*) type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);
    self->width = width;

    return (PyObject*) self;
}

static void
pen_dealloc(PenObject* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    tp->tp_free((PyObject*) self);
    Py_DECREF(tp);
}

static PyObject*
pen_get_color(PenObject* self, void* closure)
{
    return Py_BuildValue(
        "(BBBB)", self->color.r, self->color.g, self->color.b, self->color.a
        );
}

static PyObject*
pen_get_width(PenObject* self, void* closure)
{
    return PyFloat_FromDouble(self->width);
}

static PyGetSetDef pen_getset[] = {
    {"color", (getter) pen_get_color, NULL,
     (char*) "tuple: The pen color, as resolved, in (R, G, B, A) form.", NULL},
    {"width", (getter) pen_get_width, NULL,
     (char*) "float: The width of the pen.", NULL},
    {NULL}
};

static PyType_Slot pen_slots[] = {
    {Py_tp_new, (void*) pen_new},
    {Py_tp_dealloc, (void*) pen_dealloc},
    {Py_tp_getset, (void*) pen_getset},
    {Py_tp_doc, DOC_SLOT(pen_doc)},
    {0, NULL}
};

static PyType_Spec pen_spec = {
    "aggdraw._aggdraw.Pen",
    sizeof(PenObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    pen_slots
};

/* -------------------------------------------------------------------- */

const char *brush_doc = "Creates a brush object.\n"
                        "\n"
                        "Parameters\n"
                        "----------\n"
                        "color : tuple or str or int\n"
                        "    Brush color. An (R, G, B) tuple takes its alpha from `opacity`; an\n"
                        "    (R, G, B, A) tuple sets alpha directly and overrides `opacity`.\n"
                        "    Strings are resolved by PIL.ImageColor.getrgb, so CSS color names,\n"
                        "    '#rgb', '#rrggbb' and 'rgb(...)' all work; forms resolving to four\n"
                        "    components such as 'rgba(...)' do not. An integer from 0 to 255 is a\n"
                        "    shade of gray, and out-of-range values wrap (300 gives the same gray\n"
                        "    as 44). An unrecognized color is silently treated as black.\n"
                        "opacity : int, optional\n"
                        "    Brush opacity. Default 255.\n";

static PyObject*
brush_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    BrushObject* self;

    PyObject* color;
    int opacity = 255;
    static const char* const kwlist[] = { "color", "opacity", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|i:Brush", const_cast<char **>(kwlist),
                                     &color, &opacity))
        return NULL;

    self = (BrushObject*) type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);

    return (PyObject*) self;
}

static void
brush_dealloc(BrushObject* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    tp->tp_free((PyObject*) self);
    Py_DECREF(tp);
}

static PyObject*
brush_get_color(BrushObject* self, void* closure)
{
    return Py_BuildValue(
        "(BBBB)", self->color.r, self->color.g, self->color.b, self->color.a
        );
}

static PyGetSetDef brush_getset[] = {
    {"color", (getter) brush_get_color, NULL,
     (char*) "tuple: The brush color, as resolved, in (R, G, B, A) form.", NULL},
    {NULL}
};

static PyType_Slot brush_slots[] = {
    {Py_tp_new, (void*) brush_new},
    {Py_tp_dealloc, (void*) brush_dealloc},
    {Py_tp_getset, (void*) brush_getset},
    {Py_tp_doc, DOC_SLOT(brush_doc)},
    {0, NULL}
};

static PyType_Spec brush_spec = {
    "aggdraw._aggdraw.Brush",
    sizeof(BrushObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    brush_slots
};


/* -------------------------------------------------------------------- */

const char *font_doc = "Create a font object from a truetype font file for use with `text` and `textsize`.\n"
                       "\n"
                       "Parameters\n"
                       "----------\n"
                       "color : tuple or str or int\n"
                       "    Font color. An (R, G, B) tuple takes its alpha from `opacity`; an\n"
                       "    (R, G, B, A) tuple sets alpha directly and overrides `opacity`.\n"
                       "    Strings are resolved by PIL.ImageColor.getrgb, so CSS color names,\n"
                       "    '#rgb', '#rrggbb' and 'rgb(...)' all work; forms resolving to four\n"
                       "    components such as 'rgba(...)' do not. An integer from 0 to 255 is a\n"
                       "    shade of gray, and out-of-range values wrap (300 gives the same gray\n"
                       "    as 44). An unrecognized color is silently treated as black.\n"
                       "file : str\n"
                       "    Font source file.\n"
                       "size : float, optional\n"
                       "    Font size in pixels. Default 12.\n"
                       "opacity : int, optional\n"
                       "    Font opacity. Default 255.\n";

static PyObject*
font_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    PyObject* color;
    char* filename;
    float size = 12;
    int opacity = 255;
    static const char* const kwlist[] = { "color", "file", "size", "opacity", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kw, "Os|fi:Font", const_cast<char **>(kwlist),
                                     &color, &filename, &size, &opacity))
        return NULL;

#if defined(HAVE_FREETYPE2)
    FontObject* self = (FontObject*) type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);
    self->filename = new char[strlen(filename)+1];
    strcpy(self->filename, filename);

    self->height = size;

    if (!font_load(self)) {
        PyErr_SetString(PyExc_IOError, "cannot load font");
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject*) self;
#else
    PyErr_SetString(PyExc_IOError, "cannot load font (no text renderer)");
    return NULL;
#endif
}

#if defined(HAVE_FREETYPE2)
static FT_Face
font_load(FontObject* font, bool outline)
{
    if (outline)
        font_engine.load_font(font->filename, 0, agg::glyph_ren_outline);
    else
        font_engine.load_font(font->filename, 0, agg::glyph_ren_native_gray8);

    font_engine.flip_y(1);
    font_engine.height(font->height);

    // requires patch to "agg2\font_freetype\agg_font_freetype.h"
    // the patch should simply expose the m_cur_face variable
    return font_engine.m_cur_face;
}
#endif

#if defined(HAVE_FREETYPE2)
/* Each of these reloads the face: FontObject stores only the filename, and the
   shared font_engine may have been pointed at a different font since. A face
   that will not load yields None rather than an exception, as it always has. */
static PyObject*
font_get_family(FontObject* self, void* closure)
{
    FT_Face face = font_load(self);
    if (!face) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyUnicode_FromString(face->family_name);
}

static PyObject*
font_get_style(FontObject* self, void* closure)
{
    FT_Face face = font_load(self);
    if (!face) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyUnicode_FromString(face->style_name);
}

static PyObject*
font_get_ascent(FontObject* self, void* closure)
{
    FT_Face face = font_load(self);
    if (!face) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyFloat_FromDouble(face->size->metrics.ascender/64.0);
}

static PyObject*
font_get_descent(FontObject* self, void* closure)
{
    FT_Face face = font_load(self);
    if (!face) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyFloat_FromDouble(-face->size->metrics.descender/64.0);
}
#endif

static PyGetSetDef font_getset[] = {
#if defined(HAVE_FREETYPE2)
    {"family", (getter) font_get_family, NULL,
     (char*) "str: The font family name reported by FreeType.", NULL},
    {"style", (getter) font_get_style, NULL,
     (char*) "str: The font style name reported by FreeType.", NULL},
    {"ascent", (getter) font_get_ascent, NULL,
     (char*) "float: The font ascent, in pixels.", NULL},
    {"descent", (getter) font_get_descent, NULL,
     (char*) "float: The font descent, in pixels, as a positive number.", NULL},
#endif
    {NULL}
};

static PyType_Slot font_slots[] = {
    {Py_tp_new, (void*) font_new},
    {Py_tp_dealloc, (void*) font_dealloc},
    {Py_tp_getset, (void*) font_getset},
    {Py_tp_doc, DOC_SLOT(font_doc)},
    {0, NULL}
};

static PyType_Spec font_spec = {
    "aggdraw._aggdraw.Font",
    sizeof(FontObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    font_slots
};

static void
font_dealloc(FontObject* self)
{
    delete [] self->filename;

    PyTypeObject* tp = Py_TYPE(self);
    tp->tp_free((PyObject*) self);
    Py_DECREF(tp);
}

/* -------------------------------------------------------------------- */

const char *path_doc = "Create a Path object (experimental).\n"
                       "\n"
                       "Parameters\n"
                       "----------\n"
                       "xy : iterable, optional\n"
                       "    A Python sequence (x, y, x, y, ...) used to initialize the\n"
                       "    path. The path moves to the first coordinate pair and adds a\n"
                       "    line segment to each remaining pair.\n";

static PyObject*
path_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    if (kw != NULL && PyDict_GET_SIZE(kw) != 0) {
        PyErr_SetString(PyExc_TypeError, "Path() takes no keyword arguments");
        return NULL;
    }

    PyObject* xyIn = NULL;
    if (!PyArg_ParseTuple(args, "|O:Path", &xyIn))
        return NULL;

    PathObject* self = (PathObject*) type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->path = new agg::path_storage();

    if (xyIn) {
        int count;
        PointF *xy = getpoints(xyIn, &count);
        if (!xy) {
            Py_DECREF(self);
            return NULL;
        }
        self->path->move_to(xy[0].X, xy[0].Y);
        for (int i = 1; i < count; i++)
            self->path->line_to(xy[i].X, xy[i].Y);
        delete [] xy;
    }

    return (PyObject*) self;
}

/* Defined below, with the rest of the Path helpers. */
void expandPaths(PathObject* self);

/* The SVG-descriptor parser shared by Path.from_svg() and Symbol(). `type` is
   PathType, SymbolType, or a Python subclass of either; allocating through
   type->tp_alloc gives the caller an instance of the class it asked for. */
static PyObject*
path_from_svg_impl(PyTypeObject* type, char* path, float scale)
{
    PathObject* self = (PathObject*) type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->path = new agg::path_storage();

    char op = 0;
    char *p, *q, *e;
    double x, y, x1, y1, x2, y2;
    bool curve = 0;

    p = path;
    e = path + strlen(path);

#define COMMA_WSP\
    do { while (isspace(*p)) p++; if (*p == ',') p++; } while (0)

    /* sloppy SVG-style path parser */
    while (p < e) {
        while (isspace(*p))
            p++;
        if (!*p)
            break;
        else if (isalpha(*p)) {
            op = *p++;
        } else {
            if (!op) {
                PyErr_Format(
                    PyExc_ValueError, "no command at start of path"
                    );
                Py_DECREF(self);
                return NULL;
            }
            COMMA_WSP;
        }
        q = p; /* start of arguments */
        switch (op) {
        case 'M':
        case 'm':
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 'm')
                self->path->rel_to_abs(&x, &y);
            self->path->move_to(x, y);
            break;
        case 'L':
        case 'l':
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 'l')
                self->path->rel_to_abs(&x, &y);
            self->path->line_to(x, y);
            break;
        case 'h':
        case 'H':
            x = strtod(p, &p) * scale;
            if (self->path->last_vertex(&x2, &y2) > 0) {
                if (op == 'h')
                    x += x2;
                self->path->line_to(x, y2);
            }
            break;
        case 'v':
        case 'V':
            y = strtod(p, &p) * scale;
            if (self->path->last_vertex(&x2, &y2) > 0) {
                if (op == 'v')
                    y += y2;
                self->path->line_to(x2, y);
            }
            break;
        case 'c':
        case 'C':
            /* cubic bezier (postscript-style) */
            x1 = strtod(p, &p) * scale; COMMA_WSP;
            y1 = strtod(p, &p) * scale; COMMA_WSP;
            x2 = strtod(p, &p) * scale; COMMA_WSP;
            y2 = strtod(p, &p) * scale; COMMA_WSP;
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 'c') {
                self->path->rel_to_abs(&x1, &y1);
                self->path->rel_to_abs(&x2, &y2);
                self->path->rel_to_abs(&x, &y);
            }
            self->path->curve4(x1, y1, x2, y2, x, y);
            curve = true;
            break;
        case 's':
        case 'S':
            /* smooth cubic bezier (postscript-style) */
            x2 = strtod(p, &p) * scale; COMMA_WSP;
            y2 = strtod(p, &p) * scale; COMMA_WSP;
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 's') {
                self->path->rel_to_abs(&x2, &y2);
                self->path->rel_to_abs(&x, &y);
            }
            if (self->path->last_vertex(&x1, &y1)) {
                /* find control segment in previous curve */
                double x0, y0;
                if (self->path->prev_vertex(&x0, &y0)) {
                    x1 += x1 - x0;
                    y1 += y1 - y0;
                    self->path->curve4(x1, y1, x2, y2, x, y);
                }
            }
            curve = true;
            break;
        case 'q':
        case 'Q':
            /* quadratic bezier (truetype-style) */
            x1 = strtod(p, &p) * scale; COMMA_WSP;
            y1 = strtod(p, &p) * scale; COMMA_WSP;
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 'q') {
                self->path->rel_to_abs(&x1, &y1);
                self->path->rel_to_abs(&x, &y);
            }
            self->path->curve3(x1, y1, x, y);
            curve = true;
            break;
        case 't':
        case 'T':
            /* smooth quadratic bezier */
            x = strtod(p, &p) * scale; COMMA_WSP;
            y = strtod(p, &p) * scale;
            if (op == 't')
                self->path->rel_to_abs(&x, &y);
            if (self->path->last_vertex(&x1, &y1)) {
                /* find control segment in previous curve */
                double x0, y0;
                if (self->path->prev_vertex(&x0, &y0)) {
                    x1 += x1 - x0;
                    y1 += y1 - y0;
                    self->path->curve3(x1, y1, x, y);
                }
            }
            curve = true;
            break;
        case 'Z':
        case 'z':
            p++;
            self->path->end_poly();
            break;
        default:
            PyErr_Format(
                PyExc_ValueError,
                "unknown path command '%c'", op
                );
            Py_DECREF(self);
            return NULL;
        }
        if (p == q) {
            PyErr_Format(
                PyExc_ValueError,
                "invalid arguments for command '%c'", op
                );
            Py_DECREF(self);
            return NULL;
        }
    }

    if (curve)
        expandPaths(self);

    return (PyObject*) self;
}

const char *path_from_svg_doc = "Create a path from an SVG-style path descriptor.\n"
                                "\n"
                                "This is a classmethod, so calling it on a subclass returns an\n"
                                "instance of that subclass. It builds the object directly and does\n"
                                "not go through the class's own constructor.\n"
                                "\n"
                                "Parameters\n"
                                "----------\n"
                                "path : str\n"
                                "    An SVG-style path descriptor. The following operators\n"
                                "    are supported: M (move), L (line), H (horizontal line), V (vertical line),\n"
                                "    C (cubic bezier), S (smooth cubic bezier), Q (quadratic bezier),\n"
                                "    T (smooth quadratic bezier), and Z (close path). Use lower-case\n"
                                "    operators for relative coordinates, upper-case for absolute coordinates.\n"
                                "scale : float, optional\n"
                                "    A multiplier applied to every coordinate in the path descriptor\n"
                                "    as it is parsed. Default 1.0.\n"
                                "\n"
                                "Returns\n"
                                "-------\n"
                                "Path\n"
                                "    A new instance of the class this was called on.\n";

static PyObject*
path_from_svg(PyObject* cls, PyObject* args)
{
    char* path;
    float scale = 1.0;
    if (!PyArg_ParseTuple(args, "s|f:from_svg", &path, &scale))
        return NULL;

    /* The classmethod descriptor has already checked that cls is a subtype of
       Path, so path_from_svg_impl can allocate from it unconditionally. */
    return path_from_svg_impl((PyTypeObject*) cls, path, scale);
}

void expandPaths(PathObject *self)
{
    agg::path_storage* path = self->path;
    agg::conv_curve<agg::path_storage> curve(*path);
    self->path = new agg::path_storage();
    self->path->add_path(curve, 0, false);
    delete path;
}

const char *path_moveto_doc = "Move the path pointer to the given location.\n"
                              "\n"
                              "Parameters\n"
                              "----------\n"
                              "x\n"
                              "y\n";

static PyObject*
path_moveto(PathObject* self, PyObject* args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd:moveto", &x, &y))
        return NULL;

    self->path->move_to(x, y);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_rmoveto_doc = "Move the path pointer to the given location, relative to the current position.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "x\n"
                               "y\n";

static PyObject*
path_rmoveto(PathObject* self, PyObject* args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd:rmoveto", &x, &y))
        return NULL;

    self->path->rel_to_abs(&x, &y);
    self->path->move_to(x, y);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_lineto_doc = "Adds a line segment to the path.\n"
                              "\n"
                              "Parameters\n"
                              "----------\n"
                              "x\n"
                              "y\n";

static PyObject*
path_lineto(PathObject* self, PyObject* args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd:lineto", &x, &y))
        return NULL;

    self->path->line_to(x, y);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_rlineto_doc = "Adds a line segment to the path, using relative coordinates.\n"
                               "\n"
                               "Same as lineto, but the coordinates are relative to the current position.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "x\n"
                               "y\n";

static PyObject*
path_rlineto(PathObject* self, PyObject* args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd:rlineto", &x, &y))
        return NULL;

    self->path->rel_to_abs(&x, &y);
    self->path->line_to(x, y);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_curveto_doc = "Adds a cubic bezier curve segment to the path.\n"
                               "\n"
                               "(x1, y1) and (x2, y2) are the control points, (x, y) is the end point.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "x1\n"
                               "y1\n"
                               "x2\n"
                               "y2\n"
                               "x\n"
                               "y\n";

static PyObject*
path_curveto(PathObject* self, PyObject* args)
{
    double x1, y1, x2, y2, x, y;
    if (!PyArg_ParseTuple(args, "dddddd:curveto", &x1, &y1, &x2, &y2, &x, &y))
        return NULL;

    self->path->curve4(x1, y1, x2, y2, x, y);

    expandPaths(self);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_rcurveto_doc = "Adds a bezier curve segment to the path, using relative coordinates.\n"
                                "\n"
                                "Same as curveto, but the coordinates are relative to the current position.\n"
                                "\n"
                                "Parameters\n"
                                "----------\n"
                                "x1\n"
                                "y1\n"
                                "x2\n"
                                "y2\n"
                                "x\n"
                                "y\n";

static PyObject*
path_rcurveto(PathObject* self, PyObject* args)
{
    double x1, y1, x2, y2, x, y;
    if (!PyArg_ParseTuple(args, "dddddd:rcurveto", &x1, &y1, &x2, &y2, &x, &y))
        return NULL;

    self->path->rel_to_abs(&x1, &y1);
    self->path->rel_to_abs(&x2, &y2);
    self->path->rel_to_abs(&x, &y);

    self->path->curve4(x1, y1, x2, y2, x, y);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_close_doc = "Close the current path.";

static PyObject*
path_close(PathObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    self->path->close_polygon(0);
    /* expand curves */
    agg::path_storage* path = self->path;
    agg::conv_curve<agg::path_storage> curve(*path);
    self->path = new agg::path_storage();
    self->path->add_path(curve, 0, false);
    delete path;

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_polygon_doc = "Adds a closed polygon subpath to the path.\n"
                               "\n"
                               "Moves to the first coordinate pair, adds a line segment to each\n"
                               "remaining pair, then closes the subpath.\n"
                               "\n"
                               "Parameters\n"
                               "----------\n"
                               "xy : iterable\n"
                               "    A Python sequence (x, y, x, y, ...).\n";

static PyObject*
path_polygon(PathObject* self, PyObject* args)
{
    PyObject* xyIn;
    if (!PyArg_ParseTuple(args, "O:polygon", &xyIn))
        return NULL;

    int count;
    PointF *xy = getpoints(xyIn, &count);
    if (!xy)
        return NULL;

    agg::path_storage path;

    path.move_to(xy[0].X, xy[0].Y);
    for (int i = 1; i < count; i++)
        path.line_to(xy[i].X, xy[i].Y);
    path.close_polygon();
    delete [] xy;

    self->path->add_path(path, 0, false);

    Py_INCREF(Py_None);
    return Py_None;
}

const char *path_coords_doc = "Returns the coordinates for this path.\n"
                              "\n"
                              "Curves are flattened before being returned.\n";

/* Append a float to a list, releasing the temporary. PyList_Append takes its
   own reference, so the one returned by PyFloat_FromDouble must be dropped or
   every coordinate leaks an object. */
static int
append_float(PyObject* list, double value)
{
    PyObject* item = PyFloat_FromDouble(value);
    if (!item)
        return -1;
    int status = PyList_Append(list, item);
    Py_DECREF(item);
    return status;
}

static PyObject*
path_coords(PathObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ":coords"))
        return NULL;

    agg::conv_curve<agg::path_storage> curve(*self->path);

    curve.rewind(0);
    curve.approximation_scale(1);

    PyObject* list;

    list = PyList_New(0);
    if (!list)
        return NULL;

    double x, y;
    unsigned cmd;
    while (!agg::is_stop(cmd = curve.vertex(&x, &y))) {
        if (agg::is_vertex(cmd)) {
            if (append_float(list, x) < 0 || append_float(list, y) < 0) {
                Py_DECREF(list);
                return NULL;
            }
        }
    }

    return list;
}

static void
path_dealloc(PathObject* self)
{
    delete self->path;

    PyTypeObject* tp = Py_TYPE(self);
    tp->tp_free((PyObject*) self);
    Py_DECREF(tp);
}

static PyMethodDef path_methods[] = {

    {"from_svg", (PyCFunction) path_from_svg,
     METH_VARARGS | METH_CLASS, path_from_svg_doc},

    {"lineto", (PyCFunction) path_lineto, METH_VARARGS, path_lineto_doc},
    {"rlineto", (PyCFunction) path_rlineto, METH_VARARGS, path_rlineto_doc},
    {"curveto", (PyCFunction) path_curveto, METH_VARARGS, path_curveto_doc},
    {"rcurveto", (PyCFunction) path_rcurveto, METH_VARARGS, path_rcurveto_doc},
    {"moveto", (PyCFunction) path_moveto, METH_VARARGS, path_moveto_doc},
    {"rmoveto", (PyCFunction) path_rmoveto, METH_VARARGS, path_rmoveto_doc},

    {"close", (PyCFunction) path_close, METH_VARARGS, path_close_doc},

    {"polygon", (PyCFunction) path_polygon, METH_VARARGS, path_polygon_doc},

    {"coords", (PyCFunction) path_coords, METH_VARARGS, path_coords_doc},

    {NULL, NULL}
};

static PyType_Slot path_slots[] = {
    {Py_tp_new, (void*) path_new},
    {Py_tp_dealloc, (void*) path_dealloc},
    {Py_tp_methods, (void*) path_methods},
    {Py_tp_doc, DOC_SLOT(path_doc)},
    {0, NULL}
};

static PyType_Spec path_spec = {
    "aggdraw._aggdraw.Path",
    sizeof(PathObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    path_slots
};

/* -------------------------------------------------------------------- */

const char *symbol_doc = "Deprecated alias for :meth:`Path.from_svg`.\n"
                         "\n"
                         "A Symbol is a Path built from an SVG-style path descriptor rather\n"
                         "than from a coordinate sequence. It adds no state and no methods of\n"
                         "its own; use Path.from_svg(path, scale) instead. The documented\n"
                         "aggdraw.Symbol wrapper warns when one is constructed.\n"
                         "\n"
                         "Parameters\n"
                         "----------\n"
                         "path : str\n"
                         "    An SVG-style path descriptor; see Path.from_svg for the\n"
                         "    supported operators.\n"
                         "scale : float, optional\n"
                         "    A multiplier applied to every coordinate in the path descriptor\n"
                         "    as it is parsed. Default 1.0.\n";

static PyObject*
symbol_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
    /* tp_new is handed keywords whether or not it wants them. The old
       METH_VARARGS entry point rejected them for free; keep doing so rather
       than silently ignoring Symbol("M0,0", scale=2). */
    if (kw != NULL && PyDict_GET_SIZE(kw) != 0) {
        PyErr_SetString(PyExc_TypeError, "Symbol() takes no keyword arguments");
        return NULL;
    }

    char* path;
    float scale = 1.0;
    if (!PyArg_ParseTuple(args, "s|f:Symbol", &path, &scale))
        return NULL;

    return path_from_svg_impl(type, path, scale);
}

/* tp_dealloc, tp_methods, tp_alloc and tp_free are all inherited from Path.
   path_dealloc reads Py_TYPE(self) rather than a hardcoded type, so it is
   already correct for a subtype, and Symbol adds no fields to PathObject --
   hence the basicsize below. Do not add tp_methods here: that would build a
   second set of descriptors bound to SymbolType, and Symbol.lineto() would
   then reject a plain Path. tp_doc is not inherited, so it must be given. */
static PyType_Slot symbol_slots[] = {
    {Py_tp_new, (void*) symbol_new},
    {Py_tp_doc, DOC_SLOT(symbol_doc)},
    {0, NULL}
};

static PyType_Spec symbol_spec = {
    "aggdraw._aggdraw.Symbol",
    sizeof(PathObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    symbol_slots
};

/* -------------------------------------------------------------------- */

const char *mod_doc = "Python interface to the Anti-Grain Graphics Drawing library\n"
                      "\n"
                      "The aggdraw module implements the basic WCK 2D Drawing Interface on top\n"
                      "of `the AGG library <http://www.antigrain.com/>`_. This library supports\n"
                      "anti-aliasing and alpha compositing,\n"
                      "but is otherwise fully compatible with the WCK renderer.\n"
                      "\n"
                      "Examples\n"
                      "--------\n"
                      "\n"
                      "    >>> # draw cross on top of PIL image\n"
                      "    >>> d = aggdraw.Draw(im)\n"
                      "    >>> p = aggdraw.Pen(\"black\", 0.5)\n"
                      "    >>> d.line((0, 0, 500, 500), p)\n"
                      "    >>> d.line((0, 500, 500, 0), p)\n"
                      "    >>> d.flush()\n"
                      "\n"
                      "    >>> # draw cross on internal image memory\n"
                      "    >>> d = aggdraw.Draw(\"RGB\", (320, 200), \"white\")\n"
                      "    >>> p = aggdraw.Pen(\"black\", 0.5)\n"
                      "    >>> d.line((0, 0, 500, 500), p)\n"
                      "    >>> d.line((0, 500, 500, 0), p)\n"
                      "    >>> s = d.tobytes()\n";

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_aggdraw",
        mod_doc,
        -1,
        NULL,       /* m_methods: every name in this module is a type */
        NULL,
        NULL,
        NULL,
        NULL,
};


static PyObject *
aggdraw_init(void)
{
    PyObject *module = PyModule_Create(&moduledef);
    if (module == NULL)
        return NULL;

    /* Build the six types from their specs and expose them as real classes.
       PyType_FromSpec fills in ob_type, so type() and help() work; each spec
       names its own tp_new, so the types are directly constructible and
       Py_TPFLAGS_BASETYPE lets them be subclassed. The deallocators go through
       tp_free and drop a reference on the type, as heap types require.

       A row naming a base gets it through PyType_FromSpecWithBases rather than
       a Py_tp_base slot, because the base is itself a heap type that does not
       exist until this loop creates it -- a static slot array cannot name it.
       PyType_FromSpecWithBases(spec, NULL) is exactly PyType_FromSpec(spec),
       so the rows without a base are unaffected. */
    static const struct {
        PyType_Spec* spec;
        PyTypeObject** slot;
        const char* name;
        PyTypeObject** base;    /* NULL for a direct subclass of object */
    } types[] = {
        {&draw_spec, &DrawType, "Draw", NULL},
        {&pen_spec, &PenType, "Pen", NULL},
        {&brush_spec, &BrushType, "Brush", NULL},
        {&font_spec, &FontType, "Font", NULL},
        {&path_spec, &PathType, "Path", NULL},
        /* Ordering matters: a row naming a base must come after the row that
           creates it, since the base is read from *types[i].base right here. */
        {&symbol_spec, &SymbolType, "Symbol", &PathType},
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        PyObject* bases = NULL;
        if (types[i].base != NULL) {
            bases = Py_BuildValue("(O)", (PyObject*) *types[i].base);
            if (bases == NULL) {
                Py_DECREF(module);
                return NULL;
            }
        }
        PyObject* type = PyType_FromSpecWithBases(types[i].spec, bases);
        Py_XDECREF(bases);  /* the new type holds its own reference */
        if (type == NULL) {
            Py_DECREF(module);
            return NULL;
        }
        *types[i].slot = (PyTypeObject*) type;
        if (PyModule_AddObjectRef(module, types[i].name, type) < 0) {
            Py_DECREF(type);
            Py_DECREF(module);
            return NULL;
        }
        /* The module holds its own reference; the file-scope pointer above
           keeps one for the C code's own type checks. */
    }

    PyObject *version = PyUnicode_FromString(QUOTE(VERSION));
    if (version == NULL) {
        Py_DECREF(module);
        return NULL;
    }
    int rc = PyObject_SetAttrString(module, "VERSION", version);
    if (rc == 0)
        rc = PyObject_SetAttrString(module, "__version__", version);
    Py_DECREF(version);
    if (rc < 0) {
        Py_DECREF(module);
        return NULL;
    }

    PyObject* g = PyDict_New();
    if (g == NULL) {
        Py_DECREF(module);
        return NULL;
    }
    if (PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins()) < 0) {
        Py_DECREF(g);
        Py_DECREF(module);
        return NULL;
    }
    PyObject* result = PyRun_String(
        "try:\n"
        "    from PIL import ImageColor\n"
        "except ImportError:\n"
        "    ImageColor = None\n"

        "def getcolor(v):\n" // FIXME: add caching (?)
        "    return ImageColor.getrgb(v)\n"

        "", Py_file_input, g, NULL

        );
    if (result == NULL) {
        Py_DECREF(g);
        Py_DECREF(module);
        return NULL;
    }
    Py_DECREF(result);

    /* PyDict_GetItemString returns a borrowed reference, so take a strong one
       of our own before releasing the dict that owns it. */
    aggdraw_getcolor_obj = PyDict_GetItemString(g, "getcolor");
    Py_XINCREF(aggdraw_getcolor_obj);
    Py_DECREF(g);

#ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(module, Py_MOD_GIL_NOT_USED);
#endif

    return module;
}

PyMODINIT_FUNC
PyInit__aggdraw(void)
{
    return aggdraw_init();
}
