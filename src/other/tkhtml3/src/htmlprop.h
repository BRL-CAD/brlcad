/*
 * htmlprop.h --
 *
 *     This header file contains the definition of the HtmlComputedValues
 *     structure, which stores a set of CSS2 properties output by the
 *     styler. This information is used by the layout engine in
 *     htmllayout.c to create the runtime model of the document. 
 *
 * -----------------------------------------------------------------------
 *     TODO: Copyright.
 */

#ifndef __HTMLPROP_H__
#define __HTMLPROP_H__

/*
 * We need <limits.h> to get the INT_MIN symbol (the most negative number that
 * can be stored in a C "int".
 */
#include <limits.h>

typedef struct HtmlFourSides HtmlFourSides;
typedef struct HtmlComputedValues HtmlComputedValues;
typedef struct HtmlComputedValuesCreator HtmlComputedValuesCreator;
typedef struct HtmlColor HtmlColor;
typedef struct HtmlCounterList HtmlCounterList;

typedef struct HtmlFont HtmlFont;
typedef struct HtmlFontKey HtmlFontKey;
typedef struct HtmlFontCache HtmlFontCache;

/* 
 * This structure is used to group four padding, margin or border-width
 * values together. When we get around to it, it will be used for the
 * position properties too ('top', 'right', 'bottom' and 'left').
 */
struct HtmlFourSides {
    int iTop;
    int iLeft;
    int iBottom;
    int iRight;
};

/*
 * The HtmlFont structure is used to store a font in use by the current
 * document. The following properties are used to determine the Tk
 * font to load:
 * 
 *     'font-size'
 *     'font-family'
 *     'font-style'
 *     'font-weight'
 *
 * HtmlFont structures are stored in the HtmlTree.aFonts hash table. The hash
 * table uses a custom key type (struct HtmlFontKey) implemented in htmlhash.c. 
 */
#define HTML_IFONTSIZE_SCALE 1000
struct HtmlFontKey {
    /* If iFontSize is positive, then it is in thousandths of points. 
     * If negative, in thousandths of pixels. */
    int iFontSize;           /* Font size in thousandths of points */

    const char *zFontFamily; /* Name of font family (i.e. "Serif") */
    unsigned char isItalic;  /* True if the font is italic */
    unsigned char isBold;    /* True if the font is bold */
};
struct HtmlFont {
    int nRef;              /* Number of pointers to this structure */
    HtmlFontKey *pKey;     /* Pointer to corresponding HtmlFontKey structure */
    char *zFont;           /* Name of font */
    Tk_Font tkfont;        /* The Tk font */

    int em_pixels;         /* Pixels per 'em' unit */
    int ex_pixels;         /* Pixels per 'ex' unit */
    int space_pixels;      /* Pixels per space (' ') in this font */
    Tk_FontMetrics metrics;

    HtmlFont *pNext;       /* Next entry in the Html.FontCache LRU list */
};

/*
 * In Tk, allocating new fonts is very expensive. So we try hard to 
 * avoid doing it more than is required.
 */
#define HTML_MAX_ZEROREF_FONTS 50
struct HtmlFontCache {
    Tcl_HashTable aHash;
    HtmlFont *pLruHead;
    HtmlFont *pLruTail;
    int nZeroRef;
};

/*
 * An HtmlColor structure is used to store each color in use by the current
 * document. HtmlColor structures are stored in the HtmlTree.aColors hash
 * table. The hash table uses string keys (the name of the color).
 */
struct HtmlColor {
    int nRef;              /* Number of pointers to this structure */
    char *zColor;          /* Name of color */
    XColor *xcolor;        /* The XColor* */
};

/*
 * An HtmlCounterList is used to store the computed value of the 
 * 'counter-increment' and 'counter-reset' properties.
 */
struct HtmlCounterList {
  int nRef;

  int nCounter;
  char **azCounter;
  int *anValue;
};

/*
 * An instance of this structure stores a set of property values as assigned by
 * the styler process. The values are as far as I can tell "computed" values,
 * but in some cases I'm really only guessing.
 *
 * All values are stored as one of the following broad "types":
 *
 *     Variable Name       Type
 *     ------------------------------------------
 *         eXXX            Enumerated type values
 *         iXXX            Pixel type values
 *         cXXX            Color type values
 *         fXXX            Font type values
 *     ------------------------------------------
 *
 * Enumerated type values
 *
 *     Many properties can be stored as a single variable, for example the
 *     'display' property is stored in the HtmlComputedValues.eDisplay
 *     variable.  Members of the HtmlComputedValues structure with names that
 *     match the pattern "eXXX" contain a CSS constant value (one of the
 *     CSS_CONST_XXX #define symbols). These are defined in the header file
 *     cssprop.h, which is generated during compilation by the script in
 *     cssprop.tcl. 
 *
 *     Note: Since we use 'unsigned char' to store the eXXX variables:
 *
 *         assert(CSS_CONST_MIN_CONSTANT >= 0);
 *         assert(CSS_CONST_MAX_CONSTANT < 256);
 *
 * Color type values
 *
 * Font type values
 *
 * Pixel type values:
 *
 *     Most variables that match the pattern 'iXXX' contain pixel values - a
 *     length or size expressed in pixels. The only exceptions at the moment 
 *     are HtmlFontKey.iFontSize and iZIndex. 
 *
 *     Percentage values
 *
 *         Some values, for example the 'width' property, may be either
 *         calculated to an exact number of pixels by the styler or left as a
 *         percentage value. In the first case, the 'int iXXX;' variable for
 *         the property contains the number of pixels. Otherwise, it contains
 *         the percentage value multiplied by 100. If the value is a
 *         percentage, then the PROP_MASK_XXX bit is set in the
 *         HtmlComputedValues.mask mask.  For example, given the width of the
 *         parent block in pixels, the following code determines the width in
 *         pixels contained by the HtmlComputedValues structure:
 *
 *             int iParentPixelWidth = <some assignment>;
 *             HtmlComputedValues Values = <some assignment>;
 *
 *             int iPixelWidth;
 *             if (Values.mask & PROP_MASK_WIDTH) {
 *                 iPixelWidth = (Values.iWidth * iParentPixelWidth / 10000);
 *             } else {
 *                 iPixelWidth = Values.iWidth;
 *             }
 *
 *     The 'auto', 'none' and 'normal' values:
 *
 *         If a pixel type value is set to 'auto', 'none' or 'normal', the
 *         integer variable is set to the constant PIXELVAL_AUTO, 
 *         PIXELVAL_NONE or PIXELVAL_NORMAL respectively. These are both very
 *         large negative numbers, unlikely to be confused with real pixel
 *         values.
 *
 *     iVerticalAlign:
 *
 *         The 'vertical-align' property, stored in iVerticalAlign is different
 *         from the other iXXX values. The styler output for vertical align is
 *         either a number of pixels or one of the constants 'baseline', 'sub'
 *         'super', 'top', 'text-top', 'middle', 'bottom', 'text-bottom'. The
 *         'vertical-align' property can be assigned a percentage value, but
 *         the styler can resolve it. (This matches the CSS 2.1 description of
 *         the computed value - section 10.8.1).
 *
 *         If 'vertical-align' is a constant value, it is stored in
 *         eVerticalAlign (as a CSS_CONST_XXX value). Otherwise, if it is a
 *         pixel value it is stored in iVerticalAlign and eVerticalAlign is set
 *         to 0.
 *
 *     iLineHeight:
 *         Todo: Note that inheritance is not done correctly for this property
 *         if it is set to <number>.
 *
 *
 * Properties not represented:
 *
 *     The following properties should be supported by this structure, as
 *     Tkhtml aims to one day support them. They are not currently supported
 *     because (a) layout engine support is a long way off, and (b) it would 
 *     be tricky in some way to do so:
 *
 *         'clip' 'cursor' 'counter-increment' 
 *         'counter-reset' 'quotes'
 */
struct HtmlComputedValues {
    HtmlImage2 *imZoomedBackgroundImage;   /* MUST BE FIRST (see htmlhash.c) */
    int nRef;                              /* MUST BE FIRST (see htmlhash.c) */

    unsigned int mask;

    unsigned char eDisplay;           /* 'display' */
    unsigned char eFloat;             /* 'float' */
    unsigned char eClear;             /* 'clear' */

    /* ePosition stores the enumerated 'position' property. The position
     * structure stores the computed values of the 'top', 'bottom', 'left' 
     * and 'right' properties. */
    unsigned char ePosition;          /* 'position' */
    HtmlFourSides position;           /* (pixels, %, AUTO) */

    HtmlColor *cBackgroundColor;      /* 'background-color' */

    unsigned char eTextDecoration;    /* 'text-decoration' */

    /* See above. iVerticalAlign is used only if (eVerticalAlign==0) */
    unsigned char eVerticalAlign;     /* 'vertical-align' */
    int iVerticalAlign;               /* 'vertical-align' (pixels) */

    int iWidth;                       /* 'width'          (pixels, %, AUTO)   */
    int iMinWidth;                    /* 'min-width'      (pixels, %)         */
    int iMaxWidth;                    /* 'max-height'     (pixels, %, NONE)   */
    int iHeight;                      /* 'height'         (pixels, % AUTO)    */
    int iMinHeight;                   /* 'min-height'     (pixels, %)         */
    int iMaxHeight;                   /* 'max-height'     (pixels, %, NONE)   */
    HtmlFourSides padding;            /* 'padding'        (pixels, %)         */
    HtmlFourSides margin;             /* 'margin'         (pixels, %, AUTO)   */

    HtmlFourSides border;             /* 'border-width'   (pixels)            */
    unsigned char eBorderTopStyle;    /* 'border-top-style' */
    unsigned char eBorderRightStyle;  /* 'border-right-style' */
    unsigned char eBorderBottomStyle; /* 'border-bottom-style' */
    unsigned char eBorderLeftStyle;   /* 'border-left-style' */
    HtmlColor *cBorderTopColor;       /* 'border-top-color' */
    HtmlColor *cBorderRightColor;     /* 'border-right-color' */
    HtmlColor *cBorderBottomColor;    /* 'border-bottom-color' */
    HtmlColor *cBorderLeftColor;      /* 'border-left-color' */

    unsigned char eOutlineStyle;      /* 'outline-style' */
    int iOutlineWidth;                /* 'outline-width' (pixels) */
    HtmlColor *cOutlineColor;         /* 'outline-color' */

    HtmlImage2 *imBackgroundImage;        /* 'background-image' */
    unsigned char eBackgroundRepeat;      /* 'background-repeat' */
    unsigned char eBackgroundAttachment;  /* 'background-attachment' */
    int iBackgroundPositionX;
    int iBackgroundPositionY;

    unsigned char eOverflow;          /* 'overflow' */

    int iZIndex;                      /* 'z-index'        (integer, AUTO) */

    /* The Tkhtml specific properties */
    HtmlImage2 *imReplacementImage;   /* '-tkhtml-replacement-image' */

    int iOrderedListStart;            /* '-tkhtml-ordered-list-start' */
    int iOrderedListValue;            /* '-tkhtml-ordered-list-value' */

    /* Properties not yet in use - TODO! */
    unsigned char eUnicodeBidi;       /* 'unicode-bidi' */
    unsigned char eTableLayout;       /* 'table-layout' */

    HtmlCounterList *clCounterReset;
    HtmlCounterList *clCounterIncrement;

    /* 'font-size', 'font-family', 'font-style', 'font-weight' */
    HtmlFont *fFont;

    /* INHERITED PROPERTIES START HERE */
    unsigned char eListStyleType;     /* 'list-style-type' */
    unsigned char eListStylePosition; /* 'list-style-position' */
    unsigned char eWhitespace;        /* 'white-space' */
    unsigned char eTextAlign;         /* 'text-align' */
    unsigned char eVisibility;        /* 'visibility' */
    HtmlColor *cColor;                /* 'color' */
    HtmlImage2 *imListStyleImage;     /* 'list-style-image' */
    int iTextIndent;                  /* 'text-indext' (pixels, %) */
    int iBorderSpacing;               /* 'border-spacing' (pixels)            */
    int iLineHeight;                  /* 'line-height'    (pixels, %, NORMAL) */
    unsigned char eFontVariant;       /* 'font-variant' */

    unsigned char eCursor;            /* 'cursor' */

    /* Properties not yet in use - TODO! */
    int iWordSpacing;                 /* 'word-spacing'   (pixels, NORMAL) */
    int iLetterSpacing;               /* 'letter-spacing' (pixels, NORMAL) */
    unsigned char eTextTransform;     /* 'text-transform' */
    unsigned char eDirection;         /* 'direction' */
    unsigned char eBorderCollapse;    /* 'border-collapse' */
    unsigned char eCaptionSide;       /* 'caption-side' */
    unsigned char eEmptyCells;        /* 'empty-cells' */
};

/*
 * If pzContent is not NULL, then the pointer it points to may be set
 * to point at allocated memory in which to store the computed value
 * of the 'content' property.
 */
struct HtmlComputedValuesCreator {
    HtmlComputedValues values;
    HtmlFontKey fontKey;

    HtmlTree *pTree;
    HtmlNode *pNode;                 /* Node to associate LOG with */
    HtmlNode *pParent;               /* Node to inherit from */
    unsigned int em_mask;
    unsigned int ex_mask;
    int eVerticalAlignPercent;       /* True if 'vertical-align' is a % */
    CssProperty *pDeleteList;

    CssProperty *pContent;
    char **pzContent;
};

/*
 * Percentage masks.
 * 
 * According to the spec, the following CSS2 properties can also be set to
 * percentages:
 *
 *     Unsupported properties:
 *         'background-position'
 *         'bottom', 'top', 'left', 'right'
 *         'text-indent'
 *
 *     These can be set to percentages, but the styler can resolve them:
 *         'font-size', 'line-height', 'vertical-align'
 *
 * The HtmlComputedValues.mask mask also contains the
 * CONSTANT_MASK_VERTICALALIGN bit. If this bit is set, then
 * HtmlComputedValues.iVerticalAlign should be interpreted as a constant value
 * (like an HtmlComputedValues.eXXX variable).
 */
#define PROP_MASK_WIDTH                   0x00000001
#define PROP_MASK_MIN_WIDTH               0x00000002
#define PROP_MASK_MAX_WIDTH               0x00000004
#define PROP_MASK_HEIGHT                  0x00000008
#define PROP_MASK_MIN_HEIGHT              0x00000010
#define PROP_MASK_MAX_HEIGHT              0x00000020
#define PROP_MASK_MARGIN_TOP              0x00000040
#define PROP_MASK_MARGIN_RIGHT            0x00000080
#define PROP_MASK_MARGIN_BOTTOM           0x00000100
#define PROP_MASK_MARGIN_LEFT             0x00000200
#define PROP_MASK_PADDING_TOP             0x00000400
#define PROP_MASK_PADDING_RIGHT           0x00000800
#define PROP_MASK_PADDING_BOTTOM          0x00001000
#define PROP_MASK_PADDING_LEFT            0x00002000
#define PROP_MASK_VERTICAL_ALIGN          0x00004000
#define PROP_MASK_BORDER_TOP_WIDTH        0x00008000
#define PROP_MASK_BORDER_RIGHT_WIDTH      0x00010000
#define PROP_MASK_BORDER_BOTTOM_WIDTH     0x00020000
#define PROP_MASK_BORDER_LEFT_WIDTH       0x00040000
#define PROP_MASK_LINE_HEIGHT             0x00080000
#define PROP_MASK_BACKGROUND_POSITION_X   0x00100000
#define PROP_MASK_BACKGROUND_POSITION_Y   0x00200000
#define PROP_MASK_BORDER_SPACING          0x00400000
#define PROP_MASK_OUTLINE_WIDTH           0x00800000
#define PROP_MASK_TOP                     0x01000000
#define PROP_MASK_BOTTOM                  0x02000000
#define PROP_MASK_RIGHT                   0x04000000
#define PROP_MASK_LEFT                    0x08000000
#define PROP_MASK_TEXT_INDENT             0x10000000
#define PROP_MASK_WORD_SPACING            0x20000000
#define PROP_MASK_LETTER_SPACING          0x40000000

/*
 * Pixel values in the HtmlComputedValues struct may also take the following
 * special values. These are all very large negative numbers, unlikely to be
 * confused with real pixel counts. INT_MIN comes from <limits.h>, which is
 * supplied by Tcl if the operating system doesn't have it.
 */
#define PIXELVAL_AUTO       (2 + (int)INT_MIN)
#define PIXELVAL_NONE       (3 + (int)INT_MIN)
#define PIXELVAL_NORMAL     (4 + (int)INT_MIN)
#define MAX_PIXELVAL        (5 + (int)INT_MIN)

/* 
 * API Notes for managing HtmlComputedValues structures:
 *
 *     The following three functions are used by the styler phase to create and
 *     populate an HtmlComputedValues structure (a set of property values for a
 *     node):
 *
 *         HtmlComputedValuesInit()           (exactly one call)
 *         HtmlComputedValuesSet()            (zero or more calls)
 *         HtmlComputedValuesFinish()         (exactly one call)
 *
 *     To use this API, the caller allocates (either on the heap or the stack,
 *     doesn't matter) an HtmlComputedValuesCreator struct. The contents are
 *     initialised by HtmlComputedValuesInit().
 *
 *         HtmlComputedValuesCreator sValues;
 *         HtmlComputedValuesInit(pTree, pNode, &sValues);
 *
 *     This initialises the HtmlComputedValuesCreator structure to contain the
 *     default (called "initial" in the CSS spec) value for each property. The
 *     default property values can be overwritten using the
 *     HtmlComputedValuesSet() function (see comments above implementation
 *     below).
 * 
 *     Finally, HtmlComputedValuesFinish() is called to obtain the populated 
 *     HtmlComputedValues structure. This function returns a pointer to an
 *     HtmlComputedValues structure, which should be associated with the node
 *     in question before it is passed to the layout engine:
 *
 *         p = HtmlComputedValuesFinish(&sValues);
 *         assert(p);
 *         pNode->pPropertyValues = p;
 *
 *     Once an HtmlComputedValues pointer returned by Finish() is no longer
 *     required (when the node is being restyled or deleted), it should be
 *     freed using:
 *
 *         HtmlComputedValuesRelease(pNode->pPropertyValues);
 */
void HtmlComputedValuesInit(
HtmlTree*, HtmlNode*, HtmlNode*, HtmlComputedValuesCreator*);
int HtmlComputedValuesSet(HtmlComputedValuesCreator *, int, CssProperty*);
HtmlComputedValues *HtmlComputedValuesFinish(HtmlComputedValuesCreator *);

void HtmlComputedValuesFreeProperty(HtmlComputedValuesCreator*, CssProperty *);

void HtmlComputedValuesRelease(HtmlTree *, HtmlComputedValues*);
void HtmlComputedValuesReference(HtmlComputedValues *);

/*
 * The following two functions are used to initialise and destroy the following
 * tables used by code in htmlprop.c. They are called as part of the
 * initialisation and destruction of the widget.
 *
 *     HtmlTree.aColor
 *     HtmlTree.aFont
 *     HtmlTree.aValues
 *     HtmlTree.aFontSizeTable
 */
void HtmlComputedValuesSetupTables(HtmlTree *);
void HtmlComputedValuesCleanupTables(HtmlTree *);

void HtmlComputedValuesFreePrototype(HtmlTree *);

/*
 * Empty the font cache (i.e. because font config options have changed).
 */
void HtmlFontCacheClear(HtmlTree *, int);

/* 
 * This function formats the HtmlComputedValues structure as a Tcl list and
 * sets the result of the interpreter to that list. Used to allow inspection of
 * a nodes computed values from a Tcl script.
 */
int HtmlNodeProperties(Tcl_Interp *, HtmlComputedValues *);
int HtmlNodeGetProperty(Tcl_Interp *, Tcl_Obj *, HtmlComputedValues *);

/*
 * Determine if changing the computed properties of a node from one
 * argument structure to the other requires a re-layout. Return 1 if it
 * does, or 0 otherwise.
 */
int HtmlComputedValuesCompare(HtmlComputedValues *, HtmlComputedValues *);


#define HTML_COMPUTED_MARGIN_TOP      margin.iTop
#define HTML_COMPUTED_MARGIN_RIGHT    margin.iRight
#define HTML_COMPUTED_MARGIN_BOTTOM   margin.iBottom
#define HTML_COMPUTED_MARGIN_LEFT     margin.iLeft

#define HTML_COMPUTED_PADDING_TOP     padding.iTop
#define HTML_COMPUTED_PADDING_RIGHT   padding.iRight
#define HTML_COMPUTED_PADDING_BOTTOM  padding.iBottom
#define HTML_COMPUTED_PADDING_LEFT    padding.iLeft

#define HTML_COMPUTED_PADDING_TOP     padding.iTop
#define HTML_COMPUTED_PADDING_RIGHT   padding.iRight
#define HTML_COMPUTED_PADDING_BOTTOM  padding.iBottom
#define HTML_COMPUTED_PADDING_LEFT    padding.iLeft

#define HTML_COMPUTED_TOP             position.iTop
#define HTML_COMPUTED_RIGHT           position.iRight
#define HTML_COMPUTED_BOTTOM          position.iBottom
#define HTML_COMPUTED_LEFT            position.iLeft

#define HTML_COMPUTED_HEIGHT          iHeight
#define HTML_COMPUTED_WIDTH           iWidth
#define HTML_COMPUTED_MIN_HEIGHT      iMinHeight
#define HTML_COMPUTED_MIN_WIDTH       iMinWidth
#define HTML_COMPUTED_MAX_HEIGHT      iMaxHeight
#define HTML_COMPUTED_MAX_WIDTH       iMaxWidth
#define HTML_COMPUTED_TEXT_INDENT     iTextIndent

/* The PIXELVAL macro takes three arguments:
 * 
 *    pV         - Pointer to HtmlComputedValues structure.
 *
 *    prop       - Property identifier (i.e. MARGIN_LEFT). The
 *                 HTML_COMPUTED_XXX macros define the set of acceptable 
 *                 identifiers.
 *
 *    percent_of - The pixel value used to calculate percentage values against.
 *
 * Notes:
 *
 *    * If percent_of is less than 0 (i.e. PIXELVAL_AUTO) and the property
 *      specified by prop computed to a percentage, a copy of percent_of is
 *      returned.
 *
 *    * If pV==NULL, 0 is returned.
 */
#define PIXELVAL(pV, prop, percent_of) ( \
    (!pV ? 0 :                            \
    ((pV)->mask & PROP_MASK_ ## prop) ? ( \
        ((percent_of) <= 0) ? (percent_of) : \
        (((pV)-> HTML_COMPUTED_ ## prop * (percent_of)) / 10000) \
    ) : ((pV)-> HTML_COMPUTED_ ## prop) \
))

#endif

