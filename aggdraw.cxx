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

#include "Python.h"
#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#define HAVE_UNICODE
#endif
#include "bytesobject.h"

#if defined(PY_VERSION_HEX) && PY_VERSION_HEX >= 0x01060000
#if PY_VERSION_HEX  < 0x02020000 || defined(Py_USING_UNICODE)
/* defining this enables unicode support (default under 1.6a1 and later) */
#define HAVE_UNICODE
#endif
#endif

/* agg2 components */
#include "agg_arc.h"
#include "agg_conv_contour.h"
#include "agg_conv_curve.h"
// #include "agg_conv_dash.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_ellipse.h"
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
#if defined(WIN32)
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    BITMAPINFO info;
#endif
} DrawObject;

#ifndef Py_TYPE
    #define Py_TYPE(ob) (((PyObject*)(ob))->ob_type)
#endif

/* glue functions (see the init function for details) */
static PyObject* aggdraw_getcolor_obj;

static void draw_dealloc(DrawObject* self);
#ifdef IS_PY3K
static PyObject* draw_getattro(DrawObject* self, PyObject* nameobj);
static PyTypeObject DrawType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Draw", sizeof(DrawObject), 0,
    /* methods */
    (destructor) draw_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_reserved */
    0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    0, /* tp_hash*/
    0, /* tp_call*/
    0, /* tp_str*/
    (getattrofunc)draw_getattro, /* tp_getattro */
};
#else

static PyObject* draw_getattr(DrawObject* self, char* name);
static PyTypeObject DrawType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Draw", sizeof(DrawObject), 0,
    /* methods */
    (destructor) draw_dealloc, /* tp_dealloc */
    (printfunc)0, /* tp_print */
    (getattrfunc)draw_getattr, /* tp_getattr */
    0, /* tp_setattr */
};
#endif

typedef struct {
    PyObject_HEAD
    agg::rgba8 color;
    float width;
} PenObject;

static void pen_dealloc(PenObject* self);

#ifdef IS_PY3K
static PyTypeObject PenType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Pen", sizeof(PenObject), 0,
    /* methods */
    (destructor) pen_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
};

#else
static PyTypeObject PenType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Pen", sizeof(PenObject), 0,
    /* methods */
    (destructor) pen_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
};
#endif

#define Pen_Check(op) ((op) != NULL && Py_TYPE(op) == &PenType)

typedef struct {
    PyObject_HEAD
    agg::rgba8 color;
} BrushObject;

static void brush_dealloc(BrushObject* self);

#ifdef IS_PY3K
static PyTypeObject BrushType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Brush", sizeof(BrushObject), 0,
    /* methods */
    (destructor) brush_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
};

#else
static PyTypeObject BrushType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Brush", sizeof(BrushObject), 0,
    /* methods */
    (destructor) brush_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
};

#endif
#define Brush_Check(op) ((op) != NULL && Py_TYPE(op) == &BrushType)

typedef struct {
    PyObject_HEAD
    char* filename;
    float height;
    agg::rgba8 color;
} FontObject;

#if defined(HAVE_FREETYPE2)
static FT_Face font_load(FontObject* font, bool outline=false);
#endif

static void font_dealloc(FontObject* self);
#ifdef IS_PY3K
static PyObject* font_getattro(FontObject* self, PyObject* nameobj);
static PyTypeObject FontType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Font", sizeof(FontObject), 0,
    /* methods */
    (destructor) font_dealloc, /* tp_dealloc */
    (printfunc)0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_reserved */
    (reprfunc)0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    (hashfunc)0,  /*tp_hash*/
    (ternaryfunc)0,  /*tp_call*/
    (reprfunc)0,  /*tp_str*/
    (getattrofunc)font_getattro, /* tp_getattro */
};
#else
static PyObject* font_getattr(FontObject* self, char* name);
static PyTypeObject FontType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Font", sizeof(FontObject), 0,
    /* methods */
    (destructor) font_dealloc, /* tp_dealloc */
    0, /* tp_print */
    (getattrfunc) font_getattr, /* tp_getattr */
    0, /* tp_setattr */
};
#endif

#define Font_Check(op) ((op) != NULL && Py_TYPE(op) == &FontType)

typedef struct {
    PyObject_HEAD
    agg::path_storage* path;
} PathObject;

static void path_dealloc(PathObject* self);
#ifdef IS_PY3K
static PyObject* path_getattro(PathObject* self, PyObject* nameobj);
static PyTypeObject PathType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Path", sizeof(PathObject), 0,
    /* methods */
    (destructor) path_dealloc, /* tp_dealloc */
    (printfunc)0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_reserved */
    (reprfunc)0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    (hashfunc)0,  /*tp_hash*/
    (ternaryfunc)0,  /*tp_call*/
    (reprfunc)0,  /*tp_str*/
    (getattrofunc)path_getattro, /* tp_getattro */
};
#else
static PyObject* path_getattr(PathObject* self, char* name);
static PyTypeObject PathType = {
    PyObject_HEAD_INIT(NULL)
    0, "Path", sizeof(PathObject), 0,
    /* methods */
    (destructor) path_dealloc, /* tp_dealloc */
    0, /* tp_print */
    (getattrfunc) path_getattr, /* tp_getattr */
    0, /* tp_setattr */
};
#endif

#define Path_Check(op) ((op) != NULL && Py_TYPE(op) == &PathType)

static agg::rgba8 getcolor(PyObject* color, int opacity=255);

/* -------------------------------------------------------------------- */

#if defined(HAVE_FREETYPE2)
static int
text_getchar(PyObject* string, int index, unsigned long* char_out)
{
#if defined(HAVE_UNICODE)
    if (PyUnicode_Check(string)) {
        Py_UNICODE* p = PyUnicode_AS_UNICODE(string);
        int size = PyUnicode_GET_SIZE(string);
        if (index >= size)
            return 0;
        *char_out = p[index];
        return 1;
    }
#endif
    if (PyBytes_Check(string)) {
        unsigned char* p = (unsigned char*) PyBytes_AS_STRING(string);
        int size = PyBytes_GET_SIZE(string);
        if (index >= size)
            return 0;
        *char_out = (unsigned char) p[index];
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
            index++;
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

static PyObject*
draw_new(PyObject* self_, PyObject* args)
{
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
        if (PyBytes_Check(mode_obj)) {
            strncpy(buffer, PyBytes_AS_STRING(mode_obj), sizeof buffer);
            buffer[sizeof(buffer)-1] = '\0'; /* to be on the safe side */
            mode = buffer;
        } else if (PyUnicode_Check(mode_obj)) {
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

    DrawObject* self = PyObject_NEW(DrawObject, &DrawType);
    if (self == NULL)
        return NULL;

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
        PyObject_DEL(self);
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

    self->transform = NULL;

    self->image = image;
    if (image) {
        PyObject* buffer = PyObject_CallMethod(image, "tobytes", NULL);
        if (!buffer)
            return NULL; /* FIXME: release resources */
        if (!PyBytes_Check(buffer)) {
            PyErr_SetString(
                PyExc_TypeError,
                "bad 'tobytes' return value (expected string)"
                );
            Py_DECREF(buffer);
            return NULL;
        }
        char* data = PyBytes_AS_STRING(buffer);
        int data_size = PyBytes_GET_SIZE(buffer);
        if (data_size >= self->buffer_size)
            memcpy(self->buffer_data, data, self->buffer_size);
        else {
            PyErr_SetString(PyExc_ValueError, "not enough data");
            Py_DECREF(buffer);
            return NULL; /* FIXME: release resources */
        }
        Py_INCREF(image); /* hang on to this image */
        Py_DECREF(buffer);
    }

    draw_setup(self);

#if defined(WIN32)
    self->dc = NULL;
#endif

    return (PyObject*) self;
}

#if defined(WIN32)
static PyObject*
draw_dib(PyObject* self_, PyObject* args)
{
    char* mode;
    int xsize, ysize;
    PyObject* background = NULL;

    if (!PyArg_ParseTuple(args, "s(ii)|O:Dib", &mode, &xsize, &ysize, &background))
        return NULL;

    DrawObject* self = PyObject_NEW(DrawObject, &DrawType);
    if (self == NULL)
        return NULL;

    if (strcmp(mode, "RGB")) {
        PyErr_SetString(PyExc_ValueError, "bad mode");
        PyObject_DEL(self);
        return NULL;
    }

    int stride = xsize * 3;

    self->mode = agg::pix_format_bgr24;

    memset(&self->info, 0, sizeof(BITMAPINFOHEADER));
    self->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    self->info.bmiHeader.biWidth = xsize;
    self->info.bmiHeader.biHeight = ysize;
    self->info.bmiHeader.biPlanes = 1;
    self->info.bmiHeader.biBitCount = strlen(mode)*8;
    self->info.bmiHeader.biCompression = BI_RGB;

    /* Create DIB */
    self->dc = CreateCompatibleDC(NULL);
    if (!self->dc) {
        /* FIXME: cleanup */
	PyErr_NoMemory();
        return NULL;
    }

    void* bits;
    
    self->bitmap = CreateDIBSection(
        self->dc, &self->info, DIB_RGB_COLORS, &bits, NULL, 0
        );
    if (!self->bitmap) {
        /* FIXME: cleanup */
	PyErr_NoMemory();
        return NULL;
    }

    /* Bind the DIB to the device context */
    self->old_bitmap = SelectObject(self->dc, self->bitmap);

    self->buffer_size = ysize * stride;
    self->buffer_data = (unsigned char*) bits;

    Py_XINCREF(background);
    self->background = background;

    clear(self, background);

    self->buffer = new agg::rendering_buffer(
        self->buffer_data, xsize, ysize, -stride
        );

    self->xsize = xsize;
    self->ysize = ysize;

    self->transform = NULL;

    self->image = NULL;

    draw_setup(self);

    return (PyObject*) self;
}
#endif

struct PointF {
    float X;
    float Y;
};

#ifdef IS_PY3K
#define GETFLOAT(op)                                    \
    (PyLong_Check(op) ? (float) PyLong_AS_LONG((op)) :\
     PyFloat_Check(op) ? (float) PyFloat_AS_DOUBLE((op)) :\
     (float) PyFloat_AsDouble(op))
#else
#define GETFLOAT(op)                                    \
    (PyInt_Check(op) ? (float) PyInt_AS_LONG((op)) :\
     PyFloat_Check(op) ? (float) PyFloat_AS_DOUBLE((op)) :\
     (float) PyFloat_AsDouble(op))
#endif

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
#ifdef IS_PY3K
    if (PyLong_Check(color)) {
        int ink = PyLong_AsLong(color);
        return agg::rgba8(ink, ink, ink, opacity);
    }
#else
    if (PyInt_Check(color)) {
        int ink = PyInt_AsLong(color);
        return agg::rgba8(ink, ink, ink, opacity);
    }
#endif
    char buffer[10];
    char* ink = NULL;
    if (PyUnicode_Check(color)) {
        PyObject* ascii_color = PyUnicode_AsASCIIString(color);
        if (ascii_color == NULL) {
            ink = NULL;
        } else {
            strncpy(buffer, PyBytes_AsString(ascii_color), sizeof buffer);
            buffer[sizeof(buffer)-1] = '\0'; /* to be on the safe side */
            ink = buffer;
            Py_XDECREF(ascii_color);
        }
    } else if (PyBytes_Check(color)) {
        ink = PyBytes_AsString(color);
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
    if (PyUnicode_Check(color) || PyBytes_Check(color)) {
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
        delete xy;
        self->draw->draw(path, pen);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

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
        delete xy;
        self->draw->draw(path, pen, brush);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

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

static PyObject* 
draw_path(DrawObject* self, PyObject* args){
    PathObject* path;
    PyObject*   brush = NULL;
    PyObject*   pen   = NULL;

    if (!PyArg_ParseTuple(args, "O!|OO:path", &PathType, &path, &brush, &pen)){
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

static PyObject*
draw_symbol(DrawObject* self, PyObject* args)
{
    PyObject* xyIn;
    PathObject* symbol;
    PyObject* brush = NULL;
    PyObject* pen = NULL;
    if (!PyArg_ParseTuple(args, "OO!|OO:symbol",
                          &xyIn, &PathType, &symbol, &brush, &pen))
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

    delete xy;

    Py_INCREF(Py_None);
    return Py_None;
}

#if defined(HAVE_FREETYPE2)
static PyObject*
draw_text(DrawObject* self, PyObject* args)
{
    float xy[2];
    PyObject* text;
    FontObject* font;
    if (!PyArg_ParseTuple(args, "(ff)OO!:text", xy+0, xy+1, &text,
                          &FontType, &font))
        return NULL;

    self->draw->drawtext(xy, text, font);

    Py_INCREF(Py_None);
    return Py_None;
}
#endif

#if defined(HAVE_FREETYPE2)
static PyObject*
draw_textsize(DrawObject* self, PyObject* args)
{
    PyObject* text;
    FontObject* font;
    if (!PyArg_ParseTuple(args, "OO!:text", &text, &FontType, &font))
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

static PyObject*
draw_frombytes(DrawObject* self, PyObject* args)
{
    char* data = NULL; int data_size;
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

static PyObject*
draw_tobytes(DrawObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ":tobytes"))
        return NULL;

    return PyBytes_FromStringAndSize(
        (char*) self->buffer_data, self->buffer_size
        );
}

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

#if defined(WIN32)
static PyObject*
draw_expose(DrawObject* self, PyObject* args, PyObject* kw)
{
    static const char* const kwlist[] = {
        "", "hwnd", "hdc", NULL
    };
    PyObject* sentinel = NULL;
    int wnd = 0, dc = 0;
    if (!PyArg_ParseTupleAndKeywords(
        args, kw, "|Oii:expose", const_cast<char **>(kwlist), &sentinel, &wnd, &dc
        ))
        return NULL;

    if (sentinel || (wnd == 0 && dc == 0)) {
        PyErr_SetString(
            PyExc_TypeError, "expected 'hdc' or 'hwnd' keyword argument"
            );
        return NULL;
    }

    if (!self->dc) {
        PyErr_SetString(PyExc_TypeError, "cannot expose this object");
        return NULL;
    }

    HDC hdc;
    if (wnd)
        hdc = GetDC((HWND) wnd);
    else
        hdc = (HDC) dc;

    if (!hdc) {
        PyErr_SetString(PyExc_IOError, "cannot create device context");
        return NULL;
    }

    BitBlt(hdc, 0, 0, self->xsize, self->ysize, self->dc, 0, 0, SRCCOPY);

    if (wnd)
        ReleaseDC((HWND) wnd, hdc);

    Py_INCREF(Py_None);
    return Py_None;
}
#endif

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
#if defined(WIN32)
    if (self->dc) {
        if (self->bitmap) {
            SelectObject(self->dc, self->old_bitmap);
            DeleteObject(self->bitmap);
        }
        if (self->dc)
            DeleteDC(self->dc);
    }
#endif

    delete self->draw;
    delete self->buffer;
    delete [] self->buffer_data;

    Py_XDECREF(self->background);
    Py_XDECREF(self->image);

    PyObject_DEL(self);
}

static PyMethodDef draw_methods[] = {

    {"line", (PyCFunction) draw_line, METH_VARARGS},
    {"polygon", (PyCFunction) draw_polygon, METH_VARARGS},
    {"rectangle", (PyCFunction) draw_rectangle, METH_VARARGS},

#if defined(HAVE_FREETYPE2)
    {"text", (PyCFunction) draw_text, METH_VARARGS},
    {"textsize", (PyCFunction) draw_textsize, METH_VARARGS},
#endif

    {"path", (PyCFunction) draw_path, METH_VARARGS},
    {"symbol", (PyCFunction) draw_symbol, METH_VARARGS},

    {"arc", (PyCFunction) draw_arc, METH_VARARGS},
    {"chord", (PyCFunction) draw_chord, METH_VARARGS},
    {"ellipse", (PyCFunction) draw_ellipse, METH_VARARGS},
    {"pieslice", (PyCFunction) draw_pieslice, METH_VARARGS},

    {"settransform", (PyCFunction) draw_settransform, METH_VARARGS},
    {"setantialias", (PyCFunction) draw_setantialias, METH_VARARGS},

    {"flush", (PyCFunction) draw_flush, METH_VARARGS},

#if defined(WIN32)
    {"expose", (PyCFunction) draw_expose, METH_VARARGS|METH_KEYWORDS},
#endif

    {"clear", (PyCFunction) draw_clear, METH_VARARGS},

    {"frombytes", (PyCFunction) draw_frombytes, METH_VARARGS},
    {"tobytes", (PyCFunction) draw_tobytes, METH_VARARGS},

    {NULL, NULL}
};

#ifdef IS_PY3K

static PyObject*  
draw_getattro(DrawObject* self, PyObject* nameobj)
{
    if (!PyUnicode_Check(nameobj))
        goto generic;

    if (PyUnicode_CompareWithASCIIString(nameobj, "mode") == 0)
        return PyUnicode_FromString(self->draw->mode);
    if (PyUnicode_CompareWithASCIIString(nameobj, "size") == 0)
        return Py_BuildValue(
            "(ii)", self->buffer->width(), self->buffer->height()
            );
  generic:
    return PyObject_GenericGetAttr((PyObject*)self, nameobj);
}

#else

static PyObject*  
draw_getattr(DrawObject* self, char* name)
{
    if (!strcmp(name, "mode"))
        return PyBytes_FromString(self->draw->mode);
    if (!strcmp(name, "size"))
        return Py_BuildValue(
            "(ii)", self->buffer->width(), self->buffer->height()
            );
    return Py_FindMethod(draw_methods, (PyObject*) self, name);
}

#endif

/* -------------------------------------------------------------------- */

static PyObject*
pen_new(PyObject* self_, PyObject* args, PyObject* kw)
{
    PenObject* self;

    PyObject* color;
    float width = 1.0;
    int opacity = 255;
    static const char* const kwlist[] = { "color", "width", "opacity", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|fi:Pen", const_cast<char **>(kwlist),
                                     &color, &width, &opacity))
        return NULL;

    self = PyObject_NEW(PenObject, &PenType);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);
    self->width = width;

    return (PyObject*) self;
}

static void
pen_dealloc(PenObject* self)
{
    PyObject_DEL(self);
}

/* -------------------------------------------------------------------- */

static PyObject*
brush_new(PyObject* self_, PyObject* args, PyObject* kw)
{
    BrushObject* self;

    PyObject* color;
    int opacity = 255;
    static const char* const kwlist[] = { "color", "opacity", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|i:Brush", const_cast<char **>(kwlist),
                                     &color, &opacity))
        return NULL;

    self = PyObject_NEW(BrushObject, &BrushType);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);

    return (PyObject*) self;
}

static void
brush_dealloc(BrushObject* self)
{
    PyObject_DEL(self);
}


/* -------------------------------------------------------------------- */

static PyObject*
font_new(PyObject* self_, PyObject* args, PyObject* kw)
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
    FontObject* self = PyObject_NEW(FontObject, &FontType);

    if (self == NULL)
        return NULL;

    self->color = getcolor(color, opacity);
    self->filename = new char[strlen(filename)+1];
    strcpy(self->filename, filename);

    self->height = size;

    if (!font_load(self)) {
        PyErr_SetString(PyExc_IOError, "cannot load font");
        return NULL;
    }

    return (PyObject*) self;
#else
    PyErr_SetString(PyExc_IOError, "cannot load font (no text renderer)");
    return NULL;
#endif
}

static PyMethodDef font_methods[] = {
    {NULL, NULL}
};

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

#ifdef IS_PY3K
static PyObject*  
font_getattro(FontObject* self, PyObject* nameobj)
{
    if (!PyUnicode_Check(nameobj))
        goto generic;

#if defined(HAVE_FREETYPE2)
    FT_Face face;
    if (PyUnicode_CompareWithASCIIString(nameobj, "family") == 0) 
    {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyBytes_FromString(face->family_name);
    }
    if (PyUnicode_CompareWithASCIIString(nameobj, "style") == 0) 
    {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyBytes_FromString(face->style_name);
    }
    if (PyUnicode_CompareWithASCIIString(nameobj, "ascent") == 0) 
    {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyFloat_FromDouble(face->size->metrics.ascender/64.0);
    }
    if (PyUnicode_CompareWithASCIIString(nameobj, "descent") == 0) 
    {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyFloat_FromDouble(-face->size->metrics.descender/64.0);
    }
#endif
  generic:
    return PyObject_GenericGetAttr((PyObject*)self, nameobj);
}

#else        
static PyObject*  
font_getattr(FontObject* self, char* name)
{
#if defined(HAVE_FREETYPE2)
    FT_Face face;
    if (!strcmp(name, "family")) {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyBytes_FromString(face->family_name);
    }
    if (!strcmp(name, "style")) {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyBytes_FromString(face->style_name);
    }
    if (!strcmp(name, "ascent")) {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyFloat_FromDouble(face->size->metrics.ascender/64.0);
    }
    if (!strcmp(name, "descent")) {
        face = font_load(self);
        if (!face) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return PyFloat_FromDouble(-face->size->metrics.descender/64.0);
    }
#endif
    return Py_FindMethod(font_methods, (PyObject*) self, name);
}
#endif
    
static void
font_dealloc(FontObject* self)
{
    delete [] self->filename;
    PyObject_DEL(self);
}

/* -------------------------------------------------------------------- */

static PyObject*
path_new(PyObject* self_, PyObject* args)
{
    PyObject* xyIn = NULL;
    if (!PyArg_ParseTuple(args, "|O:Path", &xyIn))
        return NULL;

    PathObject* self = PyObject_NEW(PathObject, &PathType);

    if (self == NULL)
        return NULL;

    self->path = new agg::path_storage();

    if (xyIn) {
        int count;
        PointF *xy = getpoints(xyIn, &count);
        if (!xy) {
            path_dealloc(self);
            return NULL;
        }
        self->path->move_to(xy[0].X, xy[0].Y);
        for (int i = 1; i < count; i++)
            self->path->line_to(xy[i].X, xy[i].Y);
        delete xy;
    }

    return (PyObject*) self;
}

static PyObject*
symbol_new(PyObject* self_, PyObject* args)
{
    char* path;
    float scale = 1.0;
    if (!PyArg_ParseTuple(args, "s|f:Symbol", &path, &scale))
        return NULL;

    PathObject* self = PyObject_NEW(PathObject, &PathType);

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
            /* FIXME: cleanup */
            return NULL;
        }
        if (p == q) {
            PyErr_Format(
                PyExc_ValueError,
                "invalid arguments for command '%c'", op
                );
            /* FIXME: cleanup */
            return NULL;
        }
    }

    if (curve) {
        /* expand curves */
        agg::path_storage* path = self->path;
        agg::conv_curve<agg::path_storage> curve(*path);
        self->path = new agg::path_storage();
        self->path->add_path(curve, 0, false);
        delete path;
    }

    return (PyObject*) self;
}

void expandPaths(PathObject *self)
{
    agg::path_storage* path = self->path;
    agg::conv_curve<agg::path_storage> curve(*path);
    self->path = new agg::path_storage();
    self->path->add_path(curve, 0, false);
    delete path;
}

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
    delete xy;

    self->path->add_path(path, 0, false);

    Py_INCREF(Py_None);
    return Py_None;
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
            if (PyList_Append(list, PyFloat_FromDouble(x)) < 0)
                return NULL;
            if (PyList_Append(list, PyFloat_FromDouble(y)) < 0)
                return NULL;
        }
    }

    return list;
}

static void
path_dealloc(PathObject* self)
{
    delete self->path;
    PyObject_DEL(self);
}

static PyMethodDef path_methods[] = {

    {"lineto", (PyCFunction) path_lineto, METH_VARARGS},
    {"rlineto", (PyCFunction) path_rlineto, METH_VARARGS},
    {"curveto", (PyCFunction) path_curveto, METH_VARARGS},
    {"rcurveto", (PyCFunction) path_rcurveto, METH_VARARGS},
    {"moveto", (PyCFunction) path_moveto, METH_VARARGS},
    {"rmoveto", (PyCFunction) path_rmoveto, METH_VARARGS},

    {"close", (PyCFunction) path_close, METH_VARARGS},

    {"polygon", (PyCFunction) path_polygon, METH_VARARGS},

    {"coords", (PyCFunction) path_coords, METH_VARARGS},

    {NULL, NULL}
};

#ifdef IS_PY3K
static PyObject*  
path_getattro(PathObject* self, PyObject* nameobj)
{
    return PyObject_GenericGetAttr((PyObject*)self, nameobj);
}

#else
static PyObject*
path_getattr(PathObject* self, char* name)
{
    return Py_FindMethod(path_methods, (PyObject*) self, name);
}
#endif

/* -------------------------------------------------------------------- */

static PyMethodDef aggdraw_functions[] = {
    {"Pen", (PyCFunction) pen_new, METH_VARARGS|METH_KEYWORDS},
    {"Brush", (PyCFunction) brush_new, METH_VARARGS|METH_KEYWORDS},
    {"Font", (PyCFunction) font_new, METH_VARARGS|METH_KEYWORDS},
    {"Symbol", (PyCFunction) symbol_new, METH_VARARGS},
    {"Path", (PyCFunction) path_new, METH_VARARGS},
    {"Draw", (PyCFunction) draw_new, METH_VARARGS},
#if defined(WIN32)
    {"Dib", (PyCFunction) draw_dib, METH_VARARGS},
#endif
    {NULL, NULL}
};

#ifdef IS_PY3K
static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "aggdraw",
        "Python interface to the Anti-Grain Graphics Drawing library",
        -1,
        aggdraw_functions,
        NULL,
        NULL,
        NULL,
        NULL,
};
#endif


static PyObject *
aggdraw_init(void)    
{
#ifdef IS_PY3K
    // PyType_Ready(&DrawType);
    // PyType_Ready(&PathType);
    // PyType_Ready(&PenType);
    // PyType_Ready(&BrushType);
    // PyType_Ready(&FontType);

    DrawType.tp_methods = draw_methods;
    FontType.tp_methods = font_methods;
    PathType.tp_methods = path_methods;
    
    PyObject *module = PyModule_Create(&moduledef);
    PyObject *version = PyUnicode_FromString(QUOTE(VERSION));
    PyObject_SetAttrString(module, "VERSION", version);
    PyObject_SetAttrString(module, "__version__", version);
    Py_DECREF(version);
#else
    DrawType.ob_type = PathType.ob_type = &PyType_Type;
    PenType.ob_type = BrushType.ob_type = FontType.ob_type = &PyType_Type;

    PyObject *module = Py_InitModule3("aggdraw", aggdraw_functions,
                                      "Python interface to the Anti-Grain Graphics Drawing library");
    PyObject *version = PyBytes_FromString(QUOTE(VERSION));
    PyObject_SetAttrString(module, "VERSION", version);
    PyObject_SetAttrString(module, "__version__", version);
    Py_DECREF(version);
#endif
    if (module == NULL)
        return NULL;

    PyObject* g = PyDict_New();
    PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins());
    PyRun_String(
        "try:\n"
        "    from PIL import ImageColor\n"
        "except ImportError:\n"
        "    ImageColor = None\n"

        "def getcolor(v):\n" // FIXME: add caching (?)
        "    return ImageColor.getrgb(v)\n"

        "", Py_file_input, g, NULL

        );

    aggdraw_getcolor_obj = PyDict_GetItemString(g, "getcolor");
    return module;
}

#ifdef IS_PY3K
PyMODINIT_FUNC
PyInit_aggdraw(void)
{
    return aggdraw_init();
}

#else
PyMODINIT_FUNC
initaggdraw(void)
{
    aggdraw_init();
}
#endif

