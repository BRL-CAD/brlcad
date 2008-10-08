#include "cssprop.h"
#include "html.h"
#include <string.h>        /* strlen() */
#include <ctype.h>         /* tolower() */
#include <assert.h>        /* assert() */


typedef struct HashEntry HashEntry;
struct HashEntry {
    const char *zString;       /* String for this entry */
    int iNext;                 /* Next entry in hash-chain, or -1 */
};

/*
 * Return the hash of zString. The hash is guaranteed to be between 0
 * and 127, inclusive.
 */
static int 
Hash(nString, zString) 
    int nString;
    const char *zString;
{
    unsigned int result = 0;
    const char *string = zString;
    const char *end = &zString[nString];
    int c;
    for (c=*string; string != end; c=*++string) {
        result += (result<<3) + tolower(c);
    }
    if (result & 0x00000080) { 
        result = ~result;
    }
    return (result & 0x000000FF);
}

static int
Lookup(nString, zString, aTable, aHashTable) 
    int nString;
    const char *zString;
    int *aTable;
    HashEntry *aHashTable;
{
    int t;

    if (nString < 0) {
        nString = strlen(zString);
    }

    for (
         t = aTable[Hash(nString, zString)]; 
         t >= 0 && (
             strlen(aHashTable[t].zString) != nString || 
             strnicmp(zString, aHashTable[t].zString, nString)
         );
         t = aHashTable[t].iNext
    );

    return t;
}
    
static const HashEntry aHtmlCssConstant[] = {
    {"-tkhtml-center", -101},
    {"-tkhtml-inline-button", -101},
    {"-tkhtml-left", -101},
    {"-tkhtml-no-color", -101},
    {"-tkhtml-right", -101},
    {"absolute", CSS_CONST__TKHTML_NO_COLOR - 100},
    {"aqua", -101},
    {"armenian", -101},
    {"auto", -101},
    {"baseline", CSS_CONST__TKHTML_RIGHT - 100},
    {"bidi-override", -101},
    {"black", -101},
    {"block", -101},
    {"blue", CSS_CONST__TKHTML_CENTER - 100},
    {"bold", -101},
    {"bolder", -101},
    {"both", -101},
    {"bottom", -101},
    {"capitalize", -101},
    {"center", -101},
    {"circle", -101},
    {"collapse", -101},
    {"compact", -101},
    {"crosshair", -101},
    {"dashed", -101},
    {"decimal", -101},
    {"decimal-leading-zero", -101},
    {"default", CSS_CONST_CIRCLE - 100},
    {"disc", -101},
    {"dotted", CSS_CONST_BLOCK - 100},
    {"double", CSS_CONST_DOTTED - 100},
    {"e-resize", -101},
    {"embed", CSS_CONST_BOTTOM - 100},
    {"fixed", -101},
    {"fuchsia", -101},
    {"georgian", -101},
    {"gray", -101},
    {"green", CSS_CONST_DASHED - 100},
    {"groove", -101},
    {"help", CSS_CONST_GREEN - 100},
    {"hidden", -101},
    {"hide", -101},
    {"inherit", -101},
    {"inline", CSS_CONST_AQUA - 100},
    {"inline-block", -101},
    {"inline-table", -101},
    {"inset", -101},
    {"inside", CSS_CONST_GEORGIAN - 100},
    {"italic", CSS_CONST_CROSSHAIR - 100},
    {"justify", -101},
    {"large", -101},
    {"larger", -101},
    {"left", -101},
    {"lighter", -101},
    {"lime", CSS_CONST_INLINE_BLOCK - 100},
    {"line-through", -101},
    {"list-item", -101},
    {"lower-alpha", -101},
    {"lower-greek", -101},
    {"lower-latin", -101},
    {"lower-roman", CSS_CONST_HIDDEN - 100},
    {"lowercase", -101},
    {"ltr", -101},
    {"marker", CSS_CONST_GROOVE - 100},
    {"maroon", CSS_CONST_ARMENIAN - 100},
    {"medium", CSS_CONST_LINE_THROUGH - 100},
    {"middle", CSS_CONST_COMPACT - 100},
    {"move", -101},
    {"n-resize", CSS_CONST_LARGER - 100},
    {"navy", -101},
    {"ne-resize", -101},
    {"no-repeat", -101},
    {"none", -101},
    {"normal", CSS_CONST_BOLD - 100},
    {"nowrap", -101},
    {"nw-resize", CSS_CONST_NO_REPEAT - 100},
    {"oblique", CSS_CONST_NORMAL - 100},
    {"olive", CSS_CONST_LIST_ITEM - 100},
    {"outset", CSS_CONST_DISC - 100},
    {"outside", CSS_CONST_LTR - 100},
    {"overline", -101},
    {"pointer", -101},
    {"pre", -101},
    {"progress", CSS_CONST_DECIMAL_LEADING_ZERO - 100},
    {"purple", CSS_CONST_NOWRAP - 100},
    {"red", -101},
    {"relative", -101},
    {"repeat", -101},
    {"repeat-x", CSS_CONST__TKHTML_LEFT - 100},
    {"repeat-y", CSS_CONST_DECIMAL - 100},
    {"ridge", -101},
    {"right", CSS_CONST_REPEAT_X - 100},
    {"rtl", -101},
    {"run-in", CSS_CONST_NAVY - 100},
    {"s-resize", -101},
    {"scroll", -101},
    {"se-resize", -101},
    {"separate", -101},
    {"show", CSS_CONST_AUTO - 100},
    {"silver", -101},
    {"small", -101},
    {"small-caps", -101},
    {"smaller", CSS_CONST_MOVE - 100},
    {"solid", CSS_CONST_INSET - 100},
    {"square", CSS_CONST_ITALIC - 100},
    {"static", CSS_CONST_OLIVE - 100},
    {"sub", CSS_CONST_S_RESIZE - 100},
    {"super", CSS_CONST_FIXED - 100},
    {"sw-resize", -101},
    {"table", CSS_CONST_LIGHTER - 100},
    {"table-caption", CSS_CONST_DOUBLE - 100},
    {"table-cell", CSS_CONST_BLACK - 100},
    {"table-column", CSS_CONST_E_RESIZE - 100},
    {"table-column-group", CSS_CONST_SILVER - 100},
    {"table-footer-group", -101},
    {"table-header-group", -101},
    {"table-row", -101},
    {"table-row-group", -101},
    {"teal", -101},
    {"text", CSS_CONST_TABLE_ROW - 100},
    {"text-bottom", -101},
    {"text-top", -101},
    {"thick", CSS_CONST_LARGE - 100},
    {"thin", CSS_CONST_RIDGE - 100},
    {"top", CSS_CONST_OVERLINE - 100},
    {"transparent", -101},
    {"underline", CSS_CONST_SHOW - 100},
    {"upper-alpha", CSS_CONST_TABLE_ROW_GROUP - 100},
    {"upper-latin", CSS_CONST_OBLIQUE - 100},
    {"upper-roman", -101},
    {"uppercase", -101},
    {"visible", -101},
    {"w-resize", CSS_CONST_LOWER_LATIN - 100},
    {"wait", CSS_CONST_HIDE - 100},
    {"white", CSS_CONST_POINTER - 100},
    {"x-large", -101},
    {"x-small", CSS_CONST_WHITE - 100},
    {"xx-large", CSS_CONST_NONE - 100},
    {"xx-small", CSS_CONST_SW_RESIZE - 100},
    {"yellow", -101},
};


int
HtmlCssConstantLookup(n, z)
    int n;
    const char *z;
{
    int aTable[] = {
        CSS_CONST_LIME - 100, CSS_CONST_MEDIUM - 100, -101, 
        CSS_CONST_RED - 100, -101, -101, CSS_CONST_XX_SMALL - 100, 
        CSS_CONST_TEXT_BOTTOM - 100, -101, CSS_CONST_JUSTIFY - 100, 
        CSS_CONST_TEXT - 100, CSS_CONST_TOP - 100, CSS_CONST_MAROON - 100, 
        CSS_CONST_TRANSPARENT - 100, -101, CSS_CONST_SMALLER - 100, 
        CSS_CONST_SUPER - 100, CSS_CONST_VISIBLE - 100, -101, -101, 
        CSS_CONST_LOWER_ROMAN - 100, CSS_CONST_BIDI_OVERRIDE - 100, -101, 
        CSS_CONST_X_LARGE - 100, -101, -101, -101, CSS_CONST_BASELINE - 100, 
        CSS_CONST_INSIDE - 100, CSS_CONST_SUB - 100, CSS_CONST_TEAL - 100, 
        CSS_CONST_STATIC - 100, CSS_CONST_ABSOLUTE - 100, -101, 
        CSS_CONST_EMBED - 100, CSS_CONST_FUCHSIA - 100, 
        CSS_CONST_LOWER_GREEK - 100, -101, -101, CSS_CONST_SCROLL - 100, 
        CSS_CONST_PRE - 100, -101, CSS_CONST_TABLE_COLUMN_GROUP - 100, 
        CSS_CONST_LOWER_ALPHA - 100, CSS_CONST_INLINE_TABLE - 100, 
        CSS_CONST_SEPARATE - 100, CSS_CONST_CENTER - 100, -101, 
        CSS_CONST_TABLE_HEADER_GROUP - 100, 
        CSS_CONST__TKHTML_INLINE_BUTTON - 100, CSS_CONST_NE_RESIZE - 100, 
        CSS_CONST_TABLE_CAPTION - 100, -101, -101, -101, 
        CSS_CONST_PURPLE - 100, -101, CSS_CONST_W_RESIZE - 100, 
        CSS_CONST_TABLE_CELL - 100, CSS_CONST_THICK - 100, -101, -101, 
        CSS_CONST_UNDERLINE - 100, CSS_CONST_BLUE - 100, -101, 
        CSS_CONST_SQUARE - 100, -101, CSS_CONST_GRAY - 100, 
        CSS_CONST_TABLE_COLUMN - 100, CSS_CONST_TEXT_TOP - 100, -101, -101, 
        -101, CSS_CONST_TABLE_FOOTER_GROUP - 100, CSS_CONST_WAIT - 100, 
        CSS_CONST_NW_RESIZE - 100, CSS_CONST_INHERIT - 100, 
        CSS_CONST_OUTSIDE - 100, CSS_CONST_X_SMALL - 100, 
        CSS_CONST_BOLDER - 100, -101, CSS_CONST_CAPITALIZE - 100, 
        CSS_CONST_BOTH - 100, -101, CSS_CONST_SOLID - 100, 
        CSS_CONST_LOWERCASE - 100, -101, -101, -101, CSS_CONST_HELP - 100, 
        -101, CSS_CONST_THIN - 100, CSS_CONST_RELATIVE - 100, -101, 
        CSS_CONST_SMALL - 100, CSS_CONST_XX_LARGE - 100, 
        CSS_CONST_TABLE - 100, CSS_CONST_UPPER_LATIN - 100, -101, -101, 
        CSS_CONST_COLLAPSE - 100, CSS_CONST_PROGRESS - 100, 
        CSS_CONST_RIGHT - 100, CSS_CONST_REPEAT_Y - 100, 
        CSS_CONST_MIDDLE - 100, -101, CSS_CONST_SMALL_CAPS - 100, 
        CSS_CONST_OUTSET - 100, -101, CSS_CONST_RTL - 100, -101, 
        CSS_CONST_UPPER_ALPHA - 100, -101, CSS_CONST_RUN_IN - 100, 
        CSS_CONST_MARKER - 100, -101, CSS_CONST_LEFT - 100, 
        CSS_CONST_DEFAULT - 100, CSS_CONST_REPEAT - 100, 
        CSS_CONST_SE_RESIZE - 100, CSS_CONST_INLINE - 100, 
        CSS_CONST_UPPER_ROMAN - 100, CSS_CONST_N_RESIZE - 100, -101, 
        CSS_CONST_YELLOW - 100, -101, -101, CSS_CONST_UPPERCASE - 100, 
    };
    return Lookup(n, z, aTable, aHtmlCssConstant) + 100;
}


const char *
HtmlCssConstantToString(e)
    int e;
{
    return aHtmlCssConstant[e - 100].zString;
}

static const HashEntry aHtmlCssProperty[] = {
    {"-tkhtml-ordered-list-start", -1},
    {"-tkhtml-ordered-list-value", -1},
    {"-tkhtml-replacement-image", -1},
    {"azimuth", -1},
    {"background-attachment", -1},
    {"background-color", -1},
    {"background-image", -1},
    {"background-position-x", -1},
    {"background-position-y", -1},
    {"background-repeat", CSS_PROPERTY__TKHTML_REPLACEMENT_IMAGE - 0},
    {"border-bottom-color", -1},
    {"border-bottom-style", -1},
    {"border-bottom-width", -1},
    {"border-collapse", -1},
    {"border-left-color", -1},
    {"border-left-style", -1},
    {"border-left-width", -1},
    {"border-right-color", -1},
    {"border-right-style", CSS_PROPERTY_BACKGROUND_POSITION_X - 0},
    {"border-right-width", -1},
    {"border-spacing", -1},
    {"border-top-color", -1},
    {"border-top-style", -1},
    {"border-top-width", -1},
    {"bottom", -1},
    {"caption-side", -1},
    {"clear", -1},
    {"clip", -1},
    {"color", -1},
    {"content", -1},
    {"counter-increment", CSS_PROPERTY_BOTTOM - 0},
    {"counter-reset", -1},
    {"cue-after", CSS_PROPERTY_BORDER_BOTTOM_COLOR - 0},
    {"cue-before", -1},
    {"cursor", -1},
    {"direction", -1},
    {"display", -1},
    {"elevation", -1},
    {"empty-cells", -1},
    {"float", -1},
    {"font-family", CSS_PROPERTY_DISPLAY - 0},
    {"font-size", -1},
    {"font-size-adjust", CSS_PROPERTY_COLOR - 0},
    {"font-stretch", -1},
    {"font-style", -1},
    {"font-variant", -1},
    {"font-weight", -1},
    {"height", CSS_PROPERTY_BORDER_RIGHT_WIDTH - 0},
    {"left", -1},
    {"letter-spacing", -1},
    {"line-height", -1},
    {"list-style-image", -1},
    {"list-style-position", -1},
    {"list-style-type", CSS_PROPERTY_DIRECTION - 0},
    {"margin-bottom", -1},
    {"margin-left", CSS_PROPERTY_FONT_STRETCH - 0},
    {"margin-right", -1},
    {"margin-top", CSS_PROPERTY_FONT_VARIANT - 0},
    {"marker-offset", -1},
    {"marks", -1},
    {"max-height", -1},
    {"max-width", -1},
    {"min-height", CSS_PROPERTY_BORDER_RIGHT_COLOR - 0},
    {"min-width", CSS_PROPERTY_MARGIN_RIGHT - 0},
    {"orphans", CSS_PROPERTY_BORDER_LEFT_WIDTH - 0},
    {"outline-color", -1},
    {"outline-style", CSS_PROPERTY_MARKS - 0},
    {"outline-width", -1},
    {"overflow", -1},
    {"padding-bottom", -1},
    {"padding-left", -1},
    {"padding-right", CSS_PROPERTY_AZIMUTH - 0},
    {"padding-top", CSS_PROPERTY_FONT_SIZE - 0},
    {"page", -1},
    {"page-break-after", CSS_PROPERTY_MARKER_OFFSET - 0},
    {"page-break-before", CSS_PROPERTY__TKHTML_ORDERED_LIST_START - 0},
    {"page-break-inside", -1},
    {"pause", CSS_PROPERTY_HEIGHT - 0},
    {"pause-after", -1},
    {"pause-before", CSS_PROPERTY__TKHTML_ORDERED_LIST_VALUE - 0},
    {"pitch", -1},
    {"pitch-range", CSS_PROPERTY_MIN_HEIGHT - 0},
    {"play-during", -1},
    {"position", CSS_PROPERTY_LETTER_SPACING - 0},
    {"quotes", CSS_PROPERTY_LINE_HEIGHT - 0},
    {"richness", CSS_PROPERTY_PADDING_TOP - 0},
    {"right", -1},
    {"size", -1},
    {"speak", -1},
    {"speak-header", CSS_PROPERTY_LIST_STYLE_IMAGE - 0},
    {"speak-numeral", -1},
    {"speak-punctuation", -1},
    {"speech-rate", -1},
    {"stress", -1},
    {"table-layout", -1},
    {"text-align", CSS_PROPERTY_SPEAK_NUMERAL - 0},
    {"text-decoration", -1},
    {"text-indent", CSS_PROPERTY_TABLE_LAYOUT - 0},
    {"text-shadow", CSS_PROPERTY_BORDER_RIGHT_STYLE - 0},
    {"text-transform", -1},
    {"top", CSS_PROPERTY_STRESS - 0},
    {"unicode-bidi", CSS_PROPERTY_SIZE - 0},
    {"vertical-align", CSS_PROPERTY_COUNTER_INCREMENT - 0},
    {"visibility", CSS_PROPERTY_PAGE_BREAK_BEFORE - 0},
    {"voice-family", -1},
    {"volume", CSS_PROPERTY_BACKGROUND_IMAGE - 0},
    {"white-space", -1},
    {"widows", CSS_PROPERTY_PAUSE_AFTER - 0},
    {"width", -1},
    {"word-spacing", -1},
    {"z-index", CSS_PROPERTY_VOLUME - 0},
    {"background", CSS_PROPERTY_BORDER_SPACING - 0},
    {"background-position", CSS_PROPERTY_OUTLINE_WIDTH - 0},
    {"border", CSS_PROPERTY_FONT_FAMILY - 0},
    {"border-bottom", CSS_PROPERTY_RICHNESS - 0},
    {"border-color", CSS_PROPERTY_PITCH_RANGE - 0},
    {"border-left", -1},
    {"border-right", CSS_PROPERTY_BORDER_COLLAPSE - 0},
    {"border-style", CSS_PROPERTY_BACKGROUND_COLOR - 0},
    {"border-top", -1},
    {"border-width", CSS_PROPERTY_OVERFLOW - 0},
    {"cue", CSS_PROPERTY_WHITE_SPACE - 0},
    {"font", CSS_PROPERTY_MARGIN_BOTTOM - 0},
    {"list-style", -1},
    {"margin", CSS_PROPERTY_CURSOR - 0},
    {"outline", CSS_PROPERTY_TEXT_SHADOW - 0},
    {"padding", CSS_PROPERTY_CLEAR - 0},
};


int
HtmlCssPropertyLookup(n, z)
    int n;
    const char *z;
{
    int aTable[] = {
        -1, -1, CSS_PROPERTY_TEXT_DECORATION - 0, 
        CSS_PROPERTY_BORDER_TOP_WIDTH - 0, -1, 
        CSS_SHORTCUTPROPERTY_BORDER_COLOR - 0, CSS_PROPERTY_QUOTES - 0, 
        CSS_PROPERTY_WIDTH - 0, CSS_PROPERTY_PITCH - 0, -1, -1, 
        CSS_PROPERTY_TOP - 0, CSS_PROPERTY_SPEAK - 0, -1, 
        CSS_PROPERTY_OUTLINE_STYLE - 0, -1, -1, -1, 
        CSS_PROPERTY_SPEAK_HEADER - 0, CSS_SHORTCUTPROPERTY_BORDER_WIDTH - 0, 
        -1, CSS_PROPERTY_BORDER_TOP_COLOR - 0, 
        CSS_PROPERTY_BACKGROUND_ATTACHMENT - 0, -1, 
        CSS_SHORTCUTPROPERTY_FONT - 0, -1, 
        CSS_SHORTCUTPROPERTY_BACKGROUND_POSITION - 0, -1, -1, 
        CSS_PROPERTY_WIDOWS - 0, CSS_PROPERTY_PAUSE - 0, -1, 
        CSS_PROPERTY_FONT_SIZE_ADJUST - 0, -1, 
        CSS_PROPERTY_VERTICAL_ALIGN - 0, 
        CSS_PROPERTY_LIST_STYLE_POSITION - 0, CSS_PROPERTY_PLAY_DURING - 0, 
        CSS_SHORTCUTPROPERTY_LIST_STYLE - 0, -1, -1, 
        CSS_SHORTCUTPROPERTY_BACKGROUND - 0, 
        CSS_PROPERTY_LIST_STYLE_TYPE - 0, CSS_SHORTCUTPROPERTY_CUE - 0, 
        CSS_PROPERTY_MAX_WIDTH - 0, CSS_PROPERTY_BORDER_TOP_STYLE - 0, 
        CSS_PROPERTY_TEXT_ALIGN - 0, -1, 
        CSS_SHORTCUTPROPERTY_BORDER_BOTTOM - 0, 
        CSS_PROPERTY_PADDING_LEFT - 0, CSS_PROPERTY_BACKGROUND_REPEAT - 0, 
        CSS_PROPERTY_VOICE_FAMILY - 0, CSS_PROPERTY_OUTLINE_COLOR - 0, 
        CSS_PROPERTY_SPEAK_PUNCTUATION - 0, CSS_PROPERTY_PADDING_RIGHT - 0, 
        CSS_PROPERTY_TEXT_TRANSFORM - 0, -1, CSS_PROPERTY_CLIP - 0, 
        CSS_SHORTCUTPROPERTY_BORDER - 0, -1, -1, 
        CSS_SHORTCUTPROPERTY_BORDER_STYLE - 0, CSS_PROPERTY_CUE_BEFORE - 0, 
        CSS_PROPERTY_MARGIN_TOP - 0, -1, -1, CSS_PROPERTY_PAUSE_BEFORE - 0, 
        CSS_PROPERTY_BORDER_LEFT_COLOR - 0, CSS_PROPERTY_CONTENT - 0, -1, 
        CSS_PROPERTY_POSITION - 0, CSS_PROPERTY_WORD_SPACING - 0, 
        CSS_SHORTCUTPROPERTY_OUTLINE - 0, 
        CSS_PROPERTY_BACKGROUND_POSITION_Y - 0, 
        CSS_SHORTCUTPROPERTY_BORDER_TOP - 0, -1, 
        CSS_PROPERTY_TEXT_INDENT - 0, -1, CSS_PROPERTY_FONT_STYLE - 0, 
        CSS_SHORTCUTPROPERTY_MARGIN - 0, -1, -1, CSS_PROPERTY_FLOAT - 0, -1, 
        CSS_PROPERTY_MAX_HEIGHT - 0, CSS_PROPERTY_FONT_WEIGHT - 0, -1, 
        CSS_PROPERTY_MARGIN_LEFT - 0, CSS_PROPERTY_CAPTION_SIDE - 0, 
        CSS_PROPERTY_PAGE_BREAK_INSIDE - 0, 
        CSS_PROPERTY_BORDER_BOTTOM_STYLE - 0, CSS_PROPERTY_PAGE - 0, 
        CSS_PROPERTY_ORPHANS - 0, -1, -1, -1, -1, 
        CSS_SHORTCUTPROPERTY_PADDING - 0, CSS_PROPERTY_PADDING_BOTTOM - 0, 
        -1, CSS_PROPERTY_UNICODE_BIDI - 0, CSS_PROPERTY_CUE_AFTER - 0, -1, 
        CSS_PROPERTY_RIGHT - 0, -1, CSS_PROPERTY_COUNTER_RESET - 0, 
        CSS_PROPERTY_SPEECH_RATE - 0, -1, -1, -1, -1, -1, 
        CSS_PROPERTY_VISIBILITY - 0, CSS_PROPERTY_ELEVATION - 0, 
        CSS_SHORTCUTPROPERTY_BORDER_LEFT - 0, -1, -1, CSS_PROPERTY_LEFT - 0, 
        -1, CSS_PROPERTY_MIN_WIDTH - 0, CSS_PROPERTY_EMPTY_CELLS - 0, -1, 
        CSS_PROPERTY_PAGE_BREAK_AFTER - 0, -1, 
        CSS_PROPERTY_BORDER_LEFT_STYLE - 0, -1, 
        CSS_PROPERTY_BORDER_BOTTOM_WIDTH - 0, 
        CSS_SHORTCUTPROPERTY_BORDER_RIGHT - 0, CSS_PROPERTY_Z_INDEX - 0, 
    };
    return Lookup(n, z, aTable, aHtmlCssProperty) + 0;
}


const char *
HtmlCssPropertyToString(e)
    int e;
{
    return aHtmlCssProperty[e - 0].zString;
}
static unsigned char enumdata[] = {CSS_PROPERTY_VISIBILITY, CSS_CONST_VISIBLE, CSS_CONST_COLLAPSE, CSS_CONST_HIDDEN, 0, CSS_PROPERTY_FONT_VARIANT, CSS_CONST_NORMAL, CSS_CONST_SMALL_CAPS, 0, CSS_PROPERTY_TEXT_DECORATION, CSS_CONST_NONE, CSS_CONST_LINE_THROUGH, CSS_CONST_OVERLINE, CSS_CONST_UNDERLINE, 0, CSS_PROPERTY_BORDER_LEFT_STYLE, CSS_CONST_NONE, CSS_CONST_DASHED, CSS_CONST_DOTTED, CSS_CONST_DOUBLE, CSS_CONST_GROOVE, CSS_CONST_HIDDEN, CSS_CONST_INSET, CSS_CONST_OUTSET, CSS_CONST_RIDGE, CSS_CONST_SOLID, 0, CSS_PROPERTY_POSITION, CSS_CONST_STATIC, CSS_CONST_ABSOLUTE, CSS_CONST_FIXED, CSS_CONST_RELATIVE, 0, CSS_PROPERTY_TEXT_TRANSFORM, CSS_CONST_NONE, CSS_CONST_CAPITALIZE, CSS_CONST_LOWERCASE, CSS_CONST_UPPERCASE, 0, CSS_PROPERTY_BACKGROUND_ATTACHMENT, CSS_CONST_SCROLL, CSS_CONST_FIXED, 0, CSS_PROPERTY_BORDER_BOTTOM_STYLE, CSS_CONST_NONE, CSS_CONST_DASHED, CSS_CONST_DOTTED, CSS_CONST_DOUBLE, CSS_CONST_GROOVE, CSS_CONST_HIDDEN, CSS_CONST_INSET, CSS_CONST_OUTSET, CSS_CONST_RIDGE, CSS_CONST_SOLID, 0, CSS_PROPERTY_DISPLAY, CSS_CONST_INLINE, CSS_CONST__TKHTML_INLINE_BUTTON, CSS_CONST_BLOCK, CSS_CONST_COMPACT, CSS_CONST_INLINE_BLOCK, CSS_CONST_INLINE_TABLE, CSS_CONST_LIST_ITEM, CSS_CONST_MARKER, CSS_CONST_NONE, CSS_CONST_RUN_IN, CSS_CONST_TABLE, CSS_CONST_TABLE_CAPTION, CSS_CONST_TABLE_CELL, CSS_CONST_TABLE_COLUMN, CSS_CONST_TABLE_COLUMN_GROUP, CSS_CONST_TABLE_FOOTER_GROUP, CSS_CONST_TABLE_HEADER_GROUP, CSS_CONST_TABLE_ROW, CSS_CONST_TABLE_ROW_GROUP, 0, CSS_PROPERTY_EMPTY_CELLS, CSS_CONST_SHOW, CSS_CONST_HIDE, 0, CSS_PROPERTY_BORDER_RIGHT_STYLE, CSS_CONST_NONE, CSS_CONST_DASHED, CSS_CONST_DOTTED, CSS_CONST_DOUBLE, CSS_CONST_GROOVE, CSS_CONST_HIDDEN, CSS_CONST_INSET, CSS_CONST_OUTSET, CSS_CONST_RIDGE, CSS_CONST_SOLID, 0, CSS_PROPERTY_CAPTION_SIDE, CSS_CONST_TOP, CSS_CONST_BOTTOM, 0, CSS_PROPERTY_LIST_STYLE_TYPE, CSS_CONST_DISC, CSS_CONST_ARMENIAN, CSS_CONST_CIRCLE, CSS_CONST_DECIMAL, CSS_CONST_DECIMAL_LEADING_ZERO, CSS_CONST_GEORGIAN, CSS_CONST_LOWER_ALPHA, CSS_CONST_LOWER_GREEK, CSS_CONST_LOWER_LATIN, CSS_CONST_LOWER_ROMAN, CSS_CONST_NONE, CSS_CONST_SQUARE, CSS_CONST_UPPER_ALPHA, CSS_CONST_UPPER_LATIN, CSS_CONST_UPPER_ROMAN, 0, CSS_PROPERTY_DIRECTION, CSS_CONST_LTR, CSS_CONST_RTL, 0, CSS_PROPERTY_WHITE_SPACE, CSS_CONST_NORMAL, CSS_CONST_NOWRAP, CSS_CONST_PRE, 0, CSS_PROPERTY_TABLE_LAYOUT, CSS_CONST_AUTO, CSS_CONST_FIXED, 0, CSS_PROPERTY_UNICODE_BIDI, CSS_CONST_NORMAL, CSS_CONST_BIDI_OVERRIDE, CSS_CONST_EMBED, 0, CSS_PROPERTY_OVERFLOW, CSS_CONST_VISIBLE, CSS_CONST_AUTO, CSS_CONST_HIDDEN, CSS_CONST_SCROLL, 0, CSS_PROPERTY_LIST_STYLE_POSITION, CSS_CONST_OUTSIDE, CSS_CONST_INSIDE, 0, CSS_PROPERTY_BORDER_TOP_STYLE, CSS_CONST_NONE, CSS_CONST_DASHED, CSS_CONST_DOTTED, CSS_CONST_DOUBLE, CSS_CONST_GROOVE, CSS_CONST_HIDDEN, CSS_CONST_INSET, CSS_CONST_OUTSET, CSS_CONST_RIDGE, CSS_CONST_SOLID, 0, CSS_PROPERTY_TEXT_ALIGN, CSS_CONST_LEFT, CSS_CONST__TKHTML_CENTER, CSS_CONST__TKHTML_LEFT, CSS_CONST__TKHTML_RIGHT, CSS_CONST_CENTER, CSS_CONST_JUSTIFY, CSS_CONST_RIGHT, 0, CSS_PROPERTY_OUTLINE_STYLE, CSS_CONST_NONE, CSS_CONST_DASHED, CSS_CONST_DOTTED, CSS_CONST_DOUBLE, CSS_CONST_GROOVE, CSS_CONST_HIDDEN, CSS_CONST_INSET, CSS_CONST_OUTSET, CSS_CONST_RIDGE, CSS_CONST_SOLID, 0, CSS_PROPERTY_FLOAT, CSS_CONST_NONE, CSS_CONST_LEFT, CSS_CONST_RIGHT, 0, CSS_PROPERTY_BACKGROUND_REPEAT, CSS_CONST_REPEAT, CSS_CONST_NO_REPEAT, CSS_CONST_REPEAT_X, CSS_CONST_REPEAT_Y, 0, CSS_PROPERTY_BORDER_COLLAPSE, CSS_CONST_SEPARATE, CSS_CONST_COLLAPSE, 0, CSS_PROPERTY_CURSOR, CSS_CONST_AUTO, CSS_CONST_CROSSHAIR, CSS_CONST_DEFAULT, CSS_CONST_E_RESIZE, CSS_CONST_HELP, CSS_CONST_MOVE, CSS_CONST_N_RESIZE, CSS_CONST_NE_RESIZE, CSS_CONST_NW_RESIZE, CSS_CONST_POINTER, CSS_CONST_PROGRESS, CSS_CONST_S_RESIZE, CSS_CONST_SE_RESIZE, CSS_CONST_SW_RESIZE, CSS_CONST_TEXT, CSS_CONST_W_RESIZE, CSS_CONST_WAIT, 0, CSS_PROPERTY_CLEAR, CSS_CONST_NONE, CSS_CONST_BOTH, CSS_CONST_LEFT, CSS_CONST_RIGHT, 0, CSS_PROPERTY_MAX_PROPERTY+1};
unsigned char *HtmlCssEnumeratedValues(int eProp){
    static int isInit = 0;
    static int aProps[CSS_PROPERTY_MAX_PROPERTY+1];
    if (0 == isInit) {
        int novalue = sizeof(enumdata) - 2;
        int i;
        for (i = 0; i < CSS_PROPERTY_MAX_PROPERTY+1; i++){
            aProps[i] = novalue;
        }
        i = 0;
        while (enumdata[i] != CSS_PROPERTY_MAX_PROPERTY+1){
            assert(enumdata[i] <= CSS_PROPERTY_MAX_PROPERTY);
            assert(enumdata[i] > 0);
            aProps[enumdata[i]] = i + 1;
            while( enumdata[i] ) i++;
            i++;
        }
        isInit = 1;
    }

    return &enumdata[aProps[eProp]];
}

