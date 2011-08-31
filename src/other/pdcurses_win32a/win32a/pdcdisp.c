/* Public Domain Curses */

#include "pdcwin.h"

RCSID("$Id$")

#include <stdlib.h>
#include <string.h>

#define NON_BLINKING_CURSOR   0x10
// #define NO_WIDE_CHARS

// #ifdef CHTYPE_LONG

# define A(x) ((chtype)x | A_ALTCHARSET)

/* The console version of this has four '# ifdef PDC_WIDE's in the
acs_map definition.  In this "real Windows" version,  though,  we
_always_ want to use the wide versions,  so I've changed all four to read
USE_WIDE_ALWAYS, and then defined that to be true.  One could, of course,
just lop out the non-wide versions,  but this way,  the code doesn't
actually change all that much,  which may make maintenance easier. */

#define USE_WIDE_ALWAYS

chtype acs_map[128] =
{
    A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9), A(10),
    A(11), A(12), A(13), A(14), A(15), A(16), A(17), A(18), A(19),
    A(20), A(21), A(22), A(23), A(24), A(25), A(26), A(27), A(28),
    A(29), A(30), A(31), ' ', '!', '"', '#', '$', '%', '&', '\'', '(',
    ')', '*',

# ifdef USE_WIDE_ALWAYS
    0x2192, 0x2190, 0x2191, 0x2193,
# else
    A(0x1a), A(0x1b), A(0x18), A(0x19),
# endif

    '/',

# ifdef USE_WIDE_ALWAYS
    0x2588,
# else
    0xdb,
# endif

    '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=',
    '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',

# ifdef USE_WIDE_ALWAYS
    0x2666, 0x2592,
# else
    A(0x04), 0xb1,
# endif

    'b', 'c', 'd', 'e',

# ifdef USE_WIDE_ALWAYS
    0x00b0, 0x00b1, 0x2591, 0x00a4, 0x2518, 0x2510, 0x250c, 0x2514,
    0x253c, 0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524,
    0x2534, 0x252c, 0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3,
    0x00b7,
# else
    0xf8, 0xf1, 0xb0, A(0x0f), 0xd9, 0xbf, 0xda, 0xc0, 0xc5, 0x2d, 0x2d,
    0xc4, 0x2d, 0x5f, 0xc3, 0xb4, 0xc1, 0xc2, 0xb3, 0xf3, 0xf2, 0xe3,
    0xd8, 0x9c, 0xf9,
# endif

    A(127)
};

# undef A

// #endif

static const unsigned short starting_ascii_to_unicode[32] = {
   0,
   0x263a,        /*   1 smiling face              */
   0x263b,        /*   2 smiling face inverted     */
   0x2665,        /*   3 heart                     */
   0x2666,        /*   4 diamond                   */
   0x2663,        /*   5 club                      */
   0x2660,        /*   6 spade                     */
   0x2024,        /*   7 small bullet              */
   0x25d8,        /*   8 inverted bullet           */
   0x25bc,        /*   9 hollow bullet             */
   0x25d9,        /*  10 inverted hollow bullet    */
   0x2642,        /*  11 male/Mars symbol          */
   0x2640,        /*  12 female/Venus symbol       */
   0x266a,        /*  13 eighth note               */
   0x266c,        /*  14 two sixteenth notes       */
   0x263c,        /*  15 splat                     */
   0x25b6,        /*  16 right-pointing triangle   */
   0x25c0,        /*  17 left-pointing triangle    */
   0x2195,        /*  18 double up/down arrow      */
   0x203c,        /*  19 double exclamation !!     */
   0x00b6,        /*  20 pilcrow                   */
   0xa7,          /*  21                           */
   0x2582,        /*  22 lower 1/3 block           */
   0x280d,        /*  23 double up/down arrow      */
   0x2191,        /*  24 up arrow                  */
   0x2193,        /*  25 down arrow                */
   0x2192,        /*  26 right arrow               */
   0x2190,        /*  27 left arrow                */
   0x2319,        /*  28                           */
   0x280c,        /*  29 left & right arrow        */
   0x25b2,        /*  30 up triangle               */
   0x25bc};       /*  31 down triangle             */

static const unsigned short extended_ascii_to_unicode[128] = {
  0x00C7,  /* 128   80  €  latin capital letter c with cedilla                */
  0x00FC,  /* 129   81    latin small letter u with diaeresis                */
  0x00E9,  /* 130   82  ‚  latin small letter e with acute                    */
  0x00E2,  /* 131   83  ƒ  latin small letter a with circumflex               */
  0x00E4,  /* 132   84  „  latin small letter a with diaeresis                */
  0x00E0,  /* 133   85  …  latin small letter a with grave                    */
  0x00E5,  /* 134   86  †  latin small letter a with ring above               */
  0x00E7,  /* 135   87  ‡  latin small letter c with cedilla                  */
  0x00EA,  /* 136   88  ˆ  latin small letter e with circumflex               */
  0x00EB,  /* 137   89  ‰  latin small letter e with diaeresis                */
  0x00E8,  /* 138   8A  Š  latin small letter e with grave                    */
  0x00EF,  /* 139   8B  ‹  latin small letter i with diaeresis                */
  0x00EE,  /* 140   8C  Œ  latin small letter i with circumflex               */
  0x00EC,  /* 141   8D    latin small letter i with grave                    */
  0x00C4,  /* 142   8E  Ž  latin capital letter a with diaeresis              */
  0x00C5,  /* 143   8F    latin capital letter a with ring above             */
  0x00C9,  /* 144   90    latin capital letter e with acute                  */
  0x00E6,  /* 145   91  ‘  latin small ligature ae                            */
  0x00C6,  /* 146   92  ’  latin capital ligature ae                          */
  0x00F4,  /* 147   93  “  latin small letter o with circumflex               */
  0x00F6,  /* 148   94  ”  latin small letter o with diaeresis                */
  0x00F2,  /* 149   95  •  latin small letter o with grave                    */
  0x00FB,  /* 150   96  –  latin small letter u with circumflex               */
  0x00F9,  /* 151   97  —  latin small letter u with grave                    */
  0x00FF,  /* 152   98  ˜  latin small letter y with diaeresis                */
  0x00D6,  /* 153   99  ™  latin capital letter o with diaeresis              */
  0x00DC,  /* 154   9A  š  latin capital letter u with diaeresis              */
  0x00A2,  /* 155   9B  ›  cent sign                                          */
  0x00A3,  /* 156   9C  œ  pound sign                                         */
  0x00A5,  /* 157   9D    yen sign                                           */
  0x20A7,  /* 158   9E  ?  peseta sign                                        */
  0x0192,  /* 159   9F  Ÿ  latin small letter f with hook                     */
  0x00E1,  /* 160   A0     latin small letter a with acute                    */
  0x00ED,  /* 161   A1  ¡  latin small letter i with acute                    */
  0x00F3,  /* 162   A2  ¢  latin small letter o with acute                    */
  0x00FA,  /* 163   A3  £  latin small letter u with acute                    */
  0x00F1,  /* 164   A4  ¤  latin small letter n with tilde                    */
  0x00D1,  /* 165   A5  ¥  latin capital letter n with tilde                  */
  0x00AA,  /* 166   A6  ¦  feminine ordinal indicator                         */
  0x00BA,  /* 167   A7  §  masculine ordinal indicator                        */
  0x00BF,  /* 168   A8  ¨  inverted question mark                             */
  0x2310,  /* 169   A9  ?  reversed not sign                                  */
  0x00AC,  /* 170   AA  ª  not sign                                           */
  0x00BD,  /* 171   AB  «  vulgar fraction one half                           */
  0x00BC,  /* 172   AC  ¬  vulgar fraction one quarter                        */
  0x00A1,  /* 173   AD  ­  inverted exclamation mark                          */
  0x00AB,  /* 174   AE  ®  left-pointing double angle quotation mark          */
  0x00BB,  /* 175   AF  ¯  right-pointing double angle quotation mark         */
  0x2591,  /* 176   B0  °  light shade                                        */
  0x2592,  /* 177   B1  ±  medium shade                                       */
  0x2593,  /* 178   B2  ²  dark shade                                         */
  0x2502,  /* 179   B3  ³  box drawings light vertical                        */
  0x2524,  /* 180   B4  ´  box drawings light vertical and left               */
  0x2561,  /* 181   B5  µ  box drawings vertical single and left double       */
  0x2562,  /* 182   B6  ¶  box drawings vertical double and left single       */
  0x2556,  /* 183   B7  ·  box drawings down double and left single           */
  0x2555,  /* 184   B8  ¸  box drawings down single and left double           */
  0x2563,  /* 185   B9  ¹  box drawings double vertical and left              */
  0x2551,  /* 186   BA  º  box drawings double vertical                       */
  0x2557,  /* 187   BB  »  box drawings double down and left                  */
  0x255D,  /* 188   BC  ¼  box drawings double up and left                    */
  0x255C,  /* 189   BD  ½  box drawings up double and left single             */
  0x255B,  /* 190   BE  ¾  box drawings up single and left double             */
  0x2510,  /* 191   BF  ¿  box drawings light down and left                   */
  0x2514,  /* 192   C0  À  box drawings light up and right                    */
  0x2534,  /* 193   C1  Á  box drawings light up and horizontal               */
  0x252C,  /* 194   C2  Â  box drawings light down and horizontal             */
  0x251C,  /* 195   C3  Ã  box drawings light vertical and right              */
  0x2500,  /* 196   C4  Ä  box drawings light horizontal                      */
  0x253C,  /* 197   C5  Å  box drawings light vertical and horizontal         */
  0x255E,  /* 198   C6  Æ  box drawings vertical single and right double      */
  0x255F,  /* 199   C7  Ç  box drawings vertical double and right single      */
  0x255A,  /* 200   C8  È  box drawings double up and right                   */
  0x2554,  /* 201   C9  É  box drawings double down and right                 */
  0x2569,  /* 202   CA  Ê  box drawings double up and horizontal              */
  0x2566,  /* 203   CB  Ë  box drawings double down and horizontal            */
  0x2560,  /* 204   CC  Ì  box drawings double vertical and right             */
  0x2550,  /* 205   CD  Í  box drawings double horizontal                     */
  0x256C,  /* 206   CE  Î  box drawings double vertical and horizontal        */
  0x2567,  /* 207   CF  Ï  box drawings up single and horizontal double       */
  0x2568,  /* 208   D0  Ð  box drawings up double and horizontal single       */
  0x2564,  /* 209   D1  Ñ  box drawings down single and horizontal double     */
  0x2565,  /* 210   D2  Ò  box drawings down double and horizontal single     */
  0x2559,  /* 211   D3  Ó  box drawings up double and right single            */
  0x2558,  /* 212   D4  Ô  box drawings up single and right double            */
  0x2552,  /* 213   D5  Õ  box drawings down single and right double          */
  0x2553,  /* 214   D6  Ö  box drawings down double and right single          */
  0x256B,  /* 215   D7  ×  box drawings vertical double and horizontal single */
  0x256A,  /* 216   D8  Ø  box drawings vertical single and horizontal double */
  0x2518,  /* 217   D9  Ù  box drawings light up and left                     */
  0x250C,  /* 218   DA  Ú  box drawings light down and right                  */
  0x2588,  /* 219   DB  Û  full block                                         */
  0x2584,  /* 220   DC  Ü  lower half block                                   */
  0x258C,  /* 221   DD  Ý  left half block                                    */
  0x2590,  /* 222   DE  Þ  right half block                                   */
  0x2580,  /* 223   DF  ß  upper half block                                   */
  0x03B1,  /* 224   E0  à  greek small letter alpha                           */
  0x00DF,  /* 225   E1  á  latin small letter sharp s                         */
  0x0393,  /* 226   E2  â  greek capital letter gamma                         */
  0x03C0,  /* 227   E3  ã  greek small letter pi                              */
  0x03A3,  /* 228   E4  ä  greek capital letter sigma                         */
  0x03C3,  /* 229   E5  å  greek small letter sigma                           */
  0x00B5,  /* 230   E6  æ  micro sign                                         */
  0x03C4,  /* 231   E7  ç  greek small letter tau                             */
  0x03A6,  /* 232   E8  è  greek capital letter phi                           */
  0x0398,  /* 233   E9  é  greek capital letter theta                         */
  0x03A9,  /* 234   EA  ê  greek capital letter omega                         */
  0x03B4,  /* 235   EB  ë  greek small letter delta                           */
  0x221E,  /* 236   EC  ì  infinity                                           */
  0x03C6,  /* 237   ED  í  greek small letter phi                             */
  0x03B5,  /* 238   EE  î  greek small letter epsilon                         */
  0x2229,  /* 239   EF  ï  intersection                                       */
  0x2261,  /* 240   F0  ð  identical to                                       */
  0x00B1,  /* 241   F1  ñ  plus-minus sign                                    */
  0x2265,  /* 242   F2  ò  greater-than or equal to                           */
  0x2264,  /* 243   F3  ó  less-than or equal to                              */
  0x2320,  /* 244   F4  ô  top half integral                                  */
  0x2321,  /* 245   F5  õ  bottom half integral                               */
  0x00F7,  /* 246   F6  ö  division sign                                      */
  0x2248,  /* 247   F7  ÷  almost equal to                                    */
  0x00B0,  /* 248   F8  ø  degree sign                                        */
  0x2219,  /* 249   F9  ù  bullet operator                                    */
  0x00B7,  /* 250   FA  ú  middle dot                                         */
  0x221A,  /* 251   FB  û  square root                                        */
  0x207F,  /* 252   FC  ^(n)  superscript latin small letter n                */
  0x00B2,  /* 253   FD  ý  superscript two                                    */
  0x25A0,  /* 254   FE  þ  black square                                       */
  0x00A0 };/* 255   FF     no-break space                                     */

/* If the window doesn't have the focus,  or the cursor style is set to    */
/* "hollow box",  we show the cursor as a hollow box.  Other possibilities */
/* at present are a full block,  underscore,  or no cursor.                */

static void redraw_cursor( const HDC hdc)
{
    const int cursor_style = SP->visibility & 0xf;

    if( cursor_style)
    {
        RECT rect;
        extern int PDC_cxChar, PDC_cyChar;
        extern HWND PDC_hWnd;
        const int is_focussed = (PDC_hWnd == GetForegroundWindow( ));
        const int is_hollow_box =
               (!is_focussed || cursor_style == 3);

        rect.left = SP->curscol * PDC_cxChar;
        rect.right = rect.left + PDC_cxChar;
        rect.bottom = (SP->cursrow + 1) * PDC_cyChar;
        if( cursor_style == 2 || is_hollow_box)
            rect.top = rect.bottom - PDC_cyChar;
        else
            rect.top = rect.bottom - 2;
        InvertRect( hdc, &rect);
        if( is_hollow_box)
        {
            const int margin = 1;

            rect.top += margin;
            rect.bottom -= margin;
            rect.left += margin;
            rect.right -= margin;
            InvertRect( hdc, &rect);
        }
    }
}

/* position "hardware" cursor at (y, x).  We don't have a for-real hardware */
/* cursor in this version,  of course,  but we can fake it.  Note that much */
/* of the logic was borrowed from the SDL version.  In particular,  the     */
/* cursor is moved by first overwriting the "original" location.            */

void PDC_gotoyx(int row, int col)
{
    extern int PDC_blink_state;
    const int temp_visibility = SP->visibility;

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

                /* clear the old cursor... */
    SP->visibility = 0;
    PDC_transform_line( SP->cursrow, SP->curscol, 1,
                           curscr->_y[SP->cursrow] + SP->curscol);
    SP->visibility = temp_visibility;

               /* ...then draw the new (assuming it's actually visible): */
    PDC_blink_state = 1;
    if( SP->visibility & 0xf)
        if( PDC_blink_state || (SP->visibility & NON_BLINKING_CURSOR))
        {
            extern HWND PDC_hWnd;
            HDC hdc = GetDC( PDC_hWnd) ;

            SP->curscol = col;
            SP->cursrow = row;
            redraw_cursor( hdc);
            ReleaseDC( PDC_hWnd, hdc) ;
        }
}

int PDC_font_size = 12;

HFONT pdc_get_font_handle( const attr_t font_attrib)
{
    LOGFONT lf;
    const char *font_name = getenv( "PDC_FONT");

    memset(&lf, 0, sizeof(LOGFONT));        // Clear out structure.
    lf.lfHeight = -PDC_font_size;
    if( !font_name)
        font_name = "Courier New";
#ifdef PDC_WIDE
    PDC_mbstowcs( lf.lfFaceName, font_name, sizeof( lf.lfFaceName));
#else
    strcpy(lf.lfFaceName, font_name);              // Request font
#endif
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lf.lfWeight = ((font_attrib & A_BOLD) ? FW_EXTRABOLD : FW_NORMAL);
    lf.lfItalic = ((font_attrib & A_ITALIC) ? TRUE : FALSE);
    lf.lfCharSet = ANSI_CHARSET;
    lf.lfQuality = PROOF_QUALITY;
    lf.lfOutPrecision = OUT_RASTER_PRECIS;
    return( CreateFontIndirect( &lf));
}

    /* This function 'intensifies' a color by shifting it toward white. */
    /* It used to average the input color with white.  Then it did a    */
    /* weighted average:  2/3 of the input color,  1/3 white,   for a   */
    /* lower "intensification" level.                                   */
    /*    Then Mark Hessling suggested that the output level should     */
    /* remap zero to 85 (= 255 / 3, so one-third intensity),  and input */
    /* of 192 or greater should be remapped to 255 (full intensity).    */
    /* Assuming we want a linear response between zero and 192,  that   */
    /* leads to output = 85 + input * (255-85)/192.                     */
    /*    This should lead to proper handling of bold text in legacy    */
    /* apps,  where "bold" means "high intensity".                      */

static COLORREF intensified_color( COLORREF ival)
{
    int rgb, i;
    COLORREF oval = 0;

    for( i = 0; i < 3; i++, ival >>= 8)
    {
        rgb = (int)( ival & 0xff);
        if( rgb >= 192)
            rgb = 255;
        else
            rgb = 85 + rgb * (255 - 85) / 192;
        oval |= ((COLORREF)rgb << (i * 8));
    }
    return( oval);
}

/* update the given physical line to look like the corresponding line in
curscr.

   NOTE that if x > 0,  we decrement it and srcp,  and increment the
length.  In other words,  we draw the preceding character,  too.  This
is done because,  at certain font sizes,  characters break out and
overwrite the preceding character.  That results in debris left on
the screen.

   The code also now increments the length only,  drawing one more
character (i.e.,  draws the character following the "text we really
want").  Again,  this helps to avoid debris left on the screen.

   Another issue can occur with characters drawing above/below their
"proper" box.  I haven't come up with a fix for that,  except for
avoiding the few font sizes where it's an issue.  (The problem with
overwriting a little to the left of the character cell appears to be
more common,  with that of drawing a little to the right happening
sometimes as well.)                     */

static HFONT hFonts[4];

#define BUFFSIZE 50

void PDC_transform_line_given_hdc( const HDC hdc, const int lineno,
                             int x, int len, const chtype *srcp)
{
    extern int PDC_blink_state;
    extern int PDC_n_rows, PDC_n_cols;
    HFONT hOldFont = (HFONT)0;
    extern int PDC_cxChar, PDC_cyChar;
    int i, curr_color = -1;
    attr_t font_attrib = (attr_t)-1;
    int cursor_overwritten = FALSE;
    extern COLORREF *pdc_rgbs;
    short foreground_index = 0, background_index = 0;
    chtype prev_ch = 0;
    extern RECT PDC_mouse_rect;              /* see 'pdcscrn.c' */

    if( !srcp)             /* just freeing up fonts */
    {
        for( i = 0; i < 4; i++)
            if( hFonts[i])
            {
                DeleteObject( hFonts[i]);
                hFonts[i] = NULL;
            }
        return;
    }
                     /* Seems to me as if the input text to this function */
    if( x < 0)       /* should _never_ be off-screen.  But it sometimes is. */
    {                /* Clipping is therefore necessary. */
        len += x;
        srcp -= x;
        x = 0;
    }
    len++;    /* draw an extra char to avoid leaving garbage on screen */
    if( len > PDC_n_cols - x)
        len = PDC_n_cols - x;
    if( lineno >= PDC_n_rows || len <= 0 || lineno < 0)
        return;
    if( x)           /* back up by one character to avoid */
    {                /* leaving garbage on the screen */
        x--;
        len++;
        srcp--;
    }
    if( lineno == SP->cursrow && SP->curscol >= x && SP->curscol < x + len)
        if( SP->visibility & 0xf)
            if( PDC_blink_state || (SP->visibility & NON_BLINKING_CURSOR))
                cursor_overwritten = TRUE;
    while( len)
    {
        const attr_t attrib = (attr_t)( *srcp >> PDC_REAL_ATTR_SHIFT);
        const int color = (int)(( *srcp & A_COLOR) >> PDC_COLOR_SHIFT);
        attr_t new_font_attrib = (*srcp & (A_BOLD | A_ITALIC));
        RECT clip_rect;
#ifdef NO_WIDE_CHARS
        char buff[BUFFSIZE];
#else
        wchar_t buff[BUFFSIZE];
#endif

        for( i = 0; i < len && i < BUFFSIZE &&
                    attrib == (attr_t)( srcp[i] >> PDC_REAL_ATTR_SHIFT); i++)
        {
            chtype ch = srcp[i];

#ifdef CHTYPE_LONG
            if (ch & A_ALTCHARSET && !(ch & 0xff80))
                ch = acs_map[ch & 0x7f];
            else if( (ch & 0xff80) == 0x80)
               ch = extended_ascii_to_unicode[ch & 0x7f];
            else if( (ch & 0xffff) < 32)
               ch = starting_ascii_to_unicode[ch & 0x1f];
#endif
#ifdef NO_WIDE_CHARS
            buff[i] = (char)( ch & A_CHARTEXT);
#else
            buff[i] = (wchar_t)( ch & A_CHARTEXT);
#endif
        }
        if( color != curr_color || ((prev_ch ^ *srcp) & (A_REVERSE | A_BLINK)))
        {
            extern int PDC_really_blinking;          /* see 'pdcsetsc.c' */
            int reverse_colors = ((*srcp & A_REVERSE) ? 1 : 0);
            COLORREF fore, back;
            int intensify_backgnd = 0;

            PDC_pair_content( (short)color, &foreground_index, &background_index);
            if( *srcp & A_BLINK)
            {
                if( !PDC_really_blinking)   /* convert 'blinking' to 'bold' */
                {
                    new_font_attrib &= ~A_BLINK;
                    intensify_backgnd = 1;
                }
                else if( PDC_blink_state)
                    reverse_colors ^= 1;
            }
            if( reverse_colors)
            {
                const short temp = foreground_index;

                foreground_index = background_index;
                background_index = temp;
            }

            fore = pdc_rgbs[foreground_index];
            if( *srcp & A_BOLD)
                fore = intensified_color( fore);
            SetTextColor( hdc, fore);

            back = pdc_rgbs[background_index];
            if( intensify_backgnd)
                back = intensified_color( back);
            SetBkColor( hdc, back);
        }
        if( new_font_attrib != font_attrib)
        {
            HFONT hFont;
            int idx = 0;

            font_attrib = new_font_attrib;
            if( font_attrib & A_BOLD)
                idx |= 1;
            if( font_attrib & A_ITALIC)
                idx |= 2;
            if( !hFonts[idx])
                hFonts[idx] = pdc_get_font_handle( font_attrib);
            hFont = SelectObject( hdc, hFonts[idx]);
            if( !hOldFont)
                hOldFont = hFont;
        }
        prev_ch = *srcp;
        clip_rect.left = x * PDC_cxChar;
        clip_rect.top = lineno * PDC_cyChar;
        clip_rect.right = clip_rect.left + i * PDC_cxChar;
        clip_rect.bottom = clip_rect.top + PDC_cyChar;
        clip_rect.bottom += PDC_cyChar;
#ifdef NO_WIDE_CHARS
        TextOutA( hdc, x * PDC_cxChar, lineno * PDC_cyChar, buff, i);
#else
        TextOutW( hdc, x * PDC_cxChar, lineno * PDC_cyChar, buff, i);
#endif
//      ExtTextOutW( hdc, clip_rect.left, clip_rect.top, 0, NULL, buff, i, NULL);
#ifdef A_OVERLINE
        if( *srcp & (A_UNDERLINE | A_RIGHTLINE | A_LEFTLINE | A_OVERLINE | A_STRIKEOUT))
#else
        if( *srcp & (A_UNDERLINE | A_RIGHTLINE | A_LEFTLINE))
#endif
        {
            const int y1 = lineno * PDC_cyChar;
            const int y2 = y1 + PDC_cyChar - 1;
            const int x1 = x * PDC_cxChar;
            const int x2 = x1 + i * PDC_cxChar;
            int j;
            const int color_idx = (SP->line_color == -1 ?
                                    foreground_index : SP->line_color);
            const HPEN pen = CreatePen( PS_SOLID, 1, pdc_rgbs[color_idx]);
            const HPEN old_pen = SelectObject( hdc, pen);

            if( *srcp & A_UNDERLINE)
            {
                MoveToEx( hdc, x1, y2, NULL);
                LineTo(   hdc, x2, y2);
            }
#ifdef A_OVERLINE
            if( *srcp & A_OVERLINE)
            {
                MoveToEx( hdc, x1, y1, NULL);
                LineTo(   hdc, x2, y1);
            }
            if( *srcp & A_STRIKEOUT)
            {
                MoveToEx( hdc, x1, (y1 + y2) / 2, NULL);
                LineTo(   hdc, x2, (y1 + y2) / 2);
            }
#endif
            if( *srcp & A_RIGHTLINE)
                for( j = 0; j < i; j++)
                {
                    MoveToEx( hdc, x2 - j * PDC_cxChar - 1, y1, NULL);
                    LineTo(   hdc, x2 - j * PDC_cxChar - 1, y2);
                }
            if( *srcp & A_LEFTLINE)
                for( j = 0; j < i; j++)
                {
                    MoveToEx( hdc, x1 + j * PDC_cxChar, y1, NULL);
                    LineTo(   hdc, x1 + j * PDC_cxChar, y2);
                }
            SelectObject( hdc, old_pen);
        }
        if( (lineno - PDC_mouse_rect.top) * (lineno - PDC_mouse_rect.bottom) <= 0)
        {
            const int min_x = min( PDC_mouse_rect.left, PDC_mouse_rect.right);
            const int max_x = max( PDC_mouse_rect.left, PDC_mouse_rect.right) + 1;

            if( x <= max_x && x + i >= min_x)
            {
                RECT rect;

                rect.top = lineno * PDC_cyChar;
                rect.bottom = rect.top + PDC_cyChar;
                rect.right = max( x, min_x);
                rect.left = min( x + i, max_x);
                rect.right *= PDC_cxChar;
                rect.left *= PDC_cxChar;
                InvertRect( hdc, &rect);
            }
        }
        len -= i;
        x += i;
        srcp += i;
    }
    SelectObject( hdc, hOldFont);
               /* ...did we step on the cursor?  If so,  redraw it: */
    if( cursor_overwritten)
        redraw_cursor( hdc);
}

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    if( !srcp)    /* just freeing up fonts */
        PDC_transform_line_given_hdc( 0, 0, 0, 0, NULL);
    else
    {
        extern HWND PDC_hWnd;
        const HDC hdc = GetDC( PDC_hWnd) ;

        PDC_transform_line_given_hdc( hdc, lineno, x, len, srcp);
        ReleaseDC( PDC_hWnd, hdc);
    }
}

