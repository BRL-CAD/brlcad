/* Public Domain Curses */

/* $Id$ */

#ifdef PDC_WIDE
# define UNICODE
#endif

#include <windows.h>
#undef MOUSE_MOVED
#include <curspriv.h>

extern int PDC_get_buffer_rows(void);

/* The following probably ought to go into 'curses.h' at some point. */
/* Or at least,  not be here;  this is a temporary place for them.   */

/* curs_set() can take all eight of the following constants... at least, */
/* it can in the Win32a flavor.  Others take just the first three.      */
/* Blinking is handled as flag 0x10,  resulting in the odd fact that    */
/* one can have a "blinking invisible" cursor.  (Which in practice,     */
/* looks just like the non-blinking invisible cursor.)                  */

#define PDC_CURSOR_INVISIBLE_BLINKING           0x00
#define PDC_CURSOR_NORMAL_BLINKING              0x01
#define PDC_CURSOR_INTENSE_BLINKING             0x02
#define PDC_CURSOR_OUTLINE_BOX_BLINKING         0x03
#define PDC_CURSOR_INVISIBLE_NOT_BLINKING       0x10
#define PDC_CURSOR_NORMAL_NOT_BLINKING          0x11
#define PDC_CURSOR_INTENSE_NOT_BLINKING         0x12
#define PDC_CURSOR_OUTLINE_BOX_NOT_BLINKING     0x13


/* NOTE that in 'curses.h',  PDC_ATTR_SHIFT is #defined to be 19,  on the
theory that we won't actually have left/right lining.  (And in fact,
PDC_ATTR_SHIFT is used only in the DOS,  OS/2,  and 'real' Win32 versions,
none of which support left/right lining... hence,  no bug was ever noticed
ere now.)  Since this version _does_ support left/right lining,  we oughta
be using 17.  Hence:   */

#ifdef CHTYPE_LONG
    #define PDC_REAL_ATTR_SHIFT 17
#else
    #define PDC_REAL_ATTR_SHIFT PDC_ATTR_SHIFT
#endif

/* The wrong value for PDC_ATTR_SHIFT is embedded deeply in the logic of the
other three variants,  so we're probably stuck with the above distinction. */



/* The following block of #defines is completely unused for the
nonce. But I've proposed that since we're using 32 alternate
character set characters thus far,  there is the possibility of
having 96 more. Following are some possibilities,  both in Unicode
and ANSI form. They should work in all flavors of PDCurses,  not
just Win32a. Suitable #defines such as ACS_DBL_ULCORNER and such
could then be added to 'curses.h'.   */

#ifdef CHTYPE_LONG
   #define UPPER_HALF_BLOCK    0x2580
   #define LOWER_HALF_BLOCK    0x2584
   #define LEFT_HALF_BLOCK     0x258c
   #define RIGHT_HALF_BLOCK    0x2590

   #define BOX_DBL_HORIZONTAL  0x2550
   #define BOX_DBL_VERTICAL    0x2551
   #define BOX_DBL_DOWN_RIGHT  0x2554
   #define BOX_DBL_DOWN_LEFT   0x2557
   #define BOX_DBL_UP_RIGHT    0x255A
   #define BOX_DBL_UP_LEFT     0x255D
   #define BOX_DBL_TEE_RIGHT   0x2560
   #define BOX_DBL_TEE_LEFT    0x2563
   #define BOX_DBL_TEE_DOWN    0x2566
   #define BOX_DBL_TEE_UP      0x2569
   #define BOX_DBL_CROSS       0x256C

   #define SQUARE_ROOT         0x221A

   #define VULGAR_QUARTER         0xBC
   #define VULGAR_HALF            0xBD
#else
   #define UPPER_HALF_BLOCK    0xdf
   #define LOWER_HALF_BLOCK    0xdc
   #define LEFT_HALF_BLOCK     0xdd
   #define RIGHT_HALF_BLOCK    0xde

   #define BOX_DBL_HORIZONTAL  0xcd
   #define BOX_DBL_VERTICAL    0xba
   #define BOX_DBL_DOWN_RIGHT  0xc9
   #define BOX_DBL_DOWN_LEFT   0xbb
   #define BOX_DBL_UP_RIGHT    0xc8
   #define BOX_DBL_UP_LEFT     0xbc
   #define BOX_DBL_TEE_RIGHT   0xcc
   #define BOX_DBL_TEE_LEFT    0xb9
   #define BOX_DBL_TEE_DOWN    0xcb
   #define BOX_DBL_TEE_UP      0xca
   #define BOX_DBL_CROSS       0xce

   #define SQUARE_ROOT         0xfb

   #define VULGAR_QUARTER         0xac
   #define VULGAR_HALF            0xab
#endif

      /* It says at http://unicode.org/charts/PDF/U2300.pdf */
      /* that '...the scan line numbers here refer to old,  */
      /* low-resolution technology for terminals, with only */
      /* nine scan lines per fixed-size character glyph.    */
      /* Even-numbered scan lines are unified with box      */
      /* drawing graphics."                                 */
      /*  The utility of these is questionable;  they'd     */
      /* work Just Fine in win32a (_if_ the appropriate     */
      /* glyphs are available),  but not elsewhere.         */
#define UNICODE_HORIZ_SCAN_LINE_1         0x23ba
#define UNICODE_HORIZ_SCAN_LINE_3         0x23bb
#define UNICODE_HORIZ_SCAN_LINE_7         0x23bc
#define UNICODE_HORIZ_SCAN_LINE_9         0x23bd


/* These keys are currently undetected by PDCurses,  even in this flavor,
because they have values past KEY_MAX.  If KEY_MAX were to be redefined
(from 0x224 to 0x240),  they'd suddenly be detected... at least in Win32a.
Also,  even though they aren't returned by getch( ) (anything above KEY_MAX
gets filtered out),  they _can_ be used as hotkeys for shutdown,  pasting,
and smaller/larger fonts in PDC_set_function_key( ).    */

#define CTL_SEMICOLON   0x225
#define CTL_EQUAL       0x226
#define CTL_COMMA       0x227
#define CTL_MINUS       0x228
#define CTL_STOP        0x229
#define CTL_FSLASH      0x22a
#define CTL_BQUOTE      0x22b

#define KEY_APPS        0x22c
#define KEY_SAPPS       0x22d
#define CTL_APPS        0x22e
#define ALT_APPS        0x22f

#define KEY_PAUSE       0x230
#define KEY_SPAUSE      0x231
#define CTL_PAUSE       0x232

#define KEY_PRINTSCREEN 0x233
#define ALT_PRINTSCREEN 0x234
#define KEY_SCROLLLOCK  0x235
#define ALT_SCROLLLOCK  0x236

#define CTL_0           0x237
#define CTL_1           0x238
#define CTL_2           0x239
#define CTL_3           0x23a
#define CTL_4           0x23b
#define CTL_5           0x23c
#define CTL_6           0x23d
#define CTL_7           0x23e
#define CTL_8           0x23f
#define CTL_9           0x240

int PDC_set_function_key( const unsigned function, const int new_key);

/* The above function allows one to set a 'shut down' key,  and reassign
hotkeys used for pasting from the clipboard and enlarging and decreasing
the font size.  For example,  calling

PDC_set_function_key( FUNCTION_KEY_SHUT_DOWN, ALT_Q);

would reset the library so that,  if the user clicks on the 'close' box,
Alt-Q would be added to the key queue.  This would give the app the
opportunity to shut things down (and perhaps ask "are you sure",  and/or
"save changes or discard or cancel"),  rather than just having the
window close (the default behavior).

   Also,  by default,  Ctrl-V "pastes" the clipboard into the key queue;
Ctrl-L enlarges the font;  Ctrl-D decreases the font size.  But one could
call,  for example,

PDC_set_function_key( FUNCTION_KEY_PASTE, CTL_Z);

   to reset the "paste" key to be Ctrl-Z.  Or one could call

PDC_set_function_key( FUNCTION_KEY_PASTE, 0);

   to shut off that function.  (It would still be accessible from the menu.)

   Thus far,  this is a Win32a-flavor specific function.  But it could,  and
in my opinion should,  be made available in the SDL and XCurses flavors too.

   The return value is the key previously used for that function.
*/

#define N_FUNCTION_KEYS 4
#define FUNCTION_KEY_SHUT_DOWN        0
#define FUNCTION_KEY_PASTE            1
#define FUNCTION_KEY_ENLARGE_FONT     2
#define FUNCTION_KEY_SHRINK_FONT      3
