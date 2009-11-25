/* $Id$ */

/* vi:set sw=4: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2008  Greg Couch
 * See the LICENSE file for copyright details.
 */

/*
 * Togl Bitmap Font support
 *
 * If bitmap font support is requested, then this file is included into
 * togl.c.  Parts of this file are based on <http://wiki.tcl.tk/13881>,
 * "Creating and Using Tcl Handles in C Extensions".
 *
 * Neither the Tk public nor the internal interface give enough information
 * to reuse the font in OpenGL, so we copy the private structures here to
 * access what we need.
 *
 * Globals needed by the font module are in togl.c
 */

#include <tkFont.h>

#ifdef _WIN32
# define snprintf _snprintf
#endif

struct Togl_BitmapFontInfo {
    GLuint base;
    GLuint first;
    GLuint last;
    int contextTag;
    /* TODO: keep original font and/or encoding */
};
typedef struct Togl_BitmapFontInfo Togl_BitmapFontInfo;

#define BITMAP_FONT_INFO(obj) \
		((Togl_BitmapFontInfo *) (obj)->internalRep.otherValuePtr)
#define SET_BITMAP_FONT_INFO(obj) \
		(obj)->internalRep.otherValuePtr

static void Togl_FontFree(Tcl_Obj *obj);
static void Togl_FontDup(Tcl_Obj *src, Tcl_Obj *dup);
static void Togl_FontString(Tcl_Obj *obj);
static int Togl_FontSet(Tcl_Interp *interp, Tcl_Obj *obj);

static Tcl_ObjType Togl_BitmapFontType = {
	"Togl BitmapFont",	/* name */
	Togl_FontFree,		/* free internal rep */
	Togl_FontDup,		/* dup internal rep */
	Togl_FontString,	/* update string from internal rep */
	Togl_FontSet		/* set internal rep from string */
};

static int
Togl_FontSet(Tcl_Interp *interp, Tcl_Obj *obj)
{
	if (interp)
		Tcl_AppendResult(interp, "cannot (re)build object of type \"",
					Togl_BitmapFontType.name, "\"", NULL);
	return TCL_ERROR;
}

static void
Togl_FontFree(Tcl_Obj *obj)
{
    Togl_BitmapFontInfo *bfi = BITMAP_FONT_INFO(obj);
    ckfree((char *) bfi);
}

static void
Togl_FontString(Tcl_Obj *obj)
{
	/* assert(obj->bytes == NULL) */
	static char buf[256];
	register unsigned len;
	Togl_BitmapFontInfo *bfi = BITMAP_FONT_INFO(obj);
#ifndef TOGL_AGL
	snprintf(buf, sizeof buf, "{{%s} %d %d %d}", Togl_BitmapFontType.name,
			bfi->base, bfi->first, bfi->last);
#else
	/* unlike every other platform, on Aqua, GLint is long */
	snprintf(buf, sizeof buf, "{{%s} %ld %ld %ld}",
		Togl_BitmapFontType.name, bfi->base, bfi->first, bfi->last);
#endif
	len = strlen(buf);
	obj->bytes = (char *) ckalloc(len + 1);
	strcpy(obj->bytes, buf);
	obj->length = len;
}

static void Togl_FontDup(Tcl_Obj *src, Tcl_Obj *dup)
{
    /*
     * When duplicated, lose the font-ness and just be a string.
     * So don't copy the internal representation and don't set
     * dup->typePtr.
     */
}

#if defined(TOGL_X11)
/* From tkUnixFont.c */
/* 
 * The following structure encapsulates an individual screen font.  A font
 * object is made up of however many SubFonts are necessary to display a
 * stream of multilingual characters.
 */

typedef struct FontFamily FontFamily;

typedef struct SubFont
{
    char  **fontMap;            /* Pointer to font map from the FontFamily,
                                 * cached here to save a dereference. */
    XFontStruct *fontStructPtr; /* The specific screen font that will be used
                                 * when displaying/measuring chars belonging to 
                                 * the FontFamily. */
    FontFamily *familyPtr;      /* The FontFamily for this SubFont. */
} SubFont;

/* 
 * The following structure represents Unix's implementation of a font
 * object.
 */

#  define SUBFONT_SPACE		3
#  define BASE_CHARS		256

typedef struct UnixFont
{
    TkFont  font;               /* Stuff used by generic font package.  Must be 
                                 * first in structure. */
    SubFont staticSubFonts[SUBFONT_SPACE];
    /* Builtin space for a limited number of SubFonts. */
    int     numSubFonts;        /* Length of following array. */
    SubFont *subFontArray;      /* Array of SubFonts that have been loaded in
                                 * order to draw/measure all the characters
                                 * encountered by this font so far.  All fonts
                                 * start off with one SubFont initialized by
                                 * AllocFont() from the original set of font
                                 * attributes.  Usually points to
                                 * staticSubFonts, but may point to malloced
                                 * space if there are lots of SubFonts. */
    SubFont controlSubFont;     /* Font to use to display control-character
                                 * expansions. */

#  if 0
    Display *display;           /* Display that owns font. */
    int     pixelSize;          /* Original pixel size used when font was
                                 * constructed. */
    TkXLFDAttributes xa;        /* Additional attributes that specify the
                                 * preferred foundry and encoding to use when
                                 * constructing additional SubFonts. */
    int     widths[BASE_CHARS]; /* Widths of first 256 chars in the base font,
                                 * for handling common case. */
    int     underlinePos;       /* Offset from baseline to origin of underline
                                 * bar (used when drawing underlined font)
                                 * (pixels). */
    int     barHeight;          /* Height of underline or overstrike bar (used
                                 * when drawing underlined or strikeout font)
                                 * (pixels). */
#  endif
} UnixFont;

#elif defined(TOGL_WGL)
#  include <tkWinInt.h>
/* From tkWinFont.c */

typedef struct FontFamily FontFamily;

/* 
 * The following structure encapsulates an individual screen font.  A font
 * object is made up of however many SubFonts are necessary to display a
 * stream of multilingual characters.
 */

typedef struct SubFont
{
    char  **fontMap;            /* Pointer to font map from the FontFamily,
                                 * cached here to save a dereference. */
    HFONT   hFont;              /* The specific screen font that will be used
                                 * when displaying/measuring chars belonging to 
                                 * the FontFamily. */
    FontFamily *familyPtr;      /* The FontFamily for this SubFont. */
} SubFont;

/* 
 * The following structure represents Windows' implementation of a font
 * object.
 */

#  define SUBFONT_SPACE		3
#  define BASE_CHARS		128

typedef struct WinFont
{
    TkFont  font;               /* Stuff used by generic font package.  Must be 
                                 * first in structure. */
    SubFont staticSubFonts[SUBFONT_SPACE];
    /* Builtin space for a limited number of SubFonts. */
    int     numSubFonts;        /* Length of following array. */
    SubFont *subFontArray;      /* Array of SubFonts that have been loaded in
                                 * order to draw/measure all the characters
                                 * encountered by this font so far.  All fonts
                                 * start off with one SubFont initialized by
                                 * AllocFont() from the original set of font
                                 * attributes.  Usually points to
                                 * staticSubFonts, but may point to malloced
                                 * space if there are lots of SubFonts. */

    HWND    hwnd;               /* Toplevel window of application that owns
                                 * this font, used for getting HDC for
                                 * offscreen measurements. */
    int     pixelSize;          /* Original pixel size used when font was
                                 * constructed. */
    int     widths[BASE_CHARS]; /* Widths of first 128 chars in the base font,
                                 * for handling common case.  The base font is
                                 * always used to draw characters between
                                 * 0x0000 and 0x007f. */
} WinFont;

#elif defined(TOGL_AGL)

typedef struct FontFamily
{
    struct FontFamily *nextPtr; /* Next in list of all known font families. */
    int     refCount;           /* How many SubFonts are referring to this
                                 * FontFamily.  When the refCount drops to
                                 * zero, this FontFamily may be freed. */
    /* 
     * Key.
     */

    FMFontFamily faceNum;       /* Unique face number key for this FontFamily. */

    /* 
     * Derived properties.
     */

    Tcl_Encoding encoding;      /* Encoding for this font family. */
#  if 0
    int     isSymbolFont;       /* Non-zero if this is a symbol family. */
    int     isMultiByteFont;    /* Non-zero if this is a multi-byte family. */
    char    typeTable[256];     /* Table that identfies all lead bytes for a
                                 * multi-byte family, used when measuring
                                 * chars. If a byte is a lead byte, the value
                                 * at the corresponding position in the
                                 * typeTable is 1, otherwise 0.  If this is a
                                 * single-byte font, all entries are 0. */
    char   *fontMap[FONTMAP_PAGES];
    /* Two-level sparse table used to determine quickly if the specified
     * character exists. As characters are encountered, more pages in this
     * table are dynamically added.  The contents of each page is a bitmask
     * consisting of FONTMAP_BITSPERPAGE bits, representing whether this font
     * can be used to display the given character at the corresponding bit
     * position.  The high bits of the character are used to pick which page of 
     * the table is used. */
#  endif
} FontFamily;

/* 
 * The following structure encapsulates an individual screen font.  A font
 * object is made up of however many SubFonts are necessary to display a
 * stream of multilingual characters.
 */

typedef struct SubFont
{
    char  **fontMap;            /* Pointer to font map from the FontFamily,
                                 * cached here to save a dereference. */
    FontFamily *familyPtr;      /* The FontFamily for this SubFont. */
} SubFont;

/* 
 * The following structure represents Macintosh's implementation of a font
 * object.
 */

#  define SUBFONT_SPACE                3

typedef struct MacFont
{
    TkFont  font;               /* Stuff used by generic font package.  Must be 
                                 * first in structure. */
    SubFont staticSubFonts[SUBFONT_SPACE];
    /* Builtin space for a limited number of SubFonts. */
    int     numSubFonts;        /* Length of following array. */
    SubFont *subFontArray;      /* Array of SubFonts that have been loaded in
                                 * order to draw/measure all the characters
                                 * encountered by this font so far.  All fonts
                                 * start off with one SubFont initialized by
                                 * AllocFont() from the original set of font
                                 * attributes.  Usually points to
                                 * staticSubFonts, but may point to malloced
                                 * space if there are lots of SubFonts. */

    short   size;               /* Font size in pixels, constructed from font
                                 * attributes. */
    short   style;              /* Style bits, constructed from font
                                 * attributes. */
} MacFont;
#endif

/* 
 * Load the named bitmap font as a sequence of bitmaps in a display list.
 * fontname may be any font recognized by Tk_GetFont.
 */
Tcl_Obj *
Togl_LoadBitmapFont(const Togl *togl, const char *fontname)
{
    Tk_Font font;
    Togl_BitmapFontInfo *bfi;
    Tcl_Obj *obj;

#if defined(TOGL_X11)
    UnixFont *unixfont;
    XFontStruct *fontinfo;
#elif defined(TOGL_WGL)
    WinFont *winfont;
    HFONT   oldFont;
    TEXTMETRIC tm;
#elif defined(TOGL_AGL)
    MacFont *macfont;
#endif
    int     first, last, count;
    GLuint  fontbase;

    if (!fontname) {
        fontname = DEFAULT_FONTNAME;
    }

    font = Tk_GetFont(togl->Interp, togl->TkWin, fontname);
    if (!font) {
        return NULL;
    }
#if defined(TOGL_X11)
    unixfont = (UnixFont *) font;
    fontinfo = unixfont->subFontArray->fontStructPtr;
    first = fontinfo->min_char_or_byte2;
    last = fontinfo->max_char_or_byte2;
#elif defined(TOGL_WGL)
    winfont = (WinFont *) font;
    oldFont =
            (HFONT) SelectObject(togl->tglGLHdc, winfont->subFontArray->hFont);
    GetTextMetrics(togl->tglGLHdc, &tm);
    first = tm.tmFirstChar;
    last = tm.tmLastChar;
#elif defined(TOGL_AGL)
    macfont = (MacFont *) font;
    first = 10;                 /* don't know how to determine font range on
                                 * Mac... */
    last = 255;
#endif

    if (last > 255)
	last = 255;		/* no unicode support */

    count = last - first + 1;
    fontbase = glGenLists((GLuint) (last + 1));
    if (fontbase == 0) {
#ifdef TOGL_WGL
        SelectObject(togl->tglGLHdc, oldFont);
#endif
        Tk_FreeFont(font);
        return NULL;
    }
#if defined(TOGL_WGL)
    wglUseFontBitmaps(togl->tglGLHdc, first, count, fontbase + first);
    SelectObject(togl->tglGLHdc, oldFont);
#elif defined(TOGL_X11)
    glXUseXFont(fontinfo->fid, first, count, (int) fontbase + first);
#elif defined(TOGL_AGL)
    aglUseFont(togl->aglCtx,
            macfont->subFontArray->familyPtr->faceNum,
            macfont->style, macfont->size, first, count, fontbase + first);
#endif
    Tk_FreeFont(font);

    bfi = (Togl_BitmapFontInfo *) ckalloc(sizeof (Togl_BitmapFontInfo));
    bfi->base = fontbase;
    bfi->first = first;
    bfi->last = last;
    bfi->contextTag = togl->contextTag;

    obj = Tcl_NewObj();
    SET_BITMAP_FONT_INFO(obj) = bfi;
    obj->typePtr = &Togl_BitmapFontType;

    return obj;
}

/* 
 * Release the display lists which were generated by Togl_LoadBitmapFont().
 */
int
Togl_UnloadBitmapFont(const Togl *togl, Tcl_Obj *toglfont)
{
    Togl_BitmapFontInfo *bfi;
    if (toglfont == NULL || toglfont->typePtr != &Togl_BitmapFontType) {
	Tcl_Interp *interp = Togl_Interp(togl);
	Tcl_AppendResult(interp, "font not found", NULL);
	return TCL_ERROR;
    }
    bfi = BITMAP_FONT_INFO(toglfont);
    glDeleteLists(bfi->base, bfi->last + 1);	/* match glGenLists */
    return TCL_OK;
}

int
Togl_WriteObj(const Togl *togl, const Tcl_Obj *toglfont, Tcl_Obj *obj)
{
    const char *str;
    int len;

    str = Tcl_GetStringFromObj(obj, &len);
    return Togl_WriteChars(togl, toglfont, str, len);
}

int
Togl_WriteChars(const Togl *togl, const Tcl_Obj *toglfont, const char *str, int len)
{
    /* TODO: assume utf8 encoding and convert to font encoding */
    Togl_BitmapFontInfo *bfi;
    if (toglfont == NULL || toglfont->typePtr != &Togl_BitmapFontType)
	return -1;
    bfi = BITMAP_FONT_INFO(toglfont);
    if (Togl_ContextTag(togl) != bfi->contextTag)
	return -1;
    if (len == 0)
	len = strlen(str);
    glListBase(bfi->base);
    glCallLists(len, GL_UNSIGNED_BYTE, str);
    return len;
}
