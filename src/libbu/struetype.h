// struetype.h - based on stb_truetype.h v1.26 - public domain
// authored from 2009-2021 by Sean Barrett / RAD Game Tools
//
// ==========================================================================
// NOTE:  Unlike the upstream stb_truetype.h, this library includes bounds
// checking of buffer accesses to prevent memory safety vulnerabilities when
// processing untrusted font files.  This comes at the cost of API
// compatibility - a buffer size parameter is now required.
// ==========================================================================
//
//   This library processes TrueType files:
//        parse files
//        extract glyph metrics
//        extract glyph shapes
//        render glyphs to one-channel bitmaps with antialiasing (box filter)
//        render glyphs to one-channel SDF bitmaps (signed-distance field/function)
//
//   Todo:
//        non-MS cmaps
//        expand malformed-font fuzzing across additional formats
//        hinting? (no longer patented)
//        cleartype-style AA?
//        optimize: use simple memory allocator for intermediates
//        optimize: build edge-list directly from curves
//        optimize: rasterize directly from curves?
//
// ADDITIONAL CONTRIBUTORS
//
//   Mikko Mononen: compound shape support, more cmap formats
//   Tor Andersson: kerning, subpixel rendering
//   Dougall Johnson: OpenType / Type 2 font handling
//   Daniel Ribeiro Maciel: basic GPOS-based kerning
//
//   Misc other:
//       Ryan Gordon
//       Simon Glass
//       github:IntellectualKitty
//       Imanol Celaya
//       Daniel Ribeiro Maciel
//
//   Bug/warning reports/fixes:
//       "Zer" on mollyrocket       Fabian "ryg" Giesen   github:NiLuJe
//       Cass Everitt               Martins Mozeiko       github:aloucks
//       stoiko (Haemimont Games)   Cap Petschulat        github:oyvindjam
//       Brian Hook                 Omar Cornut           github:vassvik
//       Walter van Niftrik         Ryan Griege
//       David Gow                  Peter LaValle
//       David Given                Sergey Popov
//       Ivan-Assen Ivanov          Giumo X. Clanjor
//       Anthony Pesch              Higor Euripedes
//       Johan Duparc               Thomas Fields
//       Hou Qiming                 Derek Vinyard
//       Rob Loach                  Cort Stratton
//       Kenney Phillis Jr.         Brian Costabile
//       Ken Voskuil (kaesve)       Yakov Galka
//
// VERSION HISTORY
//
//   struetype (2026-08-03) comprehensive bounds, overflow, allocation,
//                         rasterizer, CFF, collection, and atlas hardening
//                         with size-aware raw-buffer APIs
//   1.26 (2021-08-28) fix broken rasterizer
//   1.25 (2021-07-11) many fixes
//   1.24 (2020-02-05) fix warning
//   1.23 (2020-02-02) query SVG data for glyphs; query whole kerning table (but only kern not GPOS)
//   1.22 (2019-08-11) minimize missing-glyph duplication; fix kerning if both 'GPOS' and 'kern' are defined
//   1.21 (2019-02-25) fix warning
//   1.20 (2019-02-07) PackFontRange skips missing codepoints; GetScaleFontVMetrics()
//   1.19 (2018-02-11) GPOS kerning, STBTT_fmod
//   1.18 (2018-01-29) add missing function
//   1.17 (2017-07-23) make more arguments const; doc fix
//   1.16 (2017-07-12) SDF support
//   1.15 (2017-03-03) make more arguments const
//   1.14 (2017-01-16) num-fonts-in-TTC function
//   1.13 (2017-01-02) support OpenType fonts, certain Apple fonts
//   1.12 (2016-10-25) suppress warnings about casting away const with -Wcast-qual
//   1.11 (2016-04-02) fix unused-variable warning
//   1.10 (2016-04-02) user-defined fabs(); rare memory leak; remove duplicate typedef
//   1.09 (2016-01-16) warning fix; avoid crash on outofmem; use allocation userdata properly
//   1.08 (2015-09-13) document stbtt_Rasterize(); fixes for vertical & horizontal edges
//   1.07 (2015-08-01) allow PackFontRanges to accept arrays of sparse codepoints;
//                     variant PackFontRanges to pack and render in separate phases;
//                     fix stbtt_GetFontOFfsetForIndex (never worked for non-0 input?);
//                     fixed an assert() bug in the new rasterizer
//                     replace assert() with STBTT_assert() in new rasterizer
//
//   Full history can be found at the end of this file.
//
// LICENSE
//
//   See end of file for license information.
//
// USAGE
//
//   Include this file in whatever places need to refer to it. In ONE C/C++
//   file, write:
//      #define STRUETYPE_IMPLEMENTATION
//   before the #include of this file. This expands out the actual
//   implementation into that C/C++ file.
//
//   To make the implementation private to the file that generates the implementation,
//      #define STT_STATIC
//
//   Simple 3D API (don't ship this, but it's fine for tools and quick start)
//           stt_BakeFontBitmap()               -- bake a font to a bitmap for use as texture
//           stt_GetBakedQuad()                 -- compute quad to draw for a given char
//
//   Improved 3D API (more shippable):
//           #include "stb_rect_pack.h"           -- optional, but you really want it
//           stt_PackBegin()
//           stt_PackSetOversampling()          -- for improved quality on small fonts
//           stt_PackFontRanges()               -- pack and renders
//           stt_PackEnd()
//           stt_GetPackedQuad()
//
//   "Load" a font file from a memory buffer (you have to keep the buffer loaded)
//           stt_InitFont()
//           stt_GetFontOffsetForIndex()        -- indexing for TTC font collections
//           stt_GetNumberOfFonts()             -- number of fonts for TTC font collections
//
//   Render a unicode codepoint to a bitmap
//           stt_GetCodepointBitmap()           -- allocates and returns a bitmap
//           stt_MakeCodepointBitmap()          -- renders into bitmap you provide
//           stt_GetCodepointBitmapBox()        -- how big the bitmap must be
//
//   Character advance/positioning
//           stt_GetCodepointHMetrics()
//           stt_GetFontVMetrics()
//           stt_GetFontVMetricsOS2()
//           stt_GetCodepointKernAdvance()
//
//   Starting with version 1.06, the rasterizer was replaced with a new,
//   faster and generally-more-precise rasterizer. The new rasterizer more
//   accurately measures pixel coverage for anti-aliasing, except in the case
//   where multiple shapes overlap, in which case it overestimates the AA pixel
//   coverage. Thus, anti-aliasing of intersecting shapes may look wrong. If
//   this turns out to be a problem, you can re-enable the old rasterizer with
//        #define STT_RASTERIZER_VERSION 1
//   which will incur about a 15% speed hit.
//
// ADDITIONAL DOCUMENTATION
//
//   Immediately after this block comment are a series of sample programs.
//
//   After the sample programs is the "header file" section. This section
//   includes documentation for each API function.
//
//   Some important concepts to understand to use this library:
//
//      Codepoint
//         Characters are defined by unicode codepoints, e.g. 65 is
//         uppercase A, 231 is lowercase c with a cedilla, 0x7e30 is
//         the hiragana for "ma".
//
//      Glyph
//         A visual character shape (every codepoint is rendered as
//         some glyph)
//
//      Glyph index
//         A font-specific integer ID representing a glyph
//
//      Baseline
//         Glyph shapes are defined relative to a baseline, which is the
//         bottom of uppercase characters. Characters extend both above
//         and below the baseline.
//
//      Current Point
//         As you draw text to the screen, you keep track of a "current point"
//         which is the origin of each character. The current point's vertical
//         position is the baseline. Even "baked fonts" use this model.
//
//      Vertical Font Metrics
//         The vertical qualities of the font, used to vertically position
//         and space the characters. See docs for stt_GetFontVMetrics.
//
//      Font Size in Pixels or Points
//         The preferred interface for specifying font sizes in stb_truetype
//         is to specify how tall the font's vertical extent should be in pixels.
//         If that sounds good enough, skip the next paragraph.
//
//         Most font APIs instead use "points", which are a common typographic
//         measurement for describing font size, defined as 72 points per inch.
//         stb_truetype provides a point API for compatibility. However, true
//         "per inch" conventions don't make much sense on computer displays
//         since different monitors have different number of pixels per
//         inch. For example, Windows traditionally uses a convention that
//         there are 96 pixels per inch, thus making 'inch' measurements have
//         nothing to do with inches, and thus effectively defining a point to
//         be 1.333 pixels. Additionally, the TrueType font data provides
//         an explicit scale factor to scale a given font's glyphs to points,
//         but the author has observed that this scale factor is often wrong
//         for non-commercial fonts, thus making fonts scaled in points
//         according to the TrueType spec incoherently sized in practice.
//
// DETAILED USAGE:
//
//  Scale:
//    Select how high you want the font to be, in points or pixels.
//    Call ScaleForPixelHeight or ScaleForMappingEmToPixels to compute
//    a scale factor SF that will be used by all other functions.
//
//  Baseline:
//    You need to select a y-coordinate that is the baseline of where
//    your text will appear. Call GetFontBoundingBox to get the baseline-relative
//    bounding box for all characters. SF*-y0 will be the distance in pixels
//    that the worst-case character could extend above the baseline, so if
//    you want the top edge of characters to appear at the top of the
//    screen where y=0, then you would set the baseline to SF*-y0.
//
//  Current point:
//    Set the current point where the first character will appear. The
//    first character could extend left of the current point; this is font
//    dependent. You can either choose a current point that is the leftmost
//    point and hope, or add some padding, or check the bounding box or
//    left-side-bearing of the first character to be displayed and set
//    the current point based on that.
//
//  Displaying a character:
//    Compute the bounding box of the character. It will contain signed values
//    relative to <current_point, baseline>. I.e. if it returns x0,y0,x1,y1,
//    then the character should be displayed in the rectangle from
//    <current_point+SF*x0, baseline+SF*y0> to <current_point+SF*x1,baseline+SF*y1).
//
//  Advancing for the next character:
//    Call GlyphHMetrics, and compute 'current_point += SF * advance'.
//
//
// ADVANCED USAGE
//
//   Quality:
//
//    - Use the functions with Subpixel at the end to allow your characters
//      to have subpixel positioning. Since the font is anti-aliased, not
//      hinted, this is very import for quality. (This is not possible with
//      baked fonts.)
//
//    - Kerning is now supported, and if you're supporting subpixel rendering
//      then kerning is worth using to give your text a polished look.
//
//   Performance:
//
//    - Convert Unicode codepoints to glyph indexes and operate on the glyphs;
//      if you don't do this, stb_truetype is forced to do the conversion on
//      every call.
//
//    - There are a lot of memory allocations. We should modify it to take
//      a temp buffer and allocate from the temp buffer (without freeing),
//      should help performance a lot.
//
// NOTES
//
//   The system uses the raw data found in the .ttf file without changing it
//   and without building auxiliary data structures. This is a bit inefficient
//   on little-endian systems (the data is big-endian), but assuming you're
//   caching the bitmaps or glyph shapes this shouldn't be a big deal.
//
//   It appears to be very hard to programmatically determine what font a
//   given file is in a general way. I provide an API for this, but I don't
//   recommend it.
//
//
// PERFORMANCE MEASUREMENTS FOR 1.06:
//
//                      32-bit     64-bit
//   Previous release:  8.83 s     7.68 s
//   Pool allocations:  7.72 s     6.34 s
//   Inline sort     :  6.54 s     5.65 s
//   New rasterizer  :  5.63 s     5.00 s

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
////
////  SAMPLE PROGRAMS
////
//
//  Incomplete text-in-3d-api example, which draws quads properly aligned to be lossless.
//  See "tests/truetype_demo_win32.c" for a complete version.
#if 0
#define STRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "struetype.h"

unsigned char ttf_buffer[1<<20];
unsigned char temp_bitmap[512*512];

stt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
GLuint ftex;

void my_stt_initfont(void)
{
   fread(ttf_buffer, 1, 1<<20, fopen("c:/windows/fonts/times.ttf", "rb"));
   stt_BakeFontBitmap(ttf_buffer, 1<<20, 0, 32.0, temp_bitmap,512,512, 32,96, cdata); // no guarantee this fits!
   // can free ttf_buffer at this point
   glGenTextures(1, &ftex);
   glBindTexture(GL_TEXTURE_2D, ftex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512,512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
   // can free temp_bitmap at this point
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void my_stt_print(float x, float y, char *text)
{
   // assume orthographic projection with units = screen pixels, origin at top left
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, ftex);
   glBegin(GL_QUADS);
   while (*text) {
      if (*text >= 32 && *text < 128) {
         stt_aligned_quad q;
         stt_GetBakedQuad(cdata, 512,512, *text-32, &x,&y,&q,1);//1=opengl & d3d10+,0=d3d9
         glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,q.y0);
         glTexCoord2f(q.s1,q.t0); glVertex2f(q.x1,q.y0);
         glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,q.y1);
         glTexCoord2f(q.s0,q.t1); glVertex2f(q.x0,q.y1);
      }
      ++text;
   }
   glEnd();
}
#endif
//
//
//////////////////////////////////////////////////////////////////////////////
//
// Complete program (this compiles): get a single bitmap, print as ASCII art
//
#if 0
#include <stdio.h>
#define STRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "struetype.h"

char ttf_buffer[1<<25];

int main(int argc, char **argv)
{
   stt_fontinfo font;
   unsigned char *bitmap;
   int w,h,i,j,c = (argc > 1 ? atoi(argv[1]) : 'a'), s = (argc > 2 ? atoi(argv[2]) : 20);

   fread(ttf_buffer, 1, 1<<25, fopen(argc > 3 ? argv[3] : "c:/windows/fonts/arialbd.ttf", "rb"));

   stt_InitFont(&font, ttf_buffer, 1<<25,
                stt_GetFontOffsetForIndex(ttf_buffer, 1<<25, 0));
   bitmap = stt_GetCodepointBitmap(&font, 0,stt_ScaleForPixelHeight(&font, s), c, &w, &h, 0,0);

   for (j=0; j < h; ++j) {
      for (i=0; i < w; ++i)
         putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
      putchar('\n');
   }
   return 0;
}
#endif
//
// Output:
//
//     .ii.
//    @@@@@@.
//   V@Mio@@o
//   :i.  V@V
//     :oM@@M
//   :@@@MM@M
//   @@o  o@M
//  :@@.  M@M
//   @@@o@@@@
//   :M@@V:@@.
//
//////////////////////////////////////////////////////////////////////////////
//
// Complete program: print "Hello World!" banner, with bugs
//
#if 0
char buffer[24<<20];
unsigned char screen[20][79];

int main(int arg, char **argv)
{
   stt_fontinfo font;
   int i,j,ascent,baseline,ch=0;
   float scale, xpos=2; // leave a little padding in case the character extends left
   char *text = "Heljo World!"; // intentionally misspelled to show 'lj' brokenness

   fread(buffer, 1, 1000000, fopen("c:/windows/fonts/arialbd.ttf", "rb"));
   stt_InitFont(&font, buffer, 1000000, 0);

   scale = stt_ScaleForPixelHeight(&font, 15);
   stt_GetFontVMetrics(&font, &ascent,0,0);
   baseline = (int) (ascent*scale);

   while (text[ch]) {
      int advance,lsb,x0,y0,x1,y1;
      float x_shift = xpos - (float) floor(xpos);
      stt_GetCodepointHMetrics(&font, text[ch], &advance, &lsb);
      stt_GetCodepointBitmapBoxSubpixel(&font, text[ch], scale,scale,x_shift,0, &x0,&y0,&x1,&y1);
      stt_MakeCodepointBitmapSubpixel(&font, &screen[baseline + y0][(int) xpos + x0], x1-x0,y1-y0, 79, scale,scale,x_shift,0, text[ch]);
      // note that this stomps the old data, so where character boxes overlap (e.g. 'lj') it's wrong
      // because this API is really for baking character bitmaps into textures. if you want to render
      // a sequence of characters, you really need to render each bitmap to a temp buffer, then
      // "alpha blend" that into the working buffer
      xpos += (advance * scale);
      if (text[ch+1])
         xpos += scale*stt_GetCodepointKernAdvance(&font, text[ch],text[ch+1]);
      ++ch;
   }

   for (j=0; j < 20; ++j) {
      for (i=0; i < 78; ++i)
         putchar(" .:ioVM@"[screen[j][i]>>5]);
      putchar('\n');
   }

   return 0;
}
#endif


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
////
////   INTEGRATION WITH YOUR CODEBASE
////
////   The following sections allow you to supply alternate definitions
////   of C library functions used by struetype, e.g. if you don't
////   link with the C runtime library.

#ifdef STRUETYPE_IMPLEMENTATION
   // #define your own (u)stt_int8/16/32 before including to override this
   #ifndef stt_uint8
   typedef unsigned char   stt_uint8;
   typedef signed   char   stt_int8;
   typedef unsigned short  stt_uint16;
   typedef signed   short  stt_int16;
   typedef unsigned int    stt_uint32;
   typedef signed   int    stt_int32;
   #endif

   typedef char stt__check_size32[sizeof(stt_int32)==4 ? 1 : -1];
   typedef char stt__check_size16[sizeof(stt_int16)==2 ? 1 : -1];

   // e.g. #define your own STT_ifloor/STT_iceil() to avoid math.h
   #ifndef STT_ifloor
   #include <math.h>
   #define STT_ifloor(x)   ((int) floor(x))
   #define STT_iceil(x)    ((int) ceil(x))
   #endif

   #ifndef STT_sqrt
   #include <math.h>
   #define STT_sqrt(x)      sqrt(x)
   #define STT_pow(x,y)     pow(x,y)
   #endif

   #ifndef STT_fmod
   #include <math.h>
   #define STT_fmod(x,y)    fmod(x,y)
   #endif

   #ifndef STT_cos
   #include <math.h>
   #define STT_cos(x)       cos(x)
   #define STT_acos(x)      acos(x)
   #endif

   #ifndef STT_fabs
   #include <math.h>
   #define STT_fabs(x)      fabs(x)
   #endif

   // #define your own functions "STT_malloc" / "STT_free" to avoid malloc.h
   #ifndef STT_malloc
   #include <stdlib.h>
   #define STT_malloc(x,u)  ((void)(u),malloc(x))
   #define STT_free(x,u)    ((void)(u),free(x))
   #endif

   #ifndef STT_assert
   #include <assert.h>
   #define STT_assert(x)    assert(x)
   #endif

   #ifndef STT_strlen
   #include <string.h>
   #define STT_strlen(x)    strlen(x)
   #endif

   #ifndef STT_memcpy
   #include <string.h>
   #define STT_memcpy       memcpy
   #define STT_memset       memset
   #endif
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifndef __INCLUDE_STRUETYPE_H__
#define __INCLUDE_STRUETYPE_H__

#ifdef STT_STATIC
#define STT_DEF static
#else
#define STT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// private structure
typedef struct
{
   unsigned char *data;
   int cursor;
   int size;
} stt__buf;

//////////////////////////////////////////////////////////////////////////////
//
// TEXTURE BAKING API
//
// If you use this API, you only have to call two functions ever.
//

typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
} stt_bakedchar;

STT_DEF int stt_BakeFontBitmap(const unsigned char *data, int data_size, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                stt_bakedchar *chardata);             // you allocate this, it's num_chars long
// if return is positive, the first unused row of the bitmap
// if return is negative, returns the negative of the number of characters that fit
// if return is 0, no characters fit and no rows were used
// This uses a very crappy packing.

typedef struct
{
   float x0,y0,s0,t0; // top-left
   float x1,y1,s1,t1; // bottom-right
} stt_aligned_quad;

STT_DEF void stt_GetBakedQuad(const stt_bakedchar *chardata, int pw, int ph,  // same data as above
                               int char_index,             // character to display
                               float *xpos, float *ypos,   // pointers to current position in screen pixel space
                               stt_aligned_quad *q,      // output: quad to draw
                               int opengl_fillrule);       // true if opengl fill rule; false if DX9 or earlier
// Call GetBakedQuad with char_index = 'character - first_char', and it
// creates the quad you need to draw and advances the current position.
//
// The coordinate system used assumes y increases downwards.
//
// Characters will extend both above and below the current position;
// see discussion of "BASELINE" above.
//
// It's inefficient; you might want to c&p it and optimize it.

STT_DEF void stt_GetScaledFontVMetrics(const unsigned char *fontdata, int fontdata_size, int index, float size, float *ascent, float *descent, float *lineGap);
// Query the font vertical metrics without having to create a font first.


//////////////////////////////////////////////////////////////////////////////
//
// NEW TEXTURE BAKING API
//
// This provides options for packing multiple fonts into one atlas, not
// perfectly but better than nothing.

typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
   float xoff2,yoff2;
} stt_packedchar;

typedef struct stt_pack_context stt_pack_context;
typedef struct stt_fontinfo stt_fontinfo;
#ifndef STB_RECT_PACK_VERSION
typedef struct stbrp_rect stbrp_rect;
#endif

STT_DEF int  stt_PackBegin(stt_pack_context *spc, unsigned char *pixels, int width, int height, int stride_in_bytes, int padding, void *alloc_context);
// Initializes a packing context stored in the passed-in stt_pack_context.
// Future calls using this context will pack characters into the bitmap passed
// in here: a 1-channel bitmap that is width * height. stride_in_bytes is
// the distance from one row to the next (or 0 to mean they are packed tightly
// together). "padding" is the amount of padding to leave between each
// character (normally you want '1' for bitmaps you'll use as textures with
// bilinear filtering).
//
// Returns 0 on failure, 1 on success.

STT_DEF void stt_PackEnd  (stt_pack_context *spc);
// Cleans up the packing context and frees all memory.

#define STT_POINT_SIZE(x)   (-(x))

STT_DEF int  stt_PackFontRange(stt_pack_context *spc, const unsigned char *fontdata, int fontdata_size, int font_index, float font_size,
                                int first_unicode_char_in_range, int num_chars_in_range, stt_packedchar *chardata_for_range);
// Creates character bitmaps from the font_index'th font found in fontdata (use
// font_index=0 if you don't know what that is). It creates num_chars_in_range
// bitmaps for characters with unicode values starting at first_unicode_char_in_range
// and increasing. Data for how to render them is stored in chardata_for_range;
// pass these to stt_GetPackedQuad to get back renderable quads.
//
// font_size is the full height of the character from ascender to descender,
// as computed by stt_ScaleForPixelHeight. To use a point size as computed
// by stt_ScaleForMappingEmToPixels, wrap the point size in STT_POINT_SIZE()
// and pass that result as 'font_size':
//       ...,                  20 , ... // font max minus min y is 20 pixels tall
//       ..., STT_POINT_SIZE(20), ... // 'M' is 20 pixels tall

typedef struct
{
   float font_size;
   int first_unicode_codepoint_in_range;  // if non-zero, then the chars are continuous, and this is the first codepoint
   int *array_of_unicode_codepoints;       // if non-zero, then this is an array of unicode codepoints
   int num_chars;
   stt_packedchar *chardata_for_range; // output
   unsigned char h_oversample, v_oversample; // don't set these, they're used internally
} stt_pack_range;

STT_DEF int  stt_PackFontRanges(stt_pack_context *spc, const unsigned char *fontdata, int fontdata_size, int font_index, stt_pack_range *ranges, int num_ranges);
// Creates character bitmaps from multiple ranges of characters stored in
// ranges. This will usually create a better-packed bitmap than multiple
// calls to stt_PackFontRange. Note that you can call this multiple
// times within a single PackBegin/PackEnd.

STT_DEF void stt_PackSetOversampling(stt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample);
// Oversampling a font increases the quality by allowing higher-quality subpixel
// positioning, and is especially valuable at smaller text sizes.
//
// This function sets the amount of oversampling for all following calls to
// stt_PackFontRange(s) or stt_PackFontRangesGatherRects for a given
// pack context. The default (no oversampling) is achieved by h_oversample=1
// and v_oversample=1. The total number of pixels required is
// h_oversample*v_oversample larger than the default; for example, 2x2
// oversampling requires 4x the storage of 1x1. For best results, render
// oversampled textures with bilinear filtering. Look at the readme in
// stb/tests/oversample for information about oversampled fonts
//
// To use with PackFontRangesGather etc., you must set it before calls
// call to PackFontRangesGatherRects.

STT_DEF void stt_PackSetSkipMissingCodepoints(stt_pack_context *spc, int skip);
// If skip != 0, this tells struetype to skip any codepoints for which
// there is no corresponding glyph. If skip=0, which is the default, then
// codepoints without a glyph recived the font's "missing character" glyph,
// typically an empty box by convention.

STT_DEF void stt_GetPackedQuad(const stt_packedchar *chardata, int pw, int ph,  // same data as above
                               int char_index,             // character to display
                               float *xpos, float *ypos,   // pointers to current position in screen pixel space
                               stt_aligned_quad *q,      // output: quad to draw
                               int align_to_integer);

STT_DEF int  stt_PackFontRangesGatherRects(stt_pack_context *spc, const stt_fontinfo *info, stt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
STT_DEF void stt_PackFontRangesPackRects(stt_pack_context *spc, stbrp_rect *rects, int num_rects);
STT_DEF int  stt_PackFontRangesRenderIntoRects(stt_pack_context *spc, const stt_fontinfo *info, stt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
// Calling these functions in sequence is roughly equivalent to calling
// stt_PackFontRanges(). If you more control over the packing of multiple
// fonts, or if you want to pack custom data into a font texture, take a look
// at the source to of stt_PackFontRanges() and create a custom version
// using these functions, e.g. call GatherRects multiple times,
// building up a single array of rects, then call PackRects once,
// then call RenderIntoRects repeatedly. This may result in a
// better packing than calling PackFontRanges multiple times
// (or it may not).

// this is an opaque structure that you shouldn't mess with which holds
// all the context needed from PackBegin to PackEnd.
struct stt_pack_context {
   void *user_allocator_context;
   void *pack_info;
   int   width;
   int   height;
   int   stride_in_bytes;
   int   padding;
   int   skip_missing;
   unsigned int   h_oversample, v_oversample;
   unsigned char *pixels;
   void  *nodes;
};

//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//
//

STT_DEF int stt_GetNumberOfFonts(const unsigned char *data, int data_size);
// This function will determine the number of fonts in a font file.  TrueType
// collection (.ttc) files may contain multiple fonts, while TrueType font
// (.ttf) files only contain one font. The number of fonts can be used for
// indexing with the previous function where the index is between zero and one
// less than the total fonts. If an error occurs, -1 is returned.

STT_DEF int stt_GetFontOffsetForIndex(const unsigned char *data, int data_size, int index);
// Each .ttf/.ttc file may have more than one font. Each font has a sequential
// index number starting from 0. Call this function to get the font offset for
// a given index; it returns -1 if the index is out of range. A regular .ttf
// file will only define one font and it always be at offset 0, so it will
// return '0' for index 0, and -1 for all other indices.

// The following structure is defined publicly so you can declare one on
// the stack or as a global or etc, but you should treat it as opaque.
struct stt_fontinfo
{
   void           * userdata;
   unsigned char  * data;              // pointer to .ttf file
   int              fontstart;         // offset of start of font
   int              data_size;         // size of .ttf file buffer

   int numGlyphs;                     // number of glyphs, needed for range checking

   int loca,head,glyf,hhea,hmtx,kern,gpos,svg; // table locations as offset from start of .ttf
   int index_map;                     // a cmap mapping for our chosen character encoding
   int indexToLocFormat;              // format needed to map from glyph index to glyph

   stt__buf cff;                    // cff font data
   stt__buf charstrings;            // the charstring index
   stt__buf gsubrs;                 // global charstring subroutines index
   stt__buf subrs;                  // private charstring subroutines index
   stt__buf fontdicts;              // array of font dicts
   stt__buf fdselect;               // map from glyph to fontdict
};

STT_DEF int stt_InitFont(stt_fontinfo *info, const unsigned char *data, int data_size, int offset);
// Given an offset into the file that defines a font, this function builds
// the necessary cached info for the rest of the system. You must allocate
// the stt_fontinfo yourself, and stt_InitFont will fill it out. You don't
// need to do anything special to free it, because the contents are pure
// value data with no additional data structures. Returns 0 on failure.
// data_size is the size of the font buffer in bytes.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH-INDEX CONVERSIOn

STT_DEF int stt_FindGlyphIndex(const stt_fontinfo *info, int unicode_codepoint);
// If you're going to perform multiple operations on the same character
// and you want a speed-up, call this function with the character you're
// going to process, then use glyph-based functions instead of the
// codepoint-based functions.
// Returns 0 if the character codepoint is not defined in the font.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER PROPERTIES
//

STT_DEF float stt_ScaleForPixelHeight(const stt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose "height" is 'pixels' tall.
// Height is measured as the distance from the highest ascender to the lowest
// descender; in other words, it's equivalent to calling stt_GetFontVMetrics
// and computing:
//       scale = pixels / (ascent - descent)
// so if you prefer to measure height by the ascent only, use a similar calculation.

STT_DEF float stt_ScaleForMappingEmToPixels(const stt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose EM size is mapped to
// 'pixels' tall. This is probably what traditional APIs compute, but
// I'm not positive.

STT_DEF void stt_GetFontVMetrics(const stt_fontinfo *info, int *ascent, int *descent, int *lineGap);
// ascent is the coordinate above the baseline the font extends; descent
// is the coordinate below the baseline the font extends (i.e. it is typically negative)
// lineGap is the spacing between one row's descent and the next row's ascent...
// so you should advance the vertical position by "*ascent - *descent + *lineGap"
//   these are expressed in unscaled coordinates, so you must multiply by
//   the scale factor for a given size

STT_DEF int  stt_GetFontVMetricsOS2(const stt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap);
// analogous to GetFontVMetrics, but returns the "typographic" values from the OS/2
// table (specific to MS/Windows TTF files).
//
// Returns 1 on success (table present), 0 on failure.

STT_DEF void stt_GetFontBoundingBox(const stt_fontinfo *info, int *x0, int *y0, int *x1, int *y1);
// the bounding box around all possible characters

STT_DEF void stt_GetCodepointHMetrics(const stt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
// leftSideBearing is the offset from the current horizontal position to the left edge of the character
// advanceWidth is the offset from the current horizontal position to the next horizontal position
//   these are expressed in unscaled coordinates

STT_DEF int  stt_GetCodepointKernAdvance(const stt_fontinfo *info, int ch1, int ch2);
// an additional amount to add to the 'advance' value between ch1 and ch2

STT_DEF int stt_GetCodepointBox(const stt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1);
// Gets the bounding box of the visible part of the glyph, in unscaled coordinates

STT_DEF void stt_GetGlyphHMetrics(const stt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
STT_DEF int  stt_GetGlyphKernAdvance(const stt_fontinfo *info, int glyph1, int glyph2);
STT_DEF int  stt_GetGlyphBox(const stt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);
// as above, but takes one or more glyph indices for greater efficiency

typedef struct stt_kerningentry
{
   int glyph1; // use stt_FindGlyphIndex
   int glyph2;
   int advance;
} stt_kerningentry;

STT_DEF int  stt_GetKerningTableLength(const stt_fontinfo *info);
STT_DEF int  stt_GetKerningTable(const stt_fontinfo *info, stt_kerningentry* table, int table_length);
// Retrieves a complete list of all of the kerning pairs provided by the font
// stt_GetKerningTable never writes more than table_length entries and returns how many entries it did write.
// The table will be sorted by (a.glyph1 == b.glyph1)?(a.glyph2 < b.glyph2):(a.glyph1 < b.glyph1)

//////////////////////////////////////////////////////////////////////////////
//
// GLYPH SHAPES (you probably don't need these, but they have to go before
// the bitmaps for C declaration-order reasons)
//

#ifndef STT_vmove // you can predefine these to use different values (but why?)
   enum {
      STT_vmove=1,
      STT_vline,
      STT_vcurve,
      STT_vcubic
   };
#endif

#ifndef stt_vertex // you can predefine this to use different values
                   // (we share this with other code at RAD)
   #define stt_vertex_type short // can't use stt_int16 because that's not visible in the header file
   typedef struct
   {
      stt_vertex_type x,y,cx,cy,cx1,cy1;
      unsigned char type,padding;
   } stt_vertex;
#endif

STT_DEF int stt_IsGlyphEmpty(const stt_fontinfo *info, int glyph_index);
// returns non-zero if nothing is drawn for this glyph

STT_DEF int stt_GetCodepointShape(const stt_fontinfo *info, int unicode_codepoint, stt_vertex **vertices);
STT_DEF int stt_GetGlyphShape(const stt_fontinfo *info, int glyph_index, stt_vertex **vertices);
// returns # of vertices and fills *vertices with the pointer to them
//   these are expressed in "unscaled" coordinates
//
// The shape is a series of contours. Each one starts with
// a STT_moveto, then consists of a series of mixed
// STT_lineto and STT_curveto segments. A lineto
// draws a line from previous endpoint to its x,y; a curveto
// draws a quadratic bezier from previous endpoint to
// its x,y, using cx,cy as the bezier control point.

STT_DEF void stt_FreeShape(const stt_fontinfo *info, stt_vertex *vertices);
// frees the data allocated above

STT_DEF unsigned char *stt_FindSVGDoc(const stt_fontinfo *info, int gl);
STT_DEF int stt_GetCodepointSVG(const stt_fontinfo *info, int unicode_codepoint, const char **svg);
STT_DEF int stt_GetGlyphSVG(const stt_fontinfo *info, int gl, const char **svg);
// fills svg with the character's SVG data.
// returns data size or 0 if SVG not found.

//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

STT_DEF void stt_FreeBitmap(unsigned char *bitmap, void *userdata);
// frees the bitmap allocated below

STT_DEF unsigned char *stt_GetCodepointBitmap(const stt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
// allocates a large-enough single-channel 8bpp bitmap and renders the
// specified character/glyph at the specified scale into it, with
// antialiasing. 0 is no coverage (transparent), 255 is fully covered (opaque).
// *width & *height are filled out with the width & height of the bitmap,
// which is stored left-to-right, top-to-bottom.
//
// xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

STT_DEF unsigned char *stt_GetCodepointBitmapSubpixel(const stt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
// the same as stt_GetCodepoitnBitmap, but you can specify a subpixel
// shift for the character

STT_DEF void stt_MakeCodepointBitmap(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint);
// the same as stt_GetCodepointBitmap, but you pass in storage for the bitmap
// in the form of 'output', with row spacing of 'out_stride' bytes. the bitmap
// is clipped to out_w/out_h bytes. Call stt_GetCodepointBitmapBox to get the
// width and height and positioning info for it first.

STT_DEF void stt_MakeCodepointBitmapSubpixel(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint);
// same as stt_MakeCodepointBitmap, but you can specify a subpixel
// shift for the character

STT_DEF void stt_MakeCodepointBitmapSubpixelPrefilter(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint);
// same as stt_MakeCodepointBitmapSubpixel, but prefiltering
// is performed (see stt_PackSetOversampling)

STT_DEF void stt_GetCodepointBitmapBox(const stt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
// get the bbox of the bitmap centered around the glyph origin; so the
// bitmap width is ix1-ix0, height is iy1-iy0, and location to place
// the bitmap top left is (leftSideBearing*scale,iy0).
// (Note that the bitmap uses y-increases-down, but the shape uses
// y-increases-up, so CodepointBitmapBox and CodepointBox are inverted.)

STT_DEF void stt_GetCodepointBitmapBoxSubpixel(const stt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);
// same as stt_GetCodepointBitmapBox, but you can specify a subpixel
// shift for the character

// the following functions are equivalent to the above functions, but operate
// on glyph indices instead of Unicode codepoints (for efficiency)
STT_DEF unsigned char *stt_GetGlyphBitmap(const stt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STT_DEF unsigned char *stt_GetGlyphBitmapSubpixel(const stt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STT_DEF void stt_MakeGlyphBitmap(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);
STT_DEF void stt_MakeGlyphBitmapSubpixel(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);
STT_DEF void stt_MakeGlyphBitmapSubpixelPrefilter(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int glyph);
STT_DEF void stt_GetGlyphBitmapBox(const stt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
STT_DEF void stt_GetGlyphBitmapBoxSubpixel(const stt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);


// @TODO: don't expose this structure
typedef struct
{
   int w,h,stride;
   unsigned char *pixels;
} stt__bitmap;

// rasterize a shape with quadratic beziers into a bitmap
STT_DEF void stt_Rasterize(stt__bitmap *result,        // 1-channel bitmap to draw into
                               float flatness_in_pixels,     // allowable error of curve in pixels
                               stt_vertex *vertices,       // array of vertices defining shape
                               int num_verts,                // number of vertices in above array
                               float scale_x, float scale_y, // scale applied to input vertices
                               float shift_x, float shift_y, // translation applied to input vertices
                               int x_off, int y_off,         // another translation applied to input
                               int invert,                   // if non-zero, vertically flip shape
                               void *userdata);              // context for to STT_MALLOC

//////////////////////////////////////////////////////////////////////////////
//
// Signed Distance Function (or Field) rendering

STT_DEF void stt_FreeSDF(unsigned char *bitmap, void *userdata);
// frees the SDF bitmap allocated below

STT_DEF unsigned char * stt_GetGlyphSDF(const stt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
STT_DEF unsigned char * stt_GetCodepointSDF(const stt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
// These functions compute a discretized SDF field for a single character, suitable for storing
// in a single-channel texture, sampling with bilinear filtering, and testing against
// larger than some threshold to produce scalable fonts.
//        info              --  the font
//        scale             --  controls the size of the resulting SDF bitmap, same as it would be creating a regular bitmap
//        glyph/codepoint   --  the character to generate the SDF for
//        padding           --  extra "pixels" around the character which are filled with the distance to the character (not 0),
//                                 which allows effects like bit outlines
//        onedge_value      --  value 0-255 to test the SDF against to reconstruct the character (i.e. the isocontour of the character)
//        pixel_dist_scale  --  what value the SDF should increase by when moving one SDF "pixel" away from the edge (on the 0..255 scale)
//                                 if positive, > onedge_value is inside; if negative, < onedge_value is inside
//        width,height      --  output height & width of the SDF bitmap (including padding)
//        xoff,yoff         --  output origin of the character
//        return value      --  a 2D array of bytes 0..255, width*height in size
//
// pixel_dist_scale & onedge_value are a scale & bias that allows you to make
// optimal use of the limited 0..255 for your application, trading off precision
// and special effects. SDF values outside the range 0..255 are clamped to 0..255.
//
// Example:
//      scale = stt_ScaleForPixelHeight(22)
//      padding = 5
//      onedge_value = 180
//      pixel_dist_scale = 180/5.0 = 36.0
//
//      This will create an SDF bitmap in which the character is about 22 pixels
//      high but the whole bitmap is about 22+5+5=32 pixels high. To produce a filled
//      shape, sample the SDF at each pixel and fill the pixel if the SDF value
//      is greater than or equal to 180/255. (You'll actually want to antialias,
//      which is beyond the scope of this example.) Additionally, you can compute
//      offset outlines (e.g. to stroke the character border inside & outside,
//      or only outside). For example, to fill outside the character up to 3 SDF
//      pixels, you would compare against (180-36.0*3)/255 = 72/255. The above
//      choice of variables maps a range from 5 pixels outside the shape to
//      2 pixels inside the shape to 0..255; this is intended primarily for apply
//      outside effects only (the interior range is needed to allow proper
//      antialiasing of the font at *smaller* sizes)
//
// The function computes the SDF analytically at each SDF pixel, not by e.g.
// building a higher-res bitmap and approximating it. In theory the quality
// should be as high as possible for an SDF of this size & representation, but
// unclear if this is true in practice (perhaps building a higher-res bitmap
// and computing from that can allow drop-out prevention).
//
// The algorithm has not been optimized at all, so expect it to be slow
// if computing lots of characters or very large sizes.



//////////////////////////////////////////////////////////////////////////////
//
// Finding the right font...
//
// You should really just solve this offline, keep your own tables
// of what font is what, and don't try to get it out of the .ttf file.
// That's because getting it out of the .ttf file is really hard, because
// the names in the file can appear in many possible encodings, in many
// possible languages, and e.g. if you need a case-insensitive comparison,
// the details of that depend on the encoding & language in a complex way
// (actually underspecified in truetype, but also gigantic).
//
// But you can use the provided functions in two possible ways:
//     stt_FindMatchingFont() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling stt_InitFont()
//
//     stt_GetFontNameString() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called stt_InitFont() first.


STT_DEF int stt_FindMatchingFont(const unsigned char *fontdata, int fontdata_size, const char *name, int flags);
// returns the offset (not index) of the font that matches, or -1 if none
//   if you use STT_MACSTYLE_DONTCARE, use a font name like "Arial Bold".
//   if you use any other flag, use a font name like "Arial"; this checks
//     the 'macStyle' header field; i don't know if fonts set this consistently
#define STT_MACSTYLE_DONTCARE     0
#define STT_MACSTYLE_BOLD         1
#define STT_MACSTYLE_ITALIC       2
#define STT_MACSTYLE_UNDERSCORE   4
#define STT_MACSTYLE_NONE         8   // <= not same as 0, this makes us check the bitfield is 0

STT_DEF int stt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2);
// returns 1/0 whether the first string interpreted as utf8 is identical to
// the second string interpreted as big-endian utf16... useful for strings from next func

STT_DEF const char *stt_GetFontNameString(const stt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID);
// returns the string (which may be big-endian double byte, e.g. for unicode)
// and puts the length in bytes in *length.
//
// some of the values for the IDs are below; for more see the truetype spec:
//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
//     http://www.microsoft.com/typography/otspec/name.htm

enum { // platformID
   STT_PLATFORM_ID_UNICODE   =0,
   STT_PLATFORM_ID_MAC       =1,
   STT_PLATFORM_ID_ISO       =2,
   STT_PLATFORM_ID_MICROSOFT =3
};

enum { // encodingID for STT_PLATFORM_ID_UNICODE
   STT_UNICODE_EID_UNICODE_1_0    =0,
   STT_UNICODE_EID_UNICODE_1_1    =1,
   STT_UNICODE_EID_ISO_10646      =2,
   STT_UNICODE_EID_UNICODE_2_0_BMP=3,
   STT_UNICODE_EID_UNICODE_2_0_FULL=4
};

enum { // encodingID for STT_PLATFORM_ID_MICROSOFT
   STT_MS_EID_SYMBOL        =0,
   STT_MS_EID_UNICODE_BMP   =1,
   STT_MS_EID_SHIFTJIS      =2,
   STT_MS_EID_UNICODE_FULL  =10
};

enum { // encodingID for STT_PLATFORM_ID_MAC; same as Script Manager codes
   STT_MAC_EID_ROMAN        =0,   STT_MAC_EID_ARABIC       =4,
   STT_MAC_EID_JAPANESE     =1,   STT_MAC_EID_HEBREW       =5,
   STT_MAC_EID_CHINESE_TRAD =2,   STT_MAC_EID_GREEK        =6,
   STT_MAC_EID_KOREAN       =3,   STT_MAC_EID_RUSSIAN      =7
};

enum { // languageID for STT_PLATFORM_ID_MICROSOFT; same as LCID...
       // problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
   STT_MS_LANG_ENGLISH     =0x0409,   STT_MS_LANG_ITALIAN     =0x0410,
   STT_MS_LANG_CHINESE     =0x0804,   STT_MS_LANG_JAPANESE    =0x0411,
   STT_MS_LANG_DUTCH       =0x0413,   STT_MS_LANG_KOREAN      =0x0412,
   STT_MS_LANG_FRENCH      =0x040c,   STT_MS_LANG_RUSSIAN     =0x0419,
   STT_MS_LANG_GERMAN      =0x0407,   STT_MS_LANG_SPANISH     =0x0409,
   STT_MS_LANG_HEBREW      =0x040d,   STT_MS_LANG_SWEDISH     =0x041D
};

enum { // languageID for STT_PLATFORM_ID_MAC
   STT_MAC_LANG_ENGLISH      =0 ,   STT_MAC_LANG_JAPANESE     =11,
   STT_MAC_LANG_ARABIC       =12,   STT_MAC_LANG_KOREAN       =23,
   STT_MAC_LANG_DUTCH        =4 ,   STT_MAC_LANG_RUSSIAN      =32,
   STT_MAC_LANG_FRENCH       =1 ,   STT_MAC_LANG_SPANISH      =6 ,
   STT_MAC_LANG_GERMAN       =2 ,   STT_MAC_LANG_SWEDISH      =5 ,
   STT_MAC_LANG_HEBREW       =10,   STT_MAC_LANG_CHINESE_SIMPLIFIED =33,
   STT_MAC_LANG_ITALIAN      =3 ,   STT_MAC_LANG_CHINESE_TRAD =19
};

#ifdef __cplusplus
}
#endif

#endif //__INCLUDE_STRUETYPE_H__

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   IMPLEMENTATION
////
////

#ifdef STRUETYPE_IMPLEMENTATION

#ifndef STT_MAX_OVERSAMPLE
#define STT_MAX_OVERSAMPLE   8
#endif

#ifndef STT_MAX_BITMAP_PIXELS
#define STT_MAX_BITMAP_PIXELS 67108864
#endif

#ifndef STT_MAX_TESSELLATION_POINTS
#define STT_MAX_TESSELLATION_POINTS 1048576
#endif

#if STT_MAX_OVERSAMPLE > 255
#error "STT_MAX_OVERSAMPLE cannot be > 255"
#endif

typedef int stt__test_oversample_pow2[(STT_MAX_OVERSAMPLE & (STT_MAX_OVERSAMPLE-1)) == 0 ? 1 : -1];

#ifndef STT_RASTERIZER_VERSION
#define STT_RASTERIZER_VERSION 2
#endif

#ifdef _MSC_VER
#define STT__NOTUSED(v)  (void)(v)
#else
#define STT__NOTUSED(v)  (void)sizeof(v)
#endif

//////////////////////////////////////////////////////////////////////////
//
// stt__buf helpers to parse data from file
//

static stt_uint8 stt__buf_get8(stt__buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor++];
}

static stt_uint8 stt__buf_peek8(stt__buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor];
}

static void stt__buf_seek(stt__buf *b, int o)
{
   // Remove assertion and handle bounds gracefully
   b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void stt__buf_skip(stt__buf *b, int o)
{
   if (o > 0 && b->cursor > b->size - o)
      b->cursor = b->size;
   else if (o < 0 && o < -b->cursor)
      b->cursor = 0;
   else
      stt__buf_seek(b, b->cursor + o);
}

static stt_uint32 stt__buf_get(stt__buf *b, int n)
{
   stt_uint32 v = 0;
   int i;
   if (n < 1 || n > 4) return 0;
   for (i = 0; i < n; i++)
      v = (v << 8) | stt__buf_get8(b);
   return v;
}

static stt__buf stt__new_buf(const void *p, size_t size)
{
   stt__buf r;
   if (size > 0x3fffffff) size = 0;
   r.data = (stt_uint8*) p;
   r.size = (int) size;
   r.cursor = 0;
   return r;
}

#define stt__buf_get16(b)  stt__buf_get((b), 2)
#define stt__buf_get32(b)  stt__buf_get((b), 4)

static stt__buf stt__buf_range(const stt__buf *b, int o, int s)
{
   stt__buf r = stt__new_buf(NULL, 0);
   if (o < 0 || s < 0 || o > b->size || s > b->size - o) return r;
   r.data = b->data + o;
   r.size = s;
   return r;
}

static stt__buf stt__cff_get_index(stt__buf *b)
{
   int count, start, offsize;
   start = b->cursor;
   count = stt__buf_get16(b);
   if (count) {
      offsize = stt__buf_get8(b);
      if (offsize < 1 || offsize > 4 || count > (b->size - b->cursor) / offsize) {
         b->cursor = b->size;
         return stt__buf_range(b, 0, 0);
      }
      stt__buf_skip(b, offsize * count);
      {
         stt_uint32 final_offset = stt__buf_get(b, offsize);
         if (final_offset == 0 || final_offset - 1 > (stt_uint32)(b->size - b->cursor)) {
            b->cursor = b->size;
            return stt__buf_range(b, 0, 0);
         }
         stt__buf_skip(b, (int)final_offset - 1);
      }
   }
   return stt__buf_range(b, start, b->cursor - start);
}

static stt_uint32 stt__cff_int(stt__buf *b)
{
   int b0 = stt__buf_get8(b);
   if (b0 >= 32 && b0 <= 246)       return b0 - 139;
   else if (b0 >= 247 && b0 <= 250) return (b0 - 247)*256 + stt__buf_get8(b) + 108;
   else if (b0 >= 251 && b0 <= 254) return -(b0 - 251)*256 - stt__buf_get8(b) - 108;
   else if (b0 == 28)               return stt__buf_get16(b);
   else if (b0 == 29)               return stt__buf_get32(b);
   // Handle invalid CFF integer format gracefully
   return 0;
}

static void stt__cff_skip_operand(stt__buf *b) {
   int v, b0 = stt__buf_peek8(b);
   if (b0 < 28) return; // Handle invalid operand gracefully
   if (b0 == 30) {
      stt__buf_skip(b, 1);
      while (b->cursor < b->size) {
         v = stt__buf_get8(b);
         if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
            break;
      }
   } else {
      stt__cff_int(b);
   }
}

static stt__buf stt__dict_get(stt__buf *b, int key)
{
   stt__buf_seek(b, 0);
   while (b->cursor < b->size) {
      int start = b->cursor, end, op;
      while (stt__buf_peek8(b) >= 28)
         stt__cff_skip_operand(b);
      end = b->cursor;
      op = stt__buf_get8(b);
      if (op == 12)  op = stt__buf_get8(b) | 0x100;
      if (op == key) return stt__buf_range(b, start, end-start);
   }
   return stt__buf_range(b, 0, 0);
}

static void stt__dict_get_ints(stt__buf *b, int key, int outcount, stt_uint32 *out)
{
   int i;
   stt__buf operands = stt__dict_get(b, key);
   for (i = 0; i < outcount && operands.cursor < operands.size; i++)
      out[i] = stt__cff_int(&operands);
}

static int stt__cff_index_count(stt__buf *b)
{
   stt__buf_seek(b, 0);
   return stt__buf_get16(b);
}

static stt__buf stt__cff_index_get(stt__buf b, int i)
{
   int count, offsize, start, end;
   stt__buf_seek(&b, 0);
   count = stt__buf_get16(&b);
   offsize = stt__buf_get8(&b);
   if (i < 0 || i >= count || offsize < 1 || offsize > 4 ||
       count > (b.size - b.cursor) / offsize)
      return stt__buf_range(&b, 0, 0);
   stt__buf_skip(&b, i*offsize);
   start = stt__buf_get(&b, offsize);
   end = stt__buf_get(&b, offsize);
   if (start < 1 || end < start)
      return stt__buf_range(&b, 0, 0);
   return stt__buf_range(&b, 2+(count+1)*offsize+start, end-start);
}

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

// Safe accessors with bounds checking
static stt_uint8 stt__safe_read8(const stt_fontinfo *info, stt_uint32 offset)
{
   if (!info || !info->data || info->data_size < 0 || offset >= (stt_uint32)info->data_size) return 0;
   return info->data[offset];
}

static stt_uint16 stt__safe_read16(const stt_fontinfo *info, stt_uint32 offset)
{
   if (!info || !info->data || info->data_size < 2 || offset > (stt_uint32)info->data_size - 2) return 0;
   return (info->data[offset] << 8) | info->data[offset+1];
}

static stt_int16 stt__safe_read16_signed(const stt_fontinfo *info, stt_uint32 offset)
{
   if (!info || !info->data || info->data_size < 2 || offset > (stt_uint32)info->data_size - 2) return 0;
   return (stt_int16)((info->data[offset] << 8) | info->data[offset+1]);
}

static stt_uint32 stt__safe_read32(const stt_fontinfo *info, stt_uint32 offset)
{
   if (!info || !info->data || info->data_size < 4 || offset > (stt_uint32)info->data_size - 4) return 0;
   return ((stt_uint32)info->data[offset] << 24) |
          ((stt_uint32)info->data[offset+1] << 16) |
          ((stt_uint32)info->data[offset+2] << 8) |
          info->data[offset+3];
}

static int stt__safe_check_bounds(const stt_fontinfo *info, stt_uint32 offset, int size)
{
   if (!info || !info->data || info->data_size < 0 || size < 0)
      return 0;
   return offset <= (stt_uint32)info->data_size &&
          (stt_uint32)size <= (stt_uint32)info->data_size - offset;
}

#define stt_tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stt_tag(p,str)           stt_tag4(p,str[0],str[1],str[2],str[3])

static int stt__raw_check_bounds(const stt_uint8 *data, int data_size, stt_uint32 offset, stt_uint32 size)
{
   if (!data || data_size < 0 || offset > (stt_uint32)data_size)
      return 0;
   return size <= (stt_uint32)data_size - offset;
}

static stt_uint32 stt__raw_read32(const stt_uint8 *data, int data_size, stt_uint32 offset)
{
   if (!stt__raw_check_bounds(data, data_size, offset, 4)) return 0;
   return ((stt_uint32)data[offset] << 24) |
          ((stt_uint32)data[offset+1] << 16) |
          ((stt_uint32)data[offset+2] << 8) | data[offset+3];
}

static int stt__raw_tag(const stt_uint8 *data, int data_size, stt_uint32 offset, const char *tag)
{
   return stt__raw_check_bounds(data, data_size, offset, 4) &&
          data[offset] == (stt_uint8)tag[0] &&
          data[offset+1] == (stt_uint8)tag[1] &&
          data[offset+2] == (stt_uint8)tag[2] &&
          data[offset+3] == (stt_uint8)tag[3];
}

static int stt__isfont(const stt_uint8 *font, int data_size)
{
   // check the version number
   if (!stt__raw_check_bounds(font, data_size, 0, 4)) return 0;
   if (font[0] == '1' && font[1] == 0 && font[2] == 0 && font[3] == 0) return 1;
   if (stt__raw_tag(font, data_size, 0, "typ1")) return 1;
   if (stt__raw_tag(font, data_size, 0, "OTTO")) return 1;
   if (font[0] == 0 && font[1] == 1 && font[2] == 0 && font[3] == 0) return 1;
   if (stt__raw_tag(font, data_size, 0, "true")) return 1;
   return 0;
}

// @OPTIMIZE: binary search
static stt_uint32 stt__find_table(const stt_fontinfo *info, stt_uint32 fontstart, const char *tag)
{
   stt_int32 num_tables;
   stt_uint32 tabledir;
   stt_int32 i;
   if (!stt__safe_check_bounds(info, fontstart, 12)) return 0;
   num_tables = stt__safe_read16(info, fontstart + 4);
   tabledir = fontstart + 12;
   if ((stt_uint32)num_tables > ((stt_uint32)info->data_size - tabledir) / 16) return 0;
   for (i=0; i < num_tables; ++i) {
      stt_uint32 loc = tabledir + 16*i;
      if (stt_tag(info->data+loc, tag)) {
         stt_uint32 offset = stt__safe_read32(info, loc + 8);
         stt_uint32 length = stt__safe_read32(info, loc + 12);
         if (offset > (stt_uint32)info->data_size ||
             length > (stt_uint32)info->data_size - offset)
            return 0;
         return offset;
      }
   }
   return 0;
}

// Get the size of a table
static stt_uint32 stt__get_table_size(const stt_fontinfo *info, stt_uint32 fontstart, const char *tag)
{
   stt_int32 num_tables;
   stt_uint32 tabledir;
   stt_int32 i;
   if (!stt__safe_check_bounds(info, fontstart, 12)) return 0;
   num_tables = stt__safe_read16(info, fontstart + 4);
   tabledir = fontstart + 12;
   if ((stt_uint32)num_tables > ((stt_uint32)info->data_size - tabledir) / 16) return 0;
   for (i=0; i < num_tables; ++i) {
      stt_uint32 loc = tabledir + 16*i;
      if (stt_tag(info->data+loc, tag)) {
         stt_uint32 offset = stt__safe_read32(info, loc + 8);
         stt_uint32 length = stt__safe_read32(info, loc + 12);
         if (offset > (stt_uint32)info->data_size ||
             length > (stt_uint32)info->data_size - offset)
            return 0;
         return length;
      }
   }
   return 0;
}

static int stt_GetFontOffsetForIndex_internal(unsigned char *font_collection, int data_size, int index)
{
   stt_uint32 version, n, offset;
   if (index < 0 || data_size < 4) return -1;
   // if it's just a font, there's only one valid index
   if (stt__isfont(font_collection, data_size))
      return index == 0 ? 0 : -1;

   // check if it's a TTC
   if (stt__raw_tag(font_collection, data_size, 0, "ttcf")) {
      if (data_size < 12) return -1;
      // version 1?
      version = stt__raw_read32(font_collection, data_size, 4);
      if (version == 0x00010000 || version == 0x00020000) {
         n = stt__raw_read32(font_collection, data_size, 8);
         if (n > ((stt_uint32)data_size - 12) / 4 || (stt_uint32)index >= n)
            return -1;
         offset = stt__raw_read32(font_collection, data_size, 12+(stt_uint32)index*4);
         if (offset > (stt_uint32)data_size || !stt__isfont(font_collection+offset, data_size-(int)offset))
            return -1;
         return (int)offset;
      }
   }
   return -1;
}

static int stt_GetNumberOfFonts_internal(unsigned char *font_collection, int data_size)
{
   stt_uint32 version, n;
   if (data_size < 4) return 0;
   // if it's just a font, there's only one valid font
   if (stt__isfont(font_collection, data_size))
      return 1;

   // check if it's a TTC
   if (stt__raw_tag(font_collection, data_size, 0, "ttcf")) {
      if (data_size < 12) return 0;
      // version 1?
      version = stt__raw_read32(font_collection, data_size, 4);
      if (version == 0x00010000 || version == 0x00020000) {
         n = stt__raw_read32(font_collection, data_size, 8);
         if (n > ((stt_uint32)data_size - 12) / 4 || n > 0x7fffffff)
            return 0;
         return (int)n;
      }
   }
   return 0;
}

static stt__buf stt__get_subrs(stt__buf cff, stt__buf fontdict)
{
   stt_uint32 subrsoff = 0, private_loc[2] = { 0, 0 };
   stt__buf pdict;
   stt__dict_get_ints(&fontdict, 18, 2, private_loc);
   if (!private_loc[1] || !private_loc[0]) return stt__new_buf(NULL, 0);
   if (private_loc[1] > (stt_uint32)cff.size ||
       private_loc[0] > (stt_uint32)cff.size-private_loc[1])
      return stt__new_buf(NULL, 0);
   pdict = stt__buf_range(&cff, (int)private_loc[1], (int)private_loc[0]);
   stt__dict_get_ints(&pdict, 19, 1, &subrsoff);
   if (!subrsoff) return stt__new_buf(NULL, 0);
   if (subrsoff > (stt_uint32)cff.size-private_loc[1])
      return stt__new_buf(NULL, 0);
   stt__buf_seek(&cff, (int)(private_loc[1]+subrsoff));
   return stt__cff_get_index(&cff);
}

// since most people won't use this, find this table the first time it's needed
static int stt__get_svg(stt_fontinfo *info)
{
   stt_uint32 t, size;
   if (!info) return 0;
   if (info->svg < 0) {
      t = stt__find_table(info, info->fontstart, "SVG ");
      if (t) {
         stt_uint32 offset = stt__safe_read32(info, t + 2);
         size = stt__get_table_size(info, info->fontstart, "SVG ");
         if (size >= 10 && offset <= size-2 && t <= 0x7fffffff-offset)
            info->svg = (int)(t+offset);
         else
            info->svg = 0;
      } else {
         info->svg = 0;
      }
   }
   return info->svg;
}

static int stt_InitFont_internal(stt_fontinfo *info, unsigned char *data, int data_size, int fontstart)
{
   stt_uint32 cmap, cmap_size, t;
   stt_int32 i,numTables;

   if (!info || !data || data_size < 12 || fontstart < 0 ||
       fontstart > data_size - 12 ||
       !stt__isfont(data + fontstart, data_size - fontstart))
      return 0;

   STT_memset(info, 0, sizeof(*info));
   info->data = data;
   info->data_size = data_size;
   info->fontstart = fontstart;
   info->cff = stt__new_buf(NULL, 0);

   cmap = stt__find_table(info, fontstart, "cmap");       // required
   info->loca = stt__find_table(info, fontstart, "loca"); // required
   info->head = stt__find_table(info, fontstart, "head"); // required
   info->glyf = stt__find_table(info, fontstart, "glyf"); // required
   info->hhea = stt__find_table(info, fontstart, "hhea"); // required
   info->hmtx = stt__find_table(info, fontstart, "hmtx"); // required
   info->kern = stt__find_table(info, fontstart, "kern"); // not required
   info->gpos = stt__find_table(info, fontstart, "GPOS"); // not required

   cmap_size = stt__get_table_size(info, fontstart, "cmap");
   if (!cmap || cmap_size < 4 || !info->head ||
       stt__get_table_size(info, fontstart, "head") < 54 ||
       !info->hhea || stt__get_table_size(info, fontstart, "hhea") < 36 ||
       !info->hmtx || stt__get_table_size(info, fontstart, "hmtx") < 4)
      return 0;
   if (info->glyf) {
      // required for truetype
      if (!info->loca || stt__get_table_size(info, fontstart, "loca") < 4)
         return 0;
   } else {
      // initialization for CFF / Type2 fonts (OTF)
      stt__buf b, topdict, topdictidx;
      stt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
      stt_uint32 cff;

      cff = stt__find_table(info, fontstart, "CFF ");
      if (!cff) return 0;

      info->fontdicts = stt__new_buf(NULL, 0);
      info->fdselect = stt__new_buf(NULL, 0);

      // Use actual table size instead of hardcoded 512MB
      stt_uint32 cff_size = stt__get_table_size(info, fontstart, "CFF ");
      if (cff_size == 0 || cff > (stt_uint32)info->data_size ||
          cff_size > (stt_uint32)info->data_size - cff) return 0;
      info->cff = stt__new_buf(data+cff, cff_size);
      b = info->cff;

      // read the header
      stt__buf_skip(&b, 2);
      stt__buf_seek(&b, stt__buf_get8(&b)); // hdrsize

      // @TODO the name INDEX could list multiple fonts,
      // but we just use the first one.
      stt__cff_get_index(&b);  // name INDEX
      topdictidx = stt__cff_get_index(&b);
      topdict = stt__cff_index_get(topdictidx, 0);
      stt__cff_get_index(&b);  // string INDEX
      info->gsubrs = stt__cff_get_index(&b);

      stt__dict_get_ints(&topdict, 17, 1, &charstrings);
      stt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
      stt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
      stt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
      info->subrs = stt__get_subrs(b, topdict);

      // we only support Type 2 charstrings
      if (cstype != 2) return 0;
      if (charstrings == 0) return 0;

      if (fdarrayoff) {
         // looks like a CID font
         if (!fdselectoff) return 0;
         if (fdarrayoff > (stt_uint32)b.size ||
             fdselectoff > (stt_uint32)b.size) return 0;
         stt__buf_seek(&b, (int)fdarrayoff);
         info->fontdicts = stt__cff_get_index(&b);
         info->fdselect = stt__buf_range(&b, (int)fdselectoff,
                                         b.size-(int)fdselectoff);
      }

      if (charstrings > (stt_uint32)b.size) return 0;
      stt__buf_seek(&b, (int)charstrings);
      info->charstrings = stt__cff_get_index(&b);
      if (!info->charstrings.size) return 0;
   }

   t = stt__find_table(info, fontstart, "maxp");
   if (t) {
      if (stt__get_table_size(info, fontstart, "maxp") < 6) return 0;
      info->numGlyphs = stt__safe_read16(info, t+4);
      if (!info->numGlyphs) return 0;
   } else
      info->numGlyphs = 0xffff;

   info->svg = -1;

   // find a cmap encoding table we understand *now* to avoid searching
   // later. (todo: could make this installable)
   // the same regardless of glyph.
   numTables = stt__safe_read16(info, cmap + 2);
   if ((stt_uint32)numTables > (cmap_size - 4) / 8)
      return 0;
   info->index_map = 0;
   for (i=0; i < numTables; ++i) {
      stt_uint32 encoding_record = cmap + 4 + 8 * i;
      if (encoding_record < cmap || encoding_record > cmap + cmap_size - 8)
         break;
      // find an encoding we understand:
      switch(stt__safe_read16(info, encoding_record)) {
         case STT_PLATFORM_ID_MICROSOFT:
            switch (stt__safe_read16(info, encoding_record+2)) {
               case STT_MS_EID_UNICODE_BMP:
               case STT_MS_EID_UNICODE_FULL:
                  // MS/Unicode
                  t = stt__safe_read32(info, encoding_record+4);
                  if (t < cmap_size)
                     info->index_map = cmap + t;
                  break;
            }
            break;
        case STT_PLATFORM_ID_UNICODE:
            // Mac/iOS has these
            // all the encodingIDs are unicode, so we don't bother to check it
            t = stt__safe_read32(info, encoding_record+4);
            if (t < cmap_size)
               info->index_map = cmap + t;
            break;
      }
   }
   if (info->index_map == 0 ||
       (stt_uint32)info->index_map > cmap + cmap_size - 2)
      return 0;
   {
      stt_uint32 cmap_relative = (stt_uint32)info->index_map-cmap;
      stt_uint32 cmap_available = cmap_size-cmap_relative;
      stt_uint16 format = stt__safe_read16(info, (stt_uint32)info->index_map);
      stt_uint32 subtable_length;
      if (format == 0 || format == 4 || format == 6) {
         if (cmap_available < 4) return 0;
         subtable_length = stt__safe_read16(info,
                                            (stt_uint32)info->index_map+2);
      } else if (format == 12 || format == 13) {
         if (cmap_available < 8) return 0;
         subtable_length = stt__safe_read32(info,
                                            (stt_uint32)info->index_map+4);
      } else {
         return 0;
      }
      if (subtable_length < 4 || subtable_length > cmap_available) return 0;
   }

   info->indexToLocFormat = stt__safe_read16(info, info->head + 50);
   return 1;
}

STT_DEF int stt_FindGlyphIndex(const stt_fontinfo *info, int unicode_codepoint)
{
   stt_uint32 index_map, available, length;
   stt_uint16 format;

   if (!info || unicode_codepoint < 0 || info->index_map < 0 ||
       !stt__safe_check_bounds(info, (stt_uint32)info->index_map, 2))
      return 0;
   index_map = (stt_uint32)info->index_map;
   available = (stt_uint32)info->data_size - index_map;
   format = stt__safe_read16(info, index_map);

   if (format == 0) {
      if (available < 6) return 0;
      length = stt__safe_read16(info, index_map+2);
      if (length < 6 || length > available || (stt_uint32)unicode_codepoint >= length-6)
         return 0;
      return stt__safe_read8(info, index_map+6+(stt_uint32)unicode_codepoint);
   }

   if (format == 6) {
      stt_uint32 first, count;
      if (available < 10) return 0;
      length = stt__safe_read16(info, index_map+2);
      first = stt__safe_read16(info, index_map+6);
      count = stt__safe_read16(info, index_map+8);
      if (length < 10 || length > available || count > (length-10)/2 ||
          (stt_uint32)unicode_codepoint < first ||
          (stt_uint32)unicode_codepoint-first >= count)
         return 0;
      return stt__safe_read16(info, index_map+10+
                              ((stt_uint32)unicode_codepoint-first)*2);
   }

   if (format == 4) {
      stt_uint32 segcount, low, high, item;
      stt_uint32 end_base, start_base, delta_base, range_base;
      stt_uint16 start, end, range_offset, glyph;
      stt_int16 delta;
      if (available < 16 || unicode_codepoint > 0xffff) return 0;
      length = stt__safe_read16(info, index_map+2);
      segcount = stt__safe_read16(info, index_map+6) >> 1;
      if (length < 16 || length > available || !segcount ||
          segcount > (length-16)/8)
         return 0;
      end_base = index_map+14;
      start_base = end_base+2*segcount+2;
      delta_base = start_base+2*segcount;
      range_base = delta_base+2*segcount;

      low = 0;
      high = segcount;
      while (low < high) {
         stt_uint32 mid = low+(high-low)/2;
         end = stt__safe_read16(info, end_base+2*mid);
         if ((stt_uint32)unicode_codepoint > end)
            low = mid+1;
         else
            high = mid;
      }
      if (low >= segcount) return 0;
      item = low;
      start = stt__safe_read16(info, start_base+2*item);
      end = stt__safe_read16(info, end_base+2*item);
      if ((stt_uint32)unicode_codepoint < start ||
          (stt_uint32)unicode_codepoint > end)
         return 0;
      delta = stt__safe_read16_signed(info, delta_base+2*item);
      range_offset = stt__safe_read16(info, range_base+2*item);
      if (!range_offset)
         return (stt_uint16)(unicode_codepoint+delta);
      {
         stt_uint32 range_word = range_base+2*item;
         stt_uint32 delta_words = (stt_uint32)unicode_codepoint-start;
         stt_uint32 table_end = index_map+length;
         stt_uint32 glyph_word;
         if (range_offset > table_end-range_word)
            return 0;
         glyph_word = range_word+range_offset;
         if (glyph_word > table_end || table_end-glyph_word < 2 ||
             delta_words > (table_end-glyph_word-2)/2)
            return 0;
         glyph = stt__safe_read16(info, glyph_word+2*delta_words);
      }
      return glyph ? (stt_uint16)(glyph+delta) : 0;
   }

   if (format == 12 || format == 13) {
      stt_uint32 ngroups, low, high;
      if (available < 16) return 0;
      length = stt__safe_read32(info, index_map+4);
      ngroups = stt__safe_read32(info, index_map+12);
      if (length < 16 || length > available || ngroups > (length-16)/12)
         return 0;
      low = 0;
      high = ngroups;
      while (low < high) {
         stt_uint32 mid = low+(high-low)/2;
         stt_uint32 group = index_map+16+mid*12;
         stt_uint32 start_char = stt__safe_read32(info, group);
         stt_uint32 end_char = stt__safe_read32(info, group+4);
         if ((stt_uint32)unicode_codepoint < start_char)
            high = mid;
         else if ((stt_uint32)unicode_codepoint > end_char)
            low = mid+1;
         else {
            stt_uint32 start_glyph = stt__safe_read32(info, group+8);
            stt_uint32 glyph = format == 12 ?
               start_glyph+(stt_uint32)unicode_codepoint-start_char : start_glyph;
            return glyph < (stt_uint32)info->numGlyphs ? (int)glyph : 0;
         }
      }
   }
   return 0;
}

STT_DEF int stt_GetCodepointShape(const stt_fontinfo *info, int unicode_codepoint, stt_vertex **vertices)
{
   return stt_GetGlyphShape(info, stt_FindGlyphIndex(info, unicode_codepoint), vertices);
}

static void stt_setvertex(stt_vertex *v, stt_uint8 type, stt_int32 x, stt_int32 y, stt_int32 cx, stt_int32 cy)
{
   v->type = type;
   v->x = (stt_int16) x;
   v->y = (stt_int16) y;
   v->cx = (stt_int16) cx;
   v->cy = (stt_int16) cy;
}

static int stt__GetGlyfOffset(const stt_fontinfo *info, int glyph_index,
                              stt_uint32 *glyph_size)
{
   stt_uint32 g1,g2, loca_size, glyf_size, entry;

   if (glyph_size) *glyph_size = 0;
   if (!info || info->cff.size || glyph_index < 0 ||
       glyph_index >= info->numGlyphs) return -1;

   if (info->indexToLocFormat >= 2)    return -1; // unknown index->glyph map format
   loca_size = stt__get_table_size(info, info->fontstart, "loca");
   glyf_size = stt__get_table_size(info, info->fontstart, "glyf");
   if (!loca_size || !glyf_size) return -1;

   if (info->indexToLocFormat == 0) {
      entry = (stt_uint32)glyph_index * 2;
      if (entry > loca_size || loca_size-entry < 4) return -1;
      g1 = (stt_uint32)stt__safe_read16(info, info->loca+entry) * 2;
      g2 = (stt_uint32)stt__safe_read16(info, info->loca+entry+2) * 2;
   } else {
      entry = (stt_uint32)glyph_index * 4;
      if (entry > loca_size || loca_size-entry < 8) return -1;
      g1 = stt__safe_read32(info, info->loca+entry);
      g2 = stt__safe_read32(info, info->loca+entry+4);
   }

   if (g1 >= g2 || g2 > glyf_size ||
       g1 > (stt_uint32)info->data_size-(stt_uint32)info->glyf)
      return -1;
   if (glyph_size) *glyph_size = g2-g1;
   return info->glyf <= 0x7fffffff-(int)g1 ? info->glyf+(int)g1 : -1;
}

static int stt__GetGlyphInfoT2(const stt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

STT_DEF int stt_GetGlyphBox(const stt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   if (x0) *x0 = 0;
   if (y0) *y0 = 0;
   if (x1) *x1 = 0;
   if (y1) *y1 = 0;
   if (!info || glyph_index < 0 || glyph_index >= info->numGlyphs) return 0;
   if (info->cff.size) {
      return stt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1) != 0;
   } else {
      stt_uint32 glyph_size;
      int g = stt__GetGlyfOffset(info, glyph_index, &glyph_size);
      if (g < 0 || glyph_size < 10) return 0;

      if (x0) *x0 = stt__safe_read16_signed(info, g + 2);
      if (y0) *y0 = stt__safe_read16_signed(info, g + 4);
      if (x1) *x1 = stt__safe_read16_signed(info, g + 6);
      if (y1) *y1 = stt__safe_read16_signed(info, g + 8);
   }
   return 1;
}

STT_DEF int stt_GetCodepointBox(const stt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1)
{
   return stt_GetGlyphBox(info, stt_FindGlyphIndex(info,codepoint), x0,y0,x1,y1);
}

STT_DEF int stt_IsGlyphEmpty(const stt_fontinfo *info, int glyph_index)
{
   stt_int16 numberOfContours;
   int g;
   if (!info || glyph_index < 0 || glyph_index >= info->numGlyphs) return 1;
   if (info->cff.size)
      return stt__GetGlyphInfoT2(info, glyph_index, NULL, NULL, NULL, NULL) == 0;
   {
      stt_uint32 glyph_size;
      g = stt__GetGlyfOffset(info, glyph_index, &glyph_size);
      if (g < 0 || glyph_size < 2) return 1;
   }
   numberOfContours = stt__safe_read16_signed(info, (stt_uint32)g);
   return numberOfContours == 0;
}

static int stt__close_shape(stt_vertex *vertices, int num_vertices, int was_off, int start_off,
    stt_int32 sx, stt_int32 sy, stt_int32 scx, stt_int32 scy, stt_int32 cx, stt_int32 cy)
{
   if (start_off) {
      if (was_off)
         stt_setvertex(&vertices[num_vertices++], STT_vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
      stt_setvertex(&vertices[num_vertices++], STT_vcurve, sx,sy,scx,scy);
   } else {
      if (was_off)
         stt_setvertex(&vertices[num_vertices++], STT_vcurve,sx,sy,cx,cy);
      else
         stt_setvertex(&vertices[num_vertices++], STT_vline,sx,sy,0,0);
   }
   return num_vertices;
}

#define STT_MAX_COMPOSITE_DEPTH 32

static int stt__GetGlyphShapeTT(const stt_fontinfo *info, int glyph_index,
                                stt_vertex **pvertices, int depth)
{
   stt_int16 numberOfContours;
   stt_vertex *vertices = NULL;
   stt_uint32 glyph_size, glyph_end;
   int num_vertices = 0;
   int g;

   if (!pvertices) return 0;
   *pvertices = NULL;
   if (depth > STT_MAX_COMPOSITE_DEPTH) return 0;
   g = stt__GetGlyfOffset(info, glyph_index, &glyph_size);
   if (g < 0 || glyph_size < 2) return 0;
   glyph_end = (stt_uint32)g + glyph_size;
   numberOfContours = stt__safe_read16_signed(info, (stt_uint32)g);

   if (numberOfContours > 0) {
      stt_uint8 flags = 0, flagcount = 0;
      stt_int32 i, j, m, n, next_move, was_off = 0, off, start_off = 0;
      stt_int32 x, y, cx, cy, sx, sy, scx, scy;
      stt_uint32 end_pts, ins_offset, points, previous_end = 0;

      if (glyph_size < 12+(stt_uint32)numberOfContours*2) return 0;
      end_pts = (stt_uint32)g+10;
      for (j=0; j<numberOfContours; ++j) {
         stt_uint32 endpoint = stt__safe_read16(info, end_pts+(stt_uint32)j*2);
         if (j && endpoint <= previous_end) return 0;
         previous_end = endpoint;
      }
      n = (stt_int32)previous_end+1;
      if (n <= 0) return 0;
      ins_offset = end_pts+(stt_uint32)numberOfContours*2;
      if (ins_offset > glyph_end || glyph_end-ins_offset < 2) return 0;
      {
         stt_uint32 ins = stt__safe_read16(info, ins_offset);
         if (ins > glyph_end-ins_offset-2) return 0;
         points = ins_offset+2+ins;
      }

      m = n+2*(stt_int32)numberOfContours;
      if (m < n || (size_t)m > ((size_t)-1)/sizeof(vertices[0])) return 0;
      vertices = (stt_vertex *)STT_malloc((size_t)m*sizeof(vertices[0]),
                                          info->userdata);
      if (!vertices) return 0;
      off = m-n;

      for (i=0; i<n; ++i) {
         if (!flagcount) {
            if (points >= glyph_end) goto malformed;
            flags = stt__safe_read8(info, points++);
            if (flags & 8) {
               if (points >= glyph_end) goto malformed;
               flagcount = stt__safe_read8(info, points++);
               if ((stt_int32)flagcount > n-i-1) goto malformed;
            }
         } else {
            --flagcount;
         }
         vertices[off+i].type = flags;
      }

      x = 0;
      for (i=0; i<n; ++i) {
         stt_int32 dx = 0;
         flags = (stt_uint8)vertices[off+i].type;
         if (flags & 2) {
            if (points >= glyph_end) goto malformed;
            dx = stt__safe_read8(info, points++);
            if (!(flags & 16)) dx = -dx;
         } else if (!(flags & 16)) {
            if (points > glyph_end || glyph_end-points < 2) goto malformed;
            dx = stt__safe_read16_signed(info, points);
            points += 2;
         }
         if ((dx > 0 && x > 2147483647-dx) ||
             (dx < 0 && x < (-2147483647-1)-dx)) goto malformed;
         x += dx;
         vertices[off+i].x = (stt_vertex_type)x;
      }

      y = 0;
      for (i=0; i<n; ++i) {
         stt_int32 dy = 0;
         flags = (stt_uint8)vertices[off+i].type;
         if (flags & 4) {
            if (points >= glyph_end) goto malformed;
            dy = stt__safe_read8(info, points++);
            if (!(flags & 32)) dy = -dy;
         } else if (!(flags & 32)) {
            if (points > glyph_end || glyph_end-points < 2) goto malformed;
            dy = stt__safe_read16_signed(info, points);
            points += 2;
         }
         if ((dy > 0 && y > 2147483647-dy) ||
             (dy < 0 && y < (-2147483647-1)-dy)) goto malformed;
         y += dy;
         vertices[off+i].y = (stt_vertex_type)y;
      }

      next_move = 0;
      j = 0;
      sx = sy = cx = cy = scx = scy = 0;
      for (i=0; i<n; ++i) {
         flags = (stt_uint8)vertices[off+i].type;
         x = vertices[off+i].x;
         y = vertices[off+i].y;

         if (next_move == i) {
            stt_int32 contour_start = i;
            stt_int32 contour_end;
            stt_int32 next_i;
            if (j >= numberOfContours) goto malformed;
            contour_end = stt__safe_read16(info, end_pts+(stt_uint32)j*2);
            if (contour_end < contour_start || contour_end >= n) goto malformed;
            if (i)
               num_vertices = stt__close_shape(vertices, num_vertices,
                  was_off, start_off, sx, sy, scx, scy, cx, cy);

            start_off = !(flags & 1);
            if (start_off) {
               scx = x;
               scy = y;
               next_i = i < contour_end ? i+1 : contour_start;
               if (!(vertices[off+next_i].type & 1)) {
                  sx = (x+(stt_int32)vertices[off+next_i].x) >> 1;
                  sy = (y+(stt_int32)vertices[off+next_i].y) >> 1;
               } else {
                  sx = vertices[off+next_i].x;
                  sy = vertices[off+next_i].y;
                  if (next_i != contour_start) ++i;
               }
            } else {
               sx = x;
               sy = y;
            }
            stt_setvertex(&vertices[num_vertices++], STT_vmove, sx, sy, 0, 0);
            was_off = 0;
            next_move = contour_end+1;
            ++j;
         } else if (!(flags & 1)) {
            if (was_off)
               stt_setvertex(&vertices[num_vertices++], STT_vcurve,
                             (cx+x)>>1, (cy+y)>>1, cx, cy);
            cx = x;
            cy = y;
            was_off = 1;
         } else {
            if (was_off)
               stt_setvertex(&vertices[num_vertices++], STT_vcurve,
                             x, y, cx, cy);
            else
               stt_setvertex(&vertices[num_vertices++], STT_vline,
                             x, y, 0, 0);
            was_off = 0;
         }
      }
      if (j != numberOfContours) goto malformed;
      num_vertices = stt__close_shape(vertices, num_vertices, was_off,
                                      start_off, sx, sy, scx, scy, cx, cy);
   } else if (numberOfContours < 0) {
      stt_uint32 comp = (stt_uint32)g+10;
      int more = 1, component_count = 0;

      if (glyph_size < 14) return 0;
      while (more) {
         stt_uint16 flags, gidx;
         int comp_num_verts, i;
         stt_vertex *comp_verts = NULL, *tmp;
         float mtx[6] = {1,0,0,1,0,0}, m, n;

         if (++component_count > (int)(glyph_size/4) ||
             comp > glyph_end || glyph_end-comp < 4) goto malformed;
         flags = stt__safe_read16(info, comp); comp += 2;
         gidx = stt__safe_read16(info, comp); comp += 2;

         if (!(flags & 2)) goto malformed; /* point matching unsupported */
         if (flags & 1) {
            if (comp > glyph_end || glyph_end-comp < 4) goto malformed;
            mtx[4] = stt__safe_read16_signed(info, comp); comp += 2;
            mtx[5] = stt__safe_read16_signed(info, comp); comp += 2;
         } else {
            if (comp > glyph_end || glyph_end-comp < 2) goto malformed;
            mtx[4] = (stt_int8)stt__safe_read8(info, comp++);
            mtx[5] = (stt_int8)stt__safe_read8(info, comp++);
         }
         if (flags & (1<<3)) {
            if (comp > glyph_end || glyph_end-comp < 2) goto malformed;
            mtx[0] = mtx[3] = stt__safe_read16_signed(info, comp)/16384.0f;
            comp += 2;
         } else if (flags & (1<<6)) {
            if (comp > glyph_end || glyph_end-comp < 4) goto malformed;
            mtx[0] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
            mtx[3] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
         } else if (flags & (1<<7)) {
            if (comp > glyph_end || glyph_end-comp < 8) goto malformed;
            mtx[0] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
            mtx[1] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
            mtx[2] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
            mtx[3] = stt__safe_read16_signed(info, comp)/16384.0f; comp += 2;
         }

         m = (float)STT_sqrt(mtx[0]*mtx[0]+mtx[1]*mtx[1]);
         n = (float)STT_sqrt(mtx[2]*mtx[2]+mtx[3]*mtx[3]);
         comp_num_verts = stt__GetGlyphShapeTT(info, gidx, &comp_verts,
                                               depth+1);
         if (comp_num_verts > 0) {
            if (comp_num_verts > 0x7fffffff-num_vertices) {
               STT_free(comp_verts, info->userdata);
               goto malformed;
            }
            for (i=0; i<comp_num_verts; ++i) {
               stt_vertex *v = &comp_verts[i];
               float tx, ty;
               stt_vertex_type vx = v->x, vy = v->y;
               tx = m*(mtx[0]*vx+mtx[2]*vy+mtx[4]);
               ty = n*(mtx[1]*vx+mtx[3]*vy+mtx[5]);
               if (tx < -32768.0f) tx = -32768.0f;
               if (tx >  32767.0f) tx =  32767.0f;
               if (ty < -32768.0f) ty = -32768.0f;
               if (ty >  32767.0f) ty =  32767.0f;
               v->x = (stt_vertex_type)tx;
               v->y = (stt_vertex_type)ty;
               vx = v->cx; vy = v->cy;
               tx = m*(mtx[0]*vx+mtx[2]*vy+mtx[4]);
               ty = n*(mtx[1]*vx+mtx[3]*vy+mtx[5]);
               if (tx < -32768.0f) tx = -32768.0f;
               if (tx >  32767.0f) tx =  32767.0f;
               if (ty < -32768.0f) ty = -32768.0f;
               if (ty >  32767.0f) ty =  32767.0f;
               v->cx = (stt_vertex_type)tx;
               v->cy = (stt_vertex_type)ty;
            }
            tmp = (stt_vertex *)STT_malloc(
               (size_t)(num_vertices+comp_num_verts)*sizeof(*tmp),
               info->userdata);
            if (!tmp) {
               STT_free(comp_verts, info->userdata);
               goto malformed;
            }
            if (num_vertices)
               STT_memcpy(tmp, vertices, (size_t)num_vertices*sizeof(*tmp));
            STT_memcpy(tmp+num_vertices, comp_verts,
                       (size_t)comp_num_verts*sizeof(*tmp));
            if (vertices) STT_free(vertices, info->userdata);
            STT_free(comp_verts, info->userdata);
            vertices = tmp;
            num_vertices += comp_num_verts;
         }
         more = flags & (1<<5);
      }
   }

   *pvertices = vertices;
   return num_vertices;

malformed:
   if (vertices) STT_free(vertices, info->userdata);
   return 0;
}

typedef struct
{
   int bounds;
   int started;
   float first_x, first_y;
   float x, y;
   stt_int32 min_x, max_x, min_y, max_y;

   stt_vertex *pvertices;
   int num_vertices;
   int capacity;
   int failed;
} stt__csctx;

#define STT__CSCTX_INIT(bounds) {bounds,0, 0,0, 0,0, 0,0,0,0, NULL, 0, 0, 0}

static void stt__track_vertex(stt__csctx *c, stt_int32 x, stt_int32 y)
{
   if (x > c->max_x || !c->started) c->max_x = x;
   if (y > c->max_y || !c->started) c->max_y = y;
   if (x < c->min_x || !c->started) c->min_x = x;
   if (y < c->min_y || !c->started) c->min_y = y;
   c->started = 1;
}

static void stt__csctx_v(stt__csctx *c, stt_uint8 type, stt_int32 x, stt_int32 y, stt_int32 cx, stt_int32 cy, stt_int32 cx1, stt_int32 cy1)
{
   if (c->bounds) {
      stt__track_vertex(c, x, y);
      if (type == STT_vcubic) {
         stt__track_vertex(c, cx, cy);
         stt__track_vertex(c, cx1, cy1);
      }
   } else {
      if (!c->pvertices || c->num_vertices >= c->capacity) {
         c->failed = 1;
         return;
      }
      stt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
      c->pvertices[c->num_vertices].cx1 = (stt_int16) cx1;
      c->pvertices[c->num_vertices].cy1 = (stt_int16) cy1;
   }
   c->num_vertices++;
}

static void stt__csctx_close_shape(stt__csctx *ctx)
{
   if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
      stt__csctx_v(ctx, STT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
}

static void stt__csctx_rmove_to(stt__csctx *ctx, float dx, float dy)
{
   stt__csctx_close_shape(ctx);
   ctx->first_x = ctx->x = ctx->x + dx;
   ctx->first_y = ctx->y = ctx->y + dy;
   stt__csctx_v(ctx, STT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stt__csctx_rline_to(stt__csctx *ctx, float dx, float dy)
{
   ctx->x += dx;
   ctx->y += dy;
   stt__csctx_v(ctx, STT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stt__csctx_rccurve_to(stt__csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
   float cx1 = ctx->x + dx1;
   float cy1 = ctx->y + dy1;
   float cx2 = cx1 + dx2;
   float cy2 = cy1 + dy2;
   ctx->x = cx2 + dx3;
   ctx->y = cy2 + dy3;
   stt__csctx_v(ctx, STT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
}

static stt__buf stt__get_subr(stt__buf idx, int n)
{
   int count = stt__cff_index_count(&idx);
   int bias = 107;
   if (count >= 33900)
      bias = 32768;
   else if (count >= 1240)
      bias = 1131;
   n += bias;
   if (n < 0 || n >= count)
      return stt__new_buf(NULL, 0);
   return stt__cff_index_get(idx, n);
}

static stt__buf stt__cid_get_glyph_subrs(const stt_fontinfo *info, int glyph_index)
{
   stt__buf fdselect;
   int nranges, start, end, v, fmt, fdselector = -1, i;

   if (!info || glyph_index < 0 || glyph_index >= info->numGlyphs)
      return stt__new_buf(NULL, 0);
   fdselect = info->fdselect;
   stt__buf_seek(&fdselect, 0);
   fmt = stt__buf_get8(&fdselect);
   if (fmt == 0) {
      // untested
      stt__buf_skip(&fdselect, glyph_index);
      fdselector = stt__buf_get8(&fdselect);
   } else if (fmt == 3) {
      nranges = stt__buf_get16(&fdselect);
      start = stt__buf_get16(&fdselect);
      for (i = 0; i < nranges; i++) {
         v = stt__buf_get8(&fdselect);
         end = stt__buf_get16(&fdselect);
         if (glyph_index >= start && glyph_index < end) {
            fdselector = v;
            break;
         }
         start = end;
      }
   }
   if (fdselector == -1) return stt__new_buf(NULL, 0);
   return stt__get_subrs(info->cff, stt__cff_index_get(info->fontdicts, fdselector));
}

static int stt__run_charstring(const stt_fontinfo *info, int glyph_index, stt__csctx *c)
{
   int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
   int has_subrs = 0, clear_stack;
   float s[48];
   stt__buf subr_stack[10], subrs, b;
   float f;

#define STT__CSERR(s) (0)

   // this currently ignores the initial width value, which isn't needed if we have hmtx
   if (!info || !c || glyph_index < 0 || glyph_index >= info->numGlyphs)
      return 0;
   subrs = info->subrs;
   b = stt__cff_index_get(info->charstrings, glyph_index);
   while (b.cursor < b.size) {
      if (c->failed) return 0;
      i = 0;
      clear_stack = 1;
      b0 = stt__buf_get8(&b);
      switch (b0) {
      // @TODO implement hinting
      case 0x13: // hintmask
      case 0x14: // cntrmask
         if (in_header)
            maskbits += (sp / 2); // implicit "vstem"
         in_header = 0;
         stt__buf_skip(&b, (maskbits + 7) / 8);
         break;

      case 0x01: // hstem
      case 0x03: // vstem
      case 0x12: // hstemhm
      case 0x17: // vstemhm
         maskbits += (sp / 2);
         break;

      case 0x15: // rmoveto
         in_header = 0;
         if (sp < 2) return STT__CSERR("rmoveto stack");
         stt__csctx_rmove_to(c, s[sp-2], s[sp-1]);
         break;
      case 0x04: // vmoveto
         in_header = 0;
         if (sp < 1) return STT__CSERR("vmoveto stack");
         stt__csctx_rmove_to(c, 0, s[sp-1]);
         break;
      case 0x16: // hmoveto
         in_header = 0;
         if (sp < 1) return STT__CSERR("hmoveto stack");
         stt__csctx_rmove_to(c, s[sp-1], 0);
         break;

      case 0x05: // rlineto
         if (sp < 2) return STT__CSERR("rlineto stack");
         for (; i + 1 < sp; i += 2)
            stt__csctx_rline_to(c, s[i], s[i+1]);
         break;

      // hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical
      // starting from a different place.

      case 0x07: // vlineto
         if (sp < 1) return STT__CSERR("vlineto stack");
         goto vlineto;
      case 0x06: // hlineto
         if (sp < 1) return STT__CSERR("hlineto stack");
         for (;;) {
            if (i >= sp) break;
            stt__csctx_rline_to(c, s[i], 0);
            i++;
      vlineto:
            if (i >= sp) break;
            stt__csctx_rline_to(c, 0, s[i]);
            i++;
         }
         break;

      case 0x1F: // hvcurveto
         if (sp < 4) return STT__CSERR("hvcurveto stack");
         goto hvcurveto;
      case 0x1E: // vhcurveto
         if (sp < 4) return STT__CSERR("vhcurveto stack");
         for (;;) {
            if (i + 3 >= sp) break;
            stt__csctx_rccurve_to(c, 0, s[i], s[i+1], s[i+2], s[i+3], (sp - i == 5) ? s[i + 4] : 0.0f);
            i += 4;
      hvcurveto:
            if (i + 3 >= sp) break;
            stt__csctx_rccurve_to(c, s[i], 0, s[i+1], s[i+2], (sp - i == 5) ? s[i+4] : 0.0f, s[i+3]);
            i += 4;
         }
         break;

      case 0x08: // rrcurveto
         if (sp < 6) return STT__CSERR("rcurveline stack");
         for (; i + 5 < sp; i += 6)
            stt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x18: // rcurveline
         if (sp < 8) return STT__CSERR("rcurveline stack");
         for (; i + 5 < sp - 2; i += 6)
            stt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         if (i + 1 >= sp) return STT__CSERR("rcurveline stack");
         stt__csctx_rline_to(c, s[i], s[i+1]);
         break;

      case 0x19: // rlinecurve
         if (sp < 8) return STT__CSERR("rlinecurve stack");
         for (; i + 1 < sp - 6; i += 2)
            stt__csctx_rline_to(c, s[i], s[i+1]);
         if (i + 5 >= sp) return STT__CSERR("rlinecurve stack");
         stt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x1A: // vvcurveto
      case 0x1B: // hhcurveto
         if (sp < 4) return STT__CSERR("(vv|hh)curveto stack");
         f = 0.0;
         if (sp & 1) { f = s[i]; i++; }
         for (; i + 3 < sp; i += 4) {
            if (b0 == 0x1B)
               stt__csctx_rccurve_to(c, s[i], f, s[i+1], s[i+2], s[i+3], 0.0);
            else
               stt__csctx_rccurve_to(c, f, s[i], s[i+1], s[i+2], 0.0, s[i+3]);
            f = 0.0;
         }
         break;

      case 0x0A: // callsubr
         if (!has_subrs) {
            if (info->fdselect.size)
               subrs = stt__cid_get_glyph_subrs(info, glyph_index);
            has_subrs = 1;
         }
         // FALLTHROUGH
      case 0x1D: // callgsubr
         if (sp < 1) return STT__CSERR("call(g|)subr stack");
         v = (int) s[--sp];
         if (subr_stack_height >= 10) return STT__CSERR("recursion limit");
         subr_stack[subr_stack_height++] = b;
         b = stt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
         if (b.size == 0) return STT__CSERR("subr not found");
         b.cursor = 0;
         clear_stack = 0;
         break;

      case 0x0B: // return
         if (subr_stack_height <= 0) return STT__CSERR("return outside subr");
         b = subr_stack[--subr_stack_height];
         clear_stack = 0;
         break;

      case 0x0E: // endchar
         stt__csctx_close_shape(c);
         return !c->failed;

      case 0x0C: { // two-byte escape
         float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
         float dx, dy;
         int b1 = stt__buf_get8(&b);
         switch (b1) {
         // @TODO These "flex" implementations ignore the flex-depth and resolution,
         // and always draw beziers.
         case 0x22: // hflex
            if (sp < 7) return STT__CSERR("hflex stack");
            dx1 = s[0];
            dx2 = s[1];
            dy2 = s[2];
            dx3 = s[3];
            dx4 = s[4];
            dx5 = s[5];
            dx6 = s[6];
            stt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
            stt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
            break;

         case 0x23: // flex
            if (sp < 13) return STT__CSERR("flex stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = s[10];
            dy6 = s[11];
            //fd is s[12]
            stt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            stt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         case 0x24: // hflex1
            if (sp < 9) return STT__CSERR("hflex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dx4 = s[5];
            dx5 = s[6];
            dy5 = s[7];
            dx6 = s[8];
            stt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
            stt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1+dy2+dy5));
            break;

         case 0x25: // flex1
            if (sp < 11) return STT__CSERR("flex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = dy6 = s[10];
            dx = dx1+dx2+dx3+dx4+dx5;
            dy = dy1+dy2+dy3+dy4+dy5;
            if (STT_fabs(dx) > STT_fabs(dy))
               dy6 = -dy;
            else
               dx6 = -dx;
            stt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            stt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         default:
            return STT__CSERR("unimplemented");
         }
      } break;

      default:
         if (b0 != 255 && b0 != 28 && b0 < 32)
            return STT__CSERR("reserved operator");

         // push immediate
         if (b0 == 255) {
            f = (float)(stt_int32)stt__buf_get32(&b) / 0x10000;
         } else {
            stt__buf_skip(&b, -1);
            f = (float)(stt_int16)stt__cff_int(&b);
         }
         if (sp >= 48) return STT__CSERR("push stack overflow");
         s[sp++] = f;
         clear_stack = 0;
         break;
      }
      if (clear_stack) sp = 0;
   }
   return STT__CSERR("no endchar");

#undef STT__CSERR
}

static int stt__GetGlyphShapeT2(const stt_fontinfo *info, int glyph_index, stt_vertex **pvertices)
{
   // runs the charstring twice, once to count and once to output (to avoid realloc)
   stt__csctx count_ctx = STT__CSCTX_INIT(1);
   stt__csctx output_ctx = STT__CSCTX_INIT(0);
   if (!pvertices) return 0;
   *pvertices = NULL;
   if (stt__run_charstring(info, glyph_index, &count_ctx) &&
       count_ctx.num_vertices > 0 &&
       (size_t)count_ctx.num_vertices <= ((size_t)-1)/sizeof(stt_vertex)) {
      *pvertices = (stt_vertex*)STT_malloc(
         (size_t)count_ctx.num_vertices*sizeof(stt_vertex), info->userdata);
      if (!*pvertices) return 0;
      output_ctx.pvertices = *pvertices;
      output_ctx.capacity = count_ctx.num_vertices;
      if (stt__run_charstring(info, glyph_index, &output_ctx)) {
         if (!output_ctx.failed && output_ctx.num_vertices == count_ctx.num_vertices)
            return output_ctx.num_vertices;
      }
      STT_free(*pvertices, info->userdata);
   }
   *pvertices = NULL;
   return 0;
}

static int stt__GetGlyphInfoT2(const stt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   stt__csctx c = STT__CSCTX_INIT(1);
   int r = stt__run_charstring(info, glyph_index, &c);
   if (x0)  *x0 = r ? c.min_x : 0;
   if (y0)  *y0 = r ? c.min_y : 0;
   if (x1)  *x1 = r ? c.max_x : 0;
   if (y1)  *y1 = r ? c.max_y : 0;
   return r ? c.num_vertices : 0;
}

STT_DEF int stt_GetGlyphShape(const stt_fontinfo *info, int glyph_index, stt_vertex **pvertices)
{
   if (!pvertices) return 0;
   *pvertices = NULL;
   if (!info || glyph_index < 0 || glyph_index >= info->numGlyphs) return 0;
   if (!info->cff.size)
      return stt__GetGlyphShapeTT(info, glyph_index, pvertices, 0);
   else
      return stt__GetGlyphShapeT2(info, glyph_index, pvertices);
}

STT_DEF void stt_GetGlyphHMetrics(const stt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
   stt_uint16 numOfLongHorMetrics;
   stt_uint32 hmtx_size, metric_offset;
   if (advanceWidth) *advanceWidth = 0;
   if (leftSideBearing) *leftSideBearing = 0;
   if (!info || glyph_index < 0 || glyph_index >= info->numGlyphs) return;
   numOfLongHorMetrics = stt__safe_read16(info, info->hhea+34);
   hmtx_size = stt__get_table_size(info, info->fontstart, "hmtx");
   if (!numOfLongHorMetrics || !hmtx_size) return;
   if (glyph_index < numOfLongHorMetrics) {
      metric_offset = (stt_uint32)glyph_index*4;
      if (metric_offset > hmtx_size || hmtx_size-metric_offset < 4) return;
      if (advanceWidth) *advanceWidth = stt__safe_read16(info,
                                                          info->hmtx+metric_offset);
      if (leftSideBearing) *leftSideBearing = stt__safe_read16_signed(
         info, info->hmtx+metric_offset+2);
   } else {
      metric_offset = (stt_uint32)(numOfLongHorMetrics-1)*4;
      if (metric_offset > hmtx_size || hmtx_size-metric_offset < 2) return;
      if (advanceWidth) *advanceWidth = stt__safe_read16(info,
                                                          info->hmtx+metric_offset);
      metric_offset = (stt_uint32)numOfLongHorMetrics*4+
                      (stt_uint32)(glyph_index-numOfLongHorMetrics)*2;
      if (metric_offset > hmtx_size || hmtx_size-metric_offset < 2) return;
      if (leftSideBearing) *leftSideBearing = stt__safe_read16_signed(
         info, info->hmtx+metric_offset);
   }
}

STT_DEF int  stt_GetKerningTableLength(const stt_fontinfo *info)
{
   stt_uint32 size, pairs;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info || !info->kern)
      return 0;
   size = stt__get_table_size(info, info->fontstart, "kern");
   if (size < 18 || stt__safe_read16(info, info->kern+2) < 1)
      return 0;
   if (stt__safe_read16(info, info->kern+8) != 1)
      return 0;
   pairs = stt__safe_read16(info, info->kern+10);
   return pairs <= (size-18)/6 ? (int)pairs : 0;
}

STT_DEF int stt_GetKerningTable(const stt_fontinfo *info, stt_kerningentry* table, int table_length)
{
   int k, length = stt_GetKerningTableLength(info);

   if (!table || table_length <= 0 || length <= 0) return 0;
   if (table_length < length)
      length = table_length;

   for (k = 0; k < length; k++)
   {
      stt_uint32 entry = (stt_uint32)info->kern+18+(stt_uint32)k*6;
      table[k].glyph1 = stt__safe_read16(info, entry);
      table[k].glyph2 = stt__safe_read16(info, entry+2);
      table[k].advance = stt__safe_read16_signed(info, entry+4);
   }

   return length;
}

static int stt__GetGlyphKernInfoAdvance(const stt_fontinfo *info, int glyph1, int glyph2)
{
   stt_uint32 needle, straw;
   int l, r, m;
   int pair_count = stt_GetKerningTableLength(info);

   if (pair_count <= 0 || glyph1 < 0 || glyph2 < 0 ||
       glyph1 > 0xffff || glyph2 > 0xffff) return 0;
   l = 0;
   r = pair_count-1;
   needle = ((stt_uint32)glyph1 << 16) | (stt_uint32)glyph2;
   while (l <= r) {
      stt_uint32 entry;
      m = (l + r) >> 1;
      entry = (stt_uint32)info->kern+18+(stt_uint32)m*6;
      straw = stt__safe_read32(info, entry);
      if (needle < straw)
         r = m - 1;
      else if (needle > straw)
         l = m + 1;
      else
         return stt__safe_read16_signed(info, entry+4);
   }
   return 0;
}

static int stt__table_range(stt_uint32 base, stt_uint32 size,
                            stt_uint32 offset, stt_uint32 bytes)
{
   if (offset < base || offset-base > size) return 0;
   return bytes <= size-(offset-base);
}

static stt_int32 stt__GetCoverageIndex(const stt_fontinfo *info,
                                        stt_uint32 base, stt_uint32 size,
                                        stt_uint32 coverage, int glyph)
{
   stt_uint16 coverageFormat;
   if (!stt__table_range(base, size, coverage, 4)) return -1;
   coverageFormat = stt__safe_read16(info, coverage);
   switch (coverageFormat) {
      case 1: {
         stt_uint16 glyphCount = stt__safe_read16(info, coverage+2);

         // Binary search.
         stt_int32 l=0, r=glyphCount-1, m;
         int straw, needle=glyph;
         if (!stt__table_range(base, size, coverage, 4+(stt_uint32)glyphCount*2))
            return -1;
         while (l <= r) {
            stt_uint16 glyphID;
            m = (l + r) >> 1;
            glyphID = stt__safe_read16(info, coverage+4+2*(stt_uint32)m);
            straw = glyphID;
            if (needle < straw)
               r = m - 1;
            else if (needle > straw)
               l = m + 1;
            else {
               return m;
            }
         }
         break;
      }

      case 2: {
         stt_uint16 rangeCount = stt__safe_read16(info, coverage+2);

         // Binary search.
         stt_int32 l=0, r=rangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         if (!stt__table_range(base, size, coverage, 4+(stt_uint32)rangeCount*6))
            return -1;
         while (l <= r) {
            stt_uint32 rangeRecord;
            m = (l + r) >> 1;
            rangeRecord = coverage+4+6*(stt_uint32)m;
            strawStart = stt__safe_read16(info, rangeRecord);
            strawEnd = stt__safe_read16(info, rangeRecord+2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else {
               stt_uint16 startCoverageIndex = stt__safe_read16(info, rangeRecord+4);
               return startCoverageIndex + glyph - strawStart;
            }
         }
         break;
      }

      default: return -1; // unsupported
   }

   return -1;
}

static stt_int32 stt__GetGlyphClass(const stt_fontinfo *info,
                                     stt_uint32 base, stt_uint32 size,
                                     stt_uint32 class_def, int glyph)
{
   stt_uint16 classDefFormat;
   if (!stt__table_range(base, size, class_def, 4)) return -1;
   classDefFormat = stt__safe_read16(info, class_def);
   switch (classDefFormat)
   {
      case 1: {
         stt_uint16 startGlyphID, glyphCount;
         if (!stt__table_range(base, size, class_def, 6)) return -1;
         startGlyphID = stt__safe_read16(info, class_def+2);
         glyphCount = stt__safe_read16(info, class_def+4);
         if (!stt__table_range(base, size, class_def, 6+(stt_uint32)glyphCount*2))
            return -1;

         if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
            return stt__safe_read16(info, class_def+6+
                                    2*(stt_uint32)(glyph-startGlyphID));
         break;
      }

      case 2: {
         stt_uint16 classRangeCount = stt__safe_read16(info, class_def+2);

         // Binary search.
         stt_int32 l=0, r=classRangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         if (!stt__table_range(base, size, class_def,
                               4+(stt_uint32)classRangeCount*6)) return -1;
         while (l <= r) {
            stt_uint32 classRangeRecord;
            m = (l + r) >> 1;
            classRangeRecord = class_def+4+6*(stt_uint32)m;
            strawStart = stt__safe_read16(info, classRangeRecord);
            strawEnd = stt__safe_read16(info, classRangeRecord+2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
               return stt__safe_read16(info, classRangeRecord+4);
         }
         break;
      }

      default:
         return -1; // Unsupported definition type, return an error.
   }

   // "All glyphs not assigned to a class fall into class 0". (OpenType spec)
   return 0;
}

// Define to STT_assert(x) if you want to break on unimplemented formats.
#define STT_GPOS_TODO_assert(x)

static stt_int32 stt__GetGlyphGPOSInfoAdvance(const stt_fontinfo *info, int glyph1, int glyph2)
{
   stt_uint16 lookupListOffset, lookupCount;
   stt_uint32 size, lookupList;
   stt_int32 i, sti;

   if (!info->gpos) return 0;
   size = stt__get_table_size(info, info->fontstart, "GPOS");
   if (size < 10 || stt__safe_read16(info, info->gpos) != 1 ||
       stt__safe_read16(info, info->gpos+2) != 0) return 0;

   lookupListOffset = stt__safe_read16(info, info->gpos+8);
   if (lookupListOffset > size || size-lookupListOffset < 2) return 0;
   lookupList = (stt_uint32)info->gpos+lookupListOffset;
   lookupCount = stt__safe_read16(info, lookupList);
   if (!stt__table_range(info->gpos, size, lookupList,
                         2+(stt_uint32)lookupCount*2)) return 0;

   for (i=0; i<lookupCount; ++i) {
      stt_uint16 lookupOffset = stt__safe_read16(info, lookupList+2+2*(stt_uint32)i);
      stt_uint32 lookupTable = lookupList+lookupOffset;
      stt_uint16 lookupType, subTableCount;
      if (!stt__table_range(info->gpos, size, lookupTable, 6)) return 0;
      lookupType = stt__safe_read16(info, lookupTable);
      subTableCount = stt__safe_read16(info, lookupTable+4);
      if (!stt__table_range(info->gpos, size, lookupTable,
                            6+(stt_uint32)subTableCount*2)) return 0;
      if (lookupType != 2) // Pair Adjustment Positioning Subtable
         continue;

      for (sti=0; sti<subTableCount; sti++) {
         stt_uint16 subtableOffset = stt__safe_read16(info,
            lookupTable+6+2*(stt_uint32)sti);
         stt_uint32 table = lookupTable+subtableOffset;
         stt_uint16 posFormat, coverageOffset;
         stt_int32 coverageIndex;
         if (!stt__table_range(info->gpos, size, table, 4)) return 0;
         posFormat = stt__safe_read16(info, table);
         coverageOffset = stt__safe_read16(info, table+2);
         if (coverageOffset > size-(table-(stt_uint32)info->gpos)) return 0;
         coverageIndex = stt__GetCoverageIndex(info, info->gpos, size,
                                                table+coverageOffset, glyph1);
         if (coverageIndex == -1) continue;

         switch (posFormat) {
            case 1: {
               stt_int32 l, r, m;
               int straw, needle;
               stt_uint16 valueFormat1, valueFormat2, pairSetCount;
               if (!stt__table_range(info->gpos, size, table, 10)) return 0;
               valueFormat1 = stt__safe_read16(info, table+4);
               valueFormat2 = stt__safe_read16(info, table+6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
                  stt_uint16 pairPosOffset, pairValueCount;
                  stt_uint32 pairValueTable;
                  pairSetCount = stt__safe_read16(info, table+8);
                  if (coverageIndex >= pairSetCount) return 0;
                  if (!stt__table_range(info->gpos, size, table,
                        10+(stt_uint32)pairSetCount*2)) return 0;
                  pairPosOffset = stt__safe_read16(info,
                     table+10+2*(stt_uint32)coverageIndex);
                  pairValueTable = table+pairPosOffset;
                  if (!stt__table_range(info->gpos, size, pairValueTable, 2)) return 0;
                  pairValueCount = stt__safe_read16(info, pairValueTable);
                  if (!stt__table_range(info->gpos, size, pairValueTable,
                                        2+(stt_uint32)pairValueCount*4)) return 0;

                  needle=glyph2;
                  r=pairValueCount-1;
                  l=0;

                  // Binary search.
                  while (l <= r) {
                     stt_uint16 secondGlyph;
                     stt_uint32 pairValue;
                     m = (l + r) >> 1;
                     pairValue = pairValueTable+2+4*(stt_uint32)m;
                     secondGlyph = stt__safe_read16(info, pairValue);
                     straw = secondGlyph;
                     if (needle < straw)
                        r = m - 1;
                     else if (needle > straw)
                        l = m + 1;
                     else {
                        stt_int16 xAdvance = stt__safe_read16_signed(info, pairValue+2);
                        return xAdvance;
                     }
                  }
               } else
                  return 0;
               break;
            }

            case 2: {
               stt_uint16 valueFormat1, valueFormat2;
               if (!stt__table_range(info->gpos, size, table, 16)) return 0;
               valueFormat1 = stt__safe_read16(info, table+4);
               valueFormat2 = stt__safe_read16(info, table+6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
                  stt_uint16 classDef1Offset = stt__safe_read16(info, table+8);
                  stt_uint16 classDef2Offset = stt__safe_read16(info, table+10);
                  int glyph1class = stt__GetGlyphClass(info, info->gpos, size,
                                                       table+classDef1Offset, glyph1);
                  int glyph2class = stt__GetGlyphClass(info, info->gpos, size,
                                                       table+classDef2Offset, glyph2);
                  stt_uint16 class1Count = stt__safe_read16(info, table+12);
                  stt_uint16 class2Count = stt__safe_read16(info, table+14);
                  stt_uint32 record_index, record_count, table_relative;
                  stt_int16 xAdvance;

                  if (glyph1class < 0 || glyph1class >= class1Count) return 0; // malformed
                  if (glyph2class < 0 || glyph2class >= class2Count) return 0; // malformed
                  record_count = (stt_uint32)class1Count*class2Count;
                  table_relative = table-(stt_uint32)info->gpos;
                  if (table_relative > size || size-table_relative < 16 ||
                      record_count > (size-table_relative-16)/2) return 0;
                  record_index = (stt_uint32)glyph1class*class2Count+
                                 (stt_uint32)glyph2class;
                  xAdvance = stt__safe_read16_signed(info,
                                                     table+16+record_index*2);
                  return xAdvance;
               } else
                  return 0;
               break;
            }

            default:
               return 0; // Unsupported position format
         }
      }
   }

   return 0;
}

STT_DEF int  stt_GetGlyphKernAdvance(const stt_fontinfo *info, int g1, int g2)
{
   int xAdvance = 0;

   if (!info || g1 < 0 || g2 < 0 || g1 >= info->numGlyphs ||
       g2 >= info->numGlyphs) return 0;
   if (info->gpos)
      xAdvance += stt__GetGlyphGPOSInfoAdvance(info, g1, g2);
   else if (info->kern)
      xAdvance += stt__GetGlyphKernInfoAdvance(info, g1, g2);

   return xAdvance;
}

STT_DEF int  stt_GetCodepointKernAdvance(const stt_fontinfo *info, int ch1, int ch2)
{
   if (!info->kern && !info->gpos) // if no kerning table, don't waste time looking up both codepoint->glyphs
      return 0;
   return stt_GetGlyphKernAdvance(info, stt_FindGlyphIndex(info,ch1), stt_FindGlyphIndex(info,ch2));
}

STT_DEF void stt_GetCodepointHMetrics(const stt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
{
   stt_GetGlyphHMetrics(info, stt_FindGlyphIndex(info,codepoint), advanceWidth, leftSideBearing);
}

STT_DEF void stt_GetFontVMetrics(const stt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
   if (!info) {
      if (ascent) *ascent = 0;
      if (descent) *descent = 0;
      if (lineGap) *lineGap = 0;
      return;
   }
   if (ascent ) *ascent  = stt__safe_read16_signed(info, info->hhea + 4);
   if (descent) *descent = stt__safe_read16_signed(info, info->hhea + 6);
   if (lineGap) *lineGap = stt__safe_read16_signed(info, info->hhea + 8);
}

STT_DEF int  stt_GetFontVMetricsOS2(const stt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap)
{
   int tab;
   if (!info) return 0;
   tab = stt__find_table(info, info->fontstart, "OS/2");
   if (!tab || stt__get_table_size(info, info->fontstart, "OS/2") < 74)
      return 0;
   if (typoAscent ) *typoAscent  = stt__safe_read16_signed(info, tab+68);
   if (typoDescent) *typoDescent = stt__safe_read16_signed(info, tab+70);
   if (typoLineGap) *typoLineGap = stt__safe_read16_signed(info, tab+72);
   return 1;
}

STT_DEF void stt_GetFontBoundingBox(const stt_fontinfo *info, int *x0, int *y0, int *x1, int *y1)
{
   if (x0) *x0 = info ? stt__safe_read16_signed(info, info->head+36) : 0;
   if (y0) *y0 = info ? stt__safe_read16_signed(info, info->head+38) : 0;
   if (x1) *x1 = info ? stt__safe_read16_signed(info, info->head+40) : 0;
   if (y1) *y1 = info ? stt__safe_read16_signed(info, info->head+42) : 0;
}

STT_DEF float stt_ScaleForPixelHeight(const stt_fontinfo *info, float height)
{
   int fheight;
   if (!info) return 0.0f;
   fheight = stt__safe_read16_signed(info, info->hhea+4)-
             stt__safe_read16_signed(info, info->hhea+6);
   return fheight ? (float)height/fheight : 0.0f;
}

STT_DEF float stt_ScaleForMappingEmToPixels(const stt_fontinfo *info, float pixels)
{
   int unitsPerEm;
   if (!info) return 0.0f;
   unitsPerEm = stt__safe_read16(info, info->head+18);
   return unitsPerEm ? pixels/unitsPerEm : 0.0f;
}

STT_DEF void stt_FreeShape(const stt_fontinfo *info, stt_vertex *v)
{
   STT_free(v, info ? info->userdata : NULL);
}

STT_DEF stt_uint8 *stt_FindSVGDoc(const stt_fontinfo *info, int gl)
{
   int i, list;
   stt_uint32 table, size, relative;
   stt_uint16 numEntries;
   if (!info || gl < 0) return NULL;
   list = stt__get_svg((stt_fontinfo *)info);
   table = stt__find_table(info, info->fontstart, "SVG ");
   size = stt__get_table_size(info, info->fontstart, "SVG ");
   if (!list || !table || (stt_uint32)list < table) return NULL;
   relative = (stt_uint32)list-table;
   if (relative > size || size-relative < 2) return NULL;
   numEntries = stt__safe_read16(info, (stt_uint32)list);
   if ((stt_uint32)numEntries > (size-relative-2)/12) return NULL;

   for(i=0; i<numEntries; i++) {
      stt_uint32 svg_doc = (stt_uint32)list+2+12*(stt_uint32)i;
      if (gl >= stt__safe_read16(info, svg_doc) &&
          gl <= stt__safe_read16(info, svg_doc+2))
         return info->data+svg_doc;
   }
   return 0;
}

STT_DEF int stt_GetGlyphSVG(const stt_fontinfo *info, int gl, const char **svg)
{
   stt_uint8 *svg_doc;
   stt_uint32 table, size, offset, length, list_relative;

   if (svg) *svg = NULL;
   if (!info || !svg || gl < 0 || gl >= info->numGlyphs ||
       stt__get_svg((stt_fontinfo *)info) == 0)
      return 0;

   svg_doc = stt_FindSVGDoc(info, gl);
   if (svg_doc != NULL) {
      stt_uint32 record = (stt_uint32)(svg_doc-info->data);
      table = stt__find_table(info, info->fontstart, "SVG ");
      size = stt__get_table_size(info, info->fontstart, "SVG ");
      if (!table || info->svg < (int)table) return 0;
      list_relative = (stt_uint32)info->svg-table;
      offset = stt__safe_read32(info, record+4);
      length = stt__safe_read32(info, record+8);
      if (list_relative > size || offset > size-list_relative ||
          length > size-list_relative-offset || length > 0x7fffffff)
         return 0;
      *svg = (const char *)info->data+info->svg+offset;
      return (int)length;
   } else {
      return 0;
   }
}

STT_DEF int stt_GetCodepointSVG(const stt_fontinfo *info, int unicode_codepoint, const char **svg)
{
   return stt_GetGlyphSVG(info, stt_FindGlyphIndex(info, unicode_codepoint), svg);
}

//////////////////////////////////////////////////////////////////////////////
//
// antialiasing software rasterizer
//

STT_DEF void stt_GetGlyphBitmapBoxSubpixel(const stt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   int x0=0,y0=0,x1,y1; // =0 suppresses compiler warning
   if (!stt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1)) {
      // e.g. space character
      if (ix0) *ix0 = 0;
      if (iy0) *iy0 = 0;
      if (ix1) *ix1 = 0;
      if (iy1) *iy1 = 0;
   } else {
      float fx0 = x0*scale_x+shift_x;
      float fy0 = -y1*scale_y+shift_y;
      float fx1 = x1*scale_x+shift_x;
      float fy1 = -y0*scale_y+shift_y;
      if (!(fx0 >= -2147483000.0f && fx0 <= 2147483000.0f) ||
          !(fy0 >= -2147483000.0f && fy0 <= 2147483000.0f) ||
          !(fx1 >= -2147483000.0f && fx1 <= 2147483000.0f) ||
          !(fy1 >= -2147483000.0f && fy1 <= 2147483000.0f)) {
         if (ix0) *ix0 = 0;
         if (iy0) *iy0 = 0;
         if (ix1) *ix1 = 0;
         if (iy1) *iy1 = 0;
         return;
      }
      // move to integral bboxes (treating pixels as little squares, what pixels get touched)?
      if (ix0) *ix0 = STT_ifloor(fx0);
      if (iy0) *iy0 = STT_ifloor(fy0);
      if (ix1) *ix1 = STT_iceil(fx1);
      if (iy1) *iy1 = STT_iceil(fy1);
   }
}

STT_DEF void stt_GetGlyphBitmapBox(const stt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y,0.0f,0.0f, ix0, iy0, ix1, iy1);
}

STT_DEF void stt_GetCodepointBitmapBoxSubpixel(const stt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stt_GetGlyphBitmapBoxSubpixel(font, stt_FindGlyphIndex(font,codepoint), scale_x, scale_y,shift_x,shift_y, ix0,iy0,ix1,iy1);
}

STT_DEF void stt_GetCodepointBitmapBox(const stt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stt_GetCodepointBitmapBoxSubpixel(font, codepoint, scale_x, scale_y,0.0f,0.0f, ix0,iy0,ix1,iy1);
}

//////////////////////////////////////////////////////////////////////////////
//
//  Rasterizer

typedef struct stt__hheap_chunk
{
   struct stt__hheap_chunk *next;
} stt__hheap_chunk;

typedef struct stt__hheap
{
   struct stt__hheap_chunk *head;
   void   *first_free;
   int    num_remaining_in_head_chunk;
} stt__hheap;

static void *stt__hheap_alloc(stt__hheap *hh, size_t size, void *userdata)
{
   if (!hh || size < sizeof(void *)) return NULL;
   if (hh->first_free) {
      void *p = hh->first_free;
      hh->first_free = * (void **) p;
      return p;
   } else {
      if (hh->num_remaining_in_head_chunk == 0) {
         int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
         stt__hheap_chunk *c;
         if (size > (((size_t)-1)-sizeof(stt__hheap_chunk)) /
                    (size_t)count)
            return NULL;
         c = (stt__hheap_chunk *)STT_malloc(
            sizeof(stt__hheap_chunk)+size*(size_t)count, userdata);
         if (c == NULL)
            return NULL;
         c->next = hh->head;
         hh->head = c;
         hh->num_remaining_in_head_chunk = count;
      }
      --hh->num_remaining_in_head_chunk;
      return (char *) (hh->head) + sizeof(stt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
   }
}

static void stt__hheap_free(stt__hheap *hh, void *p)
{
   if (!hh || !p) return;
   *(void **) p = hh->first_free;
   hh->first_free = p;
}

static void stt__hheap_cleanup(stt__hheap *hh, void *userdata)
{
   if (!hh) return;
   stt__hheap_chunk *c = hh->head;
   while (c) {
      stt__hheap_chunk *n = c->next;
      STT_free(c, userdata);
      c = n;
   }
}

typedef struct stt__edge {
   float x0,y0, x1,y1;
   int invert;
} stt__edge;


typedef struct stt__active_edge
{
   struct stt__active_edge *next;
   #if STT_RASTERIZER_VERSION==1
   int x,dx;
   float ey;
   int direction;
   #elif STT_RASTERIZER_VERSION==2
   float fx,fdx,fdy;
   float direction;
   float sy;
   float ey;
   #else
   #error "Unrecognized value of STT_RASTERIZER_VERSION"
   #endif
} stt__active_edge;

#if STT_RASTERIZER_VERSION == 1
#define STT_FIXSHIFT   10
#define STT_FIX        (1 << STT_FIXSHIFT)
#define STT_FIXMASK    (STT_FIX-1)

static stt__active_edge *stt__new_active(stt__hheap *hh, stt__edge *e, int off_x, float start_point, void *userdata)
{
   stt__active_edge *z = (stt__active_edge *) stt__hheap_alloc(hh, sizeof(*z), userdata);
   float dxdy, x;
   if (!z) return z;
   if (e->y1 == e->y0) {
      stt__hheap_free(hh, z);
      return NULL;
   }
   dxdy = (e->x1-e->x0)/(e->y1-e->y0);
   if (!(dxdy >= -2097151.0f && dxdy <= 2097151.0f)) {
      stt__hheap_free(hh, z);
      return NULL;
   }

   // round dx down to avoid overshooting
   if (dxdy < 0)
      z->dx = -STT_ifloor(STT_FIX * -dxdy);
   else
      z->dx = STT_ifloor(STT_FIX * dxdy);

   x = STT_FIX*e->x0+z->dx*(start_point-e->y0)-
       (float)off_x*STT_FIX;
   if (!(x >= -2147483000.0f && x <= 2147483000.0f)) {
      stt__hheap_free(hh, z);
      return NULL;
   }
   z->x = STT_ifloor(x);

   z->ey = e->y1;
   z->next = 0;
   z->direction = e->invert ? 1 : -1;
   return z;
}
#elif STT_RASTERIZER_VERSION == 2
static stt__active_edge *stt__new_active(stt__hheap *hh, stt__edge *e, int off_x, float start_point, void *userdata)
{
   stt__active_edge *z = (stt__active_edge *) stt__hheap_alloc(hh, sizeof(*z), userdata);
   float dxdy;
   //STT_assert(e->y0 <= start_point);
   if (!z) return z;
   if (e->y1 == e->y0) {
      stt__hheap_free(hh, z);
      return NULL;
   }
   dxdy = (e->x1-e->x0)/(e->y1-e->y0);
   if (!(dxdy >= -3.4e38f && dxdy <= 3.4e38f)) {
      stt__hheap_free(hh, z);
      return NULL;
   }
   z->fdx = dxdy;
   z->fdy = dxdy != 0.0f ? (1.0f/dxdy) : 0.0f;
   z->fx = e->x0 + dxdy * (start_point - e->y0);
   z->fx -= off_x;
   z->direction = e->invert ? 1.0f : -1.0f;
   z->sy = e->y0;
   z->ey = e->y1;
   z->next = 0;
   return z;
}
#else
#error "Unrecognized value of STT_RASTERIZER_VERSION"
#endif

#if STT_RASTERIZER_VERSION == 1
// note: this routine clips fills that extend off the edges... ideally this
// wouldn't happen, but it could happen if the truetype glyph bounding boxes
// are wrong, or if the user supplies a too-small bitmap
static void stt__fill_active_edges(unsigned char *scanline, int len, stt__active_edge *e, int max_weight)
{
   // non-zero winding fill
   int x0=0, w=0;

   while (e) {
      if (w == 0) {
         // if we're currently at zero, we need to record the edge start point
         x0 = e->x; w += e->direction;
      } else {
         int x1 = e->x; w += e->direction;
         // if we went to zero, we need to draw
         if (w == 0) {
            int i = x0 >> STT_FIXSHIFT;
            int j = x1 >> STT_FIXSHIFT;

            if (i < len && j >= 0) {
               if (i == j) {
                  // x0,x1 are the same pixel, so compute combined coverage
                  scanline[i] = scanline[i] + (stt_uint8) ((x1 - x0) * max_weight >> STT_FIXSHIFT);
               } else {
                  if (i >= 0) // add antialiasing for x0
                     scanline[i] = scanline[i] + (stt_uint8) (((STT_FIX - (x0 & STT_FIXMASK)) * max_weight) >> STT_FIXSHIFT);
                  else
                     i = -1; // clip

                  if (j < len) // add antialiasing for x1
                     scanline[j] = scanline[j] + (stt_uint8) (((x1 & STT_FIXMASK) * max_weight) >> STT_FIXSHIFT);
                  else
                     j = len; // clip

                  for (++i; i < j; ++i) // fill pixels between x0 and x1
                     scanline[i] = scanline[i] + (stt_uint8) max_weight;
               }
            }
         }
      }

      e = e->next;
   }
}

static void stt__rasterize_sorted_edges(stt__bitmap *result, stt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stt__hheap hh = { 0, 0, 0 };
   stt__active_edge *active = NULL;
   float y;
   int j=0;
   int max_weight = (255 / vsubsample);  // weight per vertical scanline
   int s; // vertical subsample index
   unsigned char scanline_data[512], *scanline;

   if (result->w > 512)
      scanline = (unsigned char *)STT_malloc((size_t)result->w, userdata);
   else
      scanline = scanline_data;
   if (!scanline) return;

   y = (float)off_y*(float)vsubsample;
   e[n].y0 = ((float)off_y+(float)result->h)*(float)vsubsample+1;

   while (j < result->h) {
      STT_memset(scanline, 0, result->w);
      for (s=0; s < vsubsample; ++s) {
         // find center of pixel for this scanline
         float scan_y = y+0.5f;
         stt__active_edge **step = &active;

         // update all active edges;
         // remove all active edges that terminate before the center of this scanline
         while (*step) {
            stt__active_edge * z = *step;
            if (z->ey <= scan_y) {
               *step = z->next; // delete from list
               STT_assert(z->direction);
               z->direction = 0;
               stt__hheap_free(&hh, z);
            } else {
               z->x += z->dx; // advance to position for current scanline
               step = &((*step)->next); // advance through list
            }
         }

         // resort the list if needed
         for(;;) {
            int changed=0;
            step = &active;
            while (*step && (*step)->next) {
               if ((*step)->x > (*step)->next->x) {
                  stt__active_edge *t = *step;
                  stt__active_edge *q = t->next;

                  t->next = q->next;
                  q->next = t;
                  *step = q;
                  changed = 1;
               }
               step = &(*step)->next;
            }
            if (!changed) break;
         }

         // insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
         while (e->y0 <= scan_y) {
            if (e->y1 > scan_y) {
               stt__active_edge *z = stt__new_active(&hh, e, off_x, scan_y, userdata);
               if (z != NULL) {
                  // find insertion point
                  if (active == NULL)
                     active = z;
                  else if (z->x < active->x) {
                     // insert at front
                     z->next = active;
                     active = z;
                  } else {
                     // find thing to insert AFTER
                     stt__active_edge *p = active;
                     while (p->next && p->next->x < z->x)
                        p = p->next;
                     // at this point, p->next->x is NOT < z->x
                     z->next = p->next;
                     p->next = z;
                  }
               }
            }
            ++e;
         }

         // now process all active edges in XOR fashion
         if (active)
            stt__fill_active_edges(scanline, result->w, active, max_weight);

         ++y;
      }
      STT_memcpy(result->pixels+(size_t)j*(size_t)result->stride,
                 scanline, (size_t)result->w);
      ++j;
   }

   stt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      STT_free(scanline, userdata);
}

#elif STT_RASTERIZER_VERSION == 2

// the edge passed in here does not cross the vertical line at x or the vertical line at x+1
// (i.e. it has already been clipped to those)
static void stt__handle_clipped_edge(float *scanline, int x, stt__active_edge *e, float x0, float y0, float x1, float y1)
{
   if (y0 == y1) return;
   STT_assert(y0 < y1);
   STT_assert(e->sy <= e->ey);
   if (y0 > e->ey) return;
   if (y1 < e->sy) return;
   if (y0 < e->sy) {
      x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
      y0 = e->sy;
   }
   if (y1 > e->ey) {
      x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
      y1 = e->ey;
   }

   if (x0 == x)
      STT_assert(x1 <= x+1);
   else if (x0 == x+1)
      STT_assert(x1 >= x);
   else if (x0 <= x)
      STT_assert(x1 <= x);
   else if (x0 >= x+1)
      STT_assert(x1 >= x+1);
   else
      STT_assert(x1 >= x && x1 <= x+1);

   if (x0 <= x && x1 <= x)
      scanline[x] += e->direction * (y1-y0);
   else if (x0 >= x+1 && x1 >= x+1)
      ;
   else {
      STT_assert(x0 >= x && x0 <= x+1 && x1 >= x && x1 <= x+1);
      scanline[x] += e->direction * (y1-y0) * (1-((x0-x)+(x1-x))/2); // coverage = 1 - average x position
   }
}

static float stt__sized_trapezoid_area(float height, float top_width, float bottom_width)
{
   STT_assert(top_width >= 0);
   STT_assert(bottom_width >= 0);
   return (top_width + bottom_width) / 2.0f * height;
}

static float stt__position_trapezoid_area(float height, float tx0, float tx1, float bx0, float bx1)
{
   return stt__sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

static float stt__sized_triangle_area(float height, float width)
{
   return height * width / 2;
}

static void stt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stt__active_edge *e, float y_top)
{
   float y_bottom = y_top+1;

   while (e) {
      // brute force every pixel

      // compute intersection points with top & bottom
      STT_assert(e->ey >= y_top);

      if (e->fdx == 0) {
         float x0 = e->fx;
         if (x0 < len) {
            if (x0 >= 0) {
               stt__handle_clipped_edge(scanline,(int) x0,e, x0,y_top, x0,y_bottom);
               stt__handle_clipped_edge(scanline_fill-1,(int) x0+1,e, x0,y_top, x0,y_bottom);
            } else {
               stt__handle_clipped_edge(scanline_fill-1,0,e, x0,y_top, x0,y_bottom);
            }
         }
      } else {
         float x0 = e->fx;
         float dx = e->fdx;
         float xb = x0 + dx;
         float x_top, x_bottom;
         float sy0,sy1;
         float dy = e->fdy;
         STT_assert(e->sy <= y_bottom && e->ey >= y_top);

         // compute endpoints of line segment clipped to this scanline (if the
         // line segment starts on this scanline. x0 is the intersection of the
         // line with y_top, but that may be off the line segment.
         if (e->sy > y_top) {
            x_top = x0 + dx * (e->sy - y_top);
            sy0 = e->sy;
         } else {
            x_top = x0;
            sy0 = y_top;
         }
         if (e->ey < y_bottom) {
            x_bottom = x0 + dx * (e->ey - y_top);
            sy1 = e->ey;
         } else {
            x_bottom = xb;
            sy1 = y_bottom;
         }

         if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
            // from here on, we don't have to range check x values

            if ((int) x_top == (int) x_bottom) {
               float height;
               // simple case, only spans one pixel
               int x = (int) x_top;
               height = (sy1 - sy0) * e->direction;
               STT_assert(x >= 0 && x < len);
               scanline[x]      += stt__position_trapezoid_area(height, x_top, x+1.0f, x_bottom, x+1.0f);
               scanline_fill[x] += height; // everything right of this pixel is filled
            } else {
               int x,x1,x2;
               float y_crossing, y_final, step, sign, area;
               // covers 2+ pixels
               if (x_top > x_bottom) {
                  // flip scanline vertically; signed area is the same
                  float t;
                  sy0 = y_bottom - (sy0 - y_top);
                  sy1 = y_bottom - (sy1 - y_top);
                  t = sy0, sy0 = sy1, sy1 = t;
                  t = x_bottom, x_bottom = x_top, x_top = t;
                  dx = -dx;
                  dy = -dy;
                  t = x0, x0 = xb, xb = t;
               }
               STT_assert(dy >= 0);
               STT_assert(dx >= 0);

               x1 = (int) x_top;
               x2 = (int) x_bottom;
               // compute intersection with y axis at x1+1
               y_crossing = y_top + dy * (x1+1 - x0);

               // compute intersection with y axis at x2
               y_final = y_top + dy * (x2 - x0);

               //           x1    x_top                            x2    x_bottom
               //     y_top  +------|-----+------------+------------+--------|---+------------+
               //            |            |            |            |            |            |
               //            |            |            |            |            |            |
               //       sy0  |      Txxxxx|............|............|............|............|
               // y_crossing |            *xxxxx.......|............|............|............|
               //            |            |     xxxxx..|............|............|............|
               //            |            |     /-   xx*xxxx........|............|............|
               //            |            | dy <       |    xxxxxx..|............|............|
               //   y_final  |            |     \-     |          xx*xxx.........|............|
               //       sy1  |            |            |            |   xxxxxB...|............|
               //            |            |            |            |            |            |
               //            |            |            |            |            |            |
               //  y_bottom  +------------+------------+------------+------------+------------+
               //
               // goal is to measure the area covered by '.' in each pixel

               // if x2 is right at the right edge of x1, y_crossing can blow up, github #1057
               // @TODO: maybe test against sy1 rather than y_bottom?
               if (y_crossing > y_bottom)
                  y_crossing = y_bottom;

               sign = e->direction;

               // area of the rectangle covered from sy0..y_crossing
               area = sign * (y_crossing-sy0);

               // area of the triangle (x_top,sy0), (x1+1,sy0), (x1+1,y_crossing)
               scanline[x1] += stt__sized_triangle_area(area, x1+1 - x_top);

               // check if final y_crossing is blown up; no test case for this
               if (y_final > y_bottom) {
                  y_final = y_bottom;
                  dy = (y_final - y_crossing ) / (x2 - (x1+1)); // if denom=0, y_final = y_crossing, so y_final <= y_bottom
               }

               // in second pixel, area covered by line segment found in first pixel
               // is always a rectangle 1 wide * the height of that line segment; this
               // is exactly what the variable 'area' stores. it also gets a contribution
               // from the line segment within it. the THIRD pixel will get the first
               // pixel's rectangle contribution, the second pixel's rectangle contribution,
               // and its own contribution. the 'own contribution' is the same in every pixel except
               // the leftmost and rightmost, a trapezoid that slides down in each pixel.
               // the second pixel's contribution to the third pixel will be the
               // rectangle 1 wide times the height change in the second pixel, which is dy.

               step = sign * dy * 1; // dy is dy/dx, change in y for every 1 change in x,
               // which multiplied by 1-pixel-width is how much pixel area changes for each step in x
               // so the area advances by 'step' every time

               for (x = x1+1; x < x2; ++x) {
                  scanline[x] += area + step/2; // area of trapezoid is 1*step/2
                  area += step;
               }
               STT_assert(STT_fabs(area) <= 1.01f); // accumulated error from area += step unless we round step down
               STT_assert(sy1 > y_final-0.01f);

               // area covered in the last pixel is the rectangle from all the pixels to the left,
               // plus the trapezoid filled by the line segment in this pixel all the way to the right edge
               scanline[x2] += area + sign * stt__position_trapezoid_area(sy1-y_final, (float) x2, x2+1.0f, x_bottom, x2+1.0f);

               // the rest of the line is filled based on the total height of the line segment in this pixel
               scanline_fill[x2] += sign * (sy1-sy0);
            }
         } else {
            // if edge goes outside of box we're drawing, we require
            // clipping logic. since this does not match the intended use
            // of this library, we use a different, very slow brute
            // force implementation
            // note though that this does happen some of the time because
            // x_top and x_bottom can be extrapolated at the top & bottom of
            // the shape and actually lie outside the bounding box
            int x;
            for (x=0; x < len; ++x) {
               // cases:
               //
               // there can be up to two intersections with the pixel. any intersection
               // with left or right edges can be handled by splitting into two (or three)
               // regions. intersections with top & bottom do not necessitate case-wise logic.
               //
               // the old way of doing this found the intersections with the left & right edges,
               // then used some simple logic to produce up to three segments in sorted order
               // from top-to-bottom. however, this had a problem: if an x edge was epsilon
               // across the x border, then the corresponding y position might not be distinct
               // from the other y segment, and it might ignored as an empty segment. to avoid
               // that, we need to explicitly produce segments based on x positions.

               // rename variables to clearly-defined pairs
               float y0 = y_top;
               float x1 = (float) (x);
               float x2 = (float) (x+1);
               float x3 = xb;
               float y3 = y_bottom;

               // x = e->x + e->dx * (y-y_top)
               // (y-y_top) = (x - e->x) / e->dx
               // y = (x - e->x) / e->dx + y_top
               float y1 = (x - x0) / dx + y_top;
               float y2 = (x+1 - x0) / dx + y_top;

               if (x0 < x1 && x3 > x2) {         // three segments descending down-right
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stt__handle_clipped_edge(scanline,x,e, x1,y1, x2,y2);
                  stt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else if (x3 < x1 && x0 > x2) {  // three segments descending down-left
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stt__handle_clipped_edge(scanline,x,e, x2,y2, x1,y1);
                  stt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x0 < x1 && x3 > x1) {  // two segments across x, down-right
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x3 < x1 && x0 > x1) {  // two segments across x, down-left
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x0 < x2 && x3 > x2) {  // two segments across x+1, down-right
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else if (x3 < x2 && x0 > x2) {  // two segments across x+1, down-left
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else {  // one segment
                  stt__handle_clipped_edge(scanline,x,e, x0,y0, x3,y3);
               }
            }
         }
      }
      e = e->next;
   }
}

// directly AA rasterize edges w/o supersampling
static void stt__rasterize_sorted_edges(stt__bitmap *result, stt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stt__hheap hh = { 0, 0, 0 };
   stt__active_edge *active = NULL;
   float y;
   int j=0, i;
   float scanline_data[129], *scanline, *scanline2;

   STT__NOTUSED(vsubsample);

   if (result->w > 64) {
      size_t scan_count = (size_t)result->w*2+1;
      if (scan_count > ((size_t)-1)/sizeof(float)) return;
      scanline = (float *)STT_malloc(scan_count*sizeof(float), userdata);
   }
   else
      scanline = scanline_data;
   if (!scanline) return;

   scanline2 = scanline + result->w;

   y = (float)off_y;
   e[n].y0 = (float)off_y+(float)result->h+1;

   while (j < result->h) {
      // find center of pixel for this scanline
      float scan_y_top    = y + 0.0f;
      float scan_y_bottom = y + 1.0f;
      stt__active_edge **step = &active;

      STT_memset(scanline , 0, result->w*sizeof(scanline[0]));
      STT_memset(scanline2, 0, (result->w+1)*sizeof(scanline[0]));

      // update all active edges;
      // remove all active edges that terminate before the top of this scanline
      while (*step) {
         stt__active_edge * z = *step;
         if (z->ey <= scan_y_top) {
            *step = z->next; // delete from list
            STT_assert(z->direction);
            z->direction = 0;
            stt__hheap_free(&hh, z);
         } else {
            step = &((*step)->next); // advance through list
         }
      }

      // insert all edges that start before the bottom of this scanline
      while (e->y0 <= scan_y_bottom) {
         if (e->y0 != e->y1) {
            stt__active_edge *z = stt__new_active(&hh, e, off_x, scan_y_top, userdata);
            if (z != NULL) {
               if (j == 0 && off_y != 0) {
                  if (z->ey < scan_y_top) {
                     // this can happen due to subpixel positioning and some kind of fp rounding error i think
                     z->ey = scan_y_top;
                  }
               }
               STT_assert(z->ey >= scan_y_top); // if we get really unlucky a tiny bit of an edge can be out of bounds
               // insert at front
               z->next = active;
               active = z;
            }
         }
         ++e;
      }

      // now process all active edges
      if (active)
         stt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);

      {
         float sum = 0;
         for (i=0; i < result->w; ++i) {
            float k;
            int m;
            sum += scanline2[i];
            k = scanline[i] + sum;
            k = (float) STT_fabs(k)*255 + 0.5f;
            if (!(k >= 0.0f))
               m = 0;
            else if (k >= 255.0f)
               m = 255;
            else
               m = (int)k;
            result->pixels[(size_t)j*(size_t)result->stride+(size_t)i] =
               (unsigned char)m;
         }
      }
      // advance all the edges
      step = &active;
      while (*step) {
         stt__active_edge *z = *step;
         z->fx += z->fdx; // advance to position for current scanline
         step = &((*step)->next); // advance through list
      }

      ++y;
      ++j;
   }

   stt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      STT_free(scanline, userdata);
}
#else
#error "Unrecognized value of STT_RASTERIZER_VERSION"
#endif

#define STT__COMPARE(a,b)  ((a)->y0 < (b)->y0)

static void stt__sort_edges_ins_sort(stt__edge *p, int n)
{
   int i,j;
   for (i=1; i < n; ++i) {
      stt__edge t = p[i], *a = &t;
      j = i;
      while (j > 0) {
         stt__edge *b = &p[j-1];
         int c = STT__COMPARE(a,b);
         if (!c) break;
         p[j] = p[j-1];
         --j;
      }
      if (i != j)
         p[j] = t;
   }
}

static void stt__sort_edges_quicksort(stt__edge *p, int n)
{
   /* threshold for transitioning to insertion sort */
   while (n > 12) {
      stt__edge t;
      int c01,c12,c,m,i,j;

      /* compute median of three */
      m = n >> 1;
      c01 = STT__COMPARE(&p[0],&p[m]);
      c12 = STT__COMPARE(&p[m],&p[n-1]);
      /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
      if (c01 != c12) {
         /* otherwise, we'll need to swap something else to middle */
         int z;
         c = STT__COMPARE(&p[0],&p[n-1]);
         /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
         /* 0<mid && mid>n:  0>n => 0; 0<n => n */
         z = (c == c12) ? 0 : n-1;
         t = p[z];
         p[z] = p[m];
         p[m] = t;
      }
      /* now p[m] is the median-of-three */
      /* swap it to the beginning so it won't move around */
      t = p[0];
      p[0] = p[m];
      p[m] = t;

      /* partition loop */
      i=1;
      j=n-1;
      for(;;) {
         /* handling of equality is crucial here */
         /* for sentinels & efficiency with duplicates */
         for (;;++i) {
            if (!STT__COMPARE(&p[i], &p[0])) break;
         }
         for (;;--j) {
            if (!STT__COMPARE(&p[0], &p[j])) break;
         }
         /* make sure we haven't crossed */
         if (i >= j) break;
         t = p[i];
         p[i] = p[j];
         p[j] = t;

         ++i;
         --j;
      }
      /* recurse on smaller side, iterate on larger */
      if (j < (n-i)) {
         stt__sort_edges_quicksort(p,j);
         p = p+i;
         n = n-i;
      } else {
         stt__sort_edges_quicksort(p+i, n-i);
         n = j;
      }
   }
}

static void stt__sort_edges(stt__edge *p, int n)
{
   stt__sort_edges_quicksort(p, n);
   stt__sort_edges_ins_sort(p, n);
}

typedef struct
{
   float x,y;
} stt__point;

static void stt__rasterize(stt__bitmap *result, stt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata)
{
   float y_scale_inv = invert ? -scale_y : scale_y;
   stt__edge *e;
   int n,i,j,k,m;
#if STT_RASTERIZER_VERSION == 1
   int vsubsample = result->h < 8 ? 15 : 5;
#elif STT_RASTERIZER_VERSION == 2
   int vsubsample = 1;
#else
   #error "Unrecognized value of STT_RASTERIZER_VERSION"
#endif
   // vsubsample should divide 255 evenly; otherwise we won't reach full opacity

   // now we have to blow out the windings into explicit edge lists
   n = 0;
   if (!result || !pts || !wcount || windings <= 0) return;
   for (i=0; i < windings; ++i) {
      if (wcount[i] < 0 || wcount[i] > STT_MAX_TESSELLATION_POINTS-n)
         return;
      n += wcount[i];
   }
   if (n <= 0 || n > STT_MAX_TESSELLATION_POINTS ||
       (size_t)n+1 > ((size_t)-1)/sizeof(*e))
      return;

   e = (stt__edge *)STT_malloc(sizeof(*e)*((size_t)n+1), userdata);
   if (e == 0) return;
   n = 0;

   m=0;
   for (i=0; i < windings; ++i) {
      stt__point *p = pts + m;
      m += wcount[i];
      j = wcount[i]-1;
      for (k=0; k < wcount[i]; j=k++) {
         int a=k,b=j;
         // skip the edge if horizontal
         if (p[j].y == p[k].y)
            continue;
         // add edge from j to k to the list
         e[n].invert = 0;
         if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
            e[n].invert = 1;
            a=j,b=k;
         }
         e[n].x0 = p[a].x * scale_x + shift_x;
         e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
         e[n].x1 = p[b].x * scale_x + shift_x;
         e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
         ++n;
      }
   }

   // now sort the edges by their highest point (should snap to integer, and then by x)
   //STT_sort(e, n, sizeof(e[0]), stt__edge_compare);
   stt__sort_edges(e, n);

   // now, traverse the scanlines and find the intersections on each scanline, use xor winding rule
   stt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

   STT_free(e, userdata);
}

static void stt__add_point(stt__point *points, int n, float x, float y)
{
   if (!points) return; // during first pass, it's unallocated
   points[n].x = x;
   points[n].y = y;
}

// tessellate until threshold p is happy... @TODO warped to compensate for non-linear stretching
static int stt__tesselate_curve(stt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
   // midpoint
   float mx = (x0 + 2*x1 + x2)/4;
   float my = (y0 + 2*y1 + y2)/4;
   // versus directly drawn line
   float dx = (x0+x2)/2 - mx;
   float dy = (y0+y2)/2 - my;
   if (*num_points >= STT_MAX_TESSELLATION_POINTS) return 0;
   if (n > 16) {
      stt__add_point(points, *num_points, x2, y2);
      *num_points = *num_points+1;
      return 1;
   }
   if (dx*dx+dy*dy > objspace_flatness_squared) { // half-pixel error allowed... need to be smaller if AA
      if (!stt__tesselate_curve(points, num_points, x0,y0,
             (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my,
             objspace_flatness_squared,n+1)) return 0;
      if (!stt__tesselate_curve(points, num_points, mx,my,
             (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2,
             objspace_flatness_squared,n+1)) return 0;
   } else {
      stt__add_point(points, *num_points,x2,y2);
      *num_points = *num_points+1;
   }
   return 1;
}

static int stt__tesselate_cubic(stt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float objspace_flatness_squared, int n)
{
   // @TODO this "flatness" calculation is just made-up nonsense that seems to work well enough
   float dx0 = x1-x0;
   float dy0 = y1-y0;
   float dx1 = x2-x1;
   float dy1 = y2-y1;
   float dx2 = x3-x2;
   float dy2 = y3-y2;
   float dx = x3-x0;
   float dy = y3-y0;
   float longlen = (float) (STT_sqrt(dx0*dx0+dy0*dy0)+STT_sqrt(dx1*dx1+dy1*dy1)+STT_sqrt(dx2*dx2+dy2*dy2));
   float shortlen = (float) STT_sqrt(dx*dx+dy*dy);
   float flatness_squared = longlen*longlen-shortlen*shortlen;

   if (*num_points >= STT_MAX_TESSELLATION_POINTS) return 0;
   if (n > 16) {
      stt__add_point(points, *num_points, x3, y3);
      *num_points = *num_points+1;
      return 1;
   }

   if (flatness_squared > objspace_flatness_squared) {
      float x01 = (x0+x1)/2;
      float y01 = (y0+y1)/2;
      float x12 = (x1+x2)/2;
      float y12 = (y1+y2)/2;
      float x23 = (x2+x3)/2;
      float y23 = (y2+y3)/2;

      float xa = (x01+x12)/2;
      float ya = (y01+y12)/2;
      float xb = (x12+x23)/2;
      float yb = (y12+y23)/2;

      float mx = (xa+xb)/2;
      float my = (ya+yb)/2;

      if (!stt__tesselate_cubic(points, num_points, x0,y0, x01,y01,
                                xa,ya, mx,my,
                                objspace_flatness_squared,n+1)) return 0;
      if (!stt__tesselate_cubic(points, num_points, mx,my, xb,yb,
                                x23,y23, x3,y3,
                                objspace_flatness_squared,n+1)) return 0;
   } else {
      stt__add_point(points, *num_points,x3,y3);
      *num_points = *num_points+1;
   }
   return 1;
}

// returns number of contours
static stt__point *stt_FlattenCurves(stt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
   stt__point *points=0;
   int num_points=0;

   float objspace_flatness_squared = objspace_flatness * objspace_flatness;
   int i,n=0,start=0, pass;

   if (!contour_lengths || !num_contours || !vertices || num_verts <= 0)
      return NULL;
   *contour_lengths = NULL;
   *num_contours = 0;

   // count how many "moves" there are to get the contour count
   for (i=0; i < num_verts; ++i)
      if (vertices[i].type == STT_vmove)
         ++n;

   *num_contours = n;
   if (n == 0) return 0;

   if ((size_t)n > ((size_t)-1)/sizeof(**contour_lengths)) return NULL;
   *contour_lengths = (int *)STT_malloc(sizeof(**contour_lengths)*(size_t)n,
                                        userdata);

   if (*contour_lengths == 0) {
      *num_contours = 0;
      return 0;
   }

   // make two passes through the points so we don't need to realloc
   for (pass=0; pass < 2; ++pass) {
      float x=0,y=0;
      if (pass == 1) {
         if (num_points <= 0 || num_points > STT_MAX_TESSELLATION_POINTS ||
             (size_t)num_points > ((size_t)-1)/sizeof(points[0])) goto error;
         points = (stt__point *)STT_malloc((size_t)num_points*sizeof(points[0]),
                                           userdata);
         if (points == NULL) goto error;
      }
      num_points = 0;
      n= -1;
      for (i=0; i < num_verts; ++i) {
         switch (vertices[i].type) {
            case STT_vmove:
               // start the next contour
               if (n >= 0)
                  (*contour_lengths)[n] = num_points - start;
               ++n;
               start = num_points;

               x = vertices[i].x, y = vertices[i].y;
               stt__add_point(points, num_points++, x,y);
               break;
            case STT_vline:
               x = vertices[i].x, y = vertices[i].y;
               stt__add_point(points, num_points++, x, y);
               break;
            case STT_vcurve:
               if (!stt__tesselate_curve(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0)) goto error;
               x = vertices[i].x, y = vertices[i].y;
               break;
            case STT_vcubic:
               if (!stt__tesselate_cubic(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].cx1, vertices[i].cy1,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0)) goto error;
               x = vertices[i].x, y = vertices[i].y;
               break;
         }
      }
      (*contour_lengths)[n] = num_points - start;
   }

   return points;
error:
   STT_free(points, userdata);
   STT_free(*contour_lengths, userdata);
   *contour_lengths = 0;
   *num_contours = 0;
   return NULL;
}

STT_DEF void stt_Rasterize(stt__bitmap *result, float flatness_in_pixels, stt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata)
{
   float abs_scale_x = (float)STT_fabs(scale_x);
   float abs_scale_y = (float)STT_fabs(scale_y);
   float scale = abs_scale_x > abs_scale_y ? abs_scale_y : abs_scale_x;
   int winding_count      = 0;
   int *winding_lengths   = NULL;
   stt__point *windings;
   if (!result || !result->pixels || result->w <= 0 || result->h <= 0 ||
       result->stride < result->w || !vertices || num_verts <= 0 ||
       num_verts > STT_MAX_TESSELLATION_POINTS ||
       (size_t)result->stride > ((size_t)-1)/(size_t)result->h ||
       (size_t)result->w > ((size_t)-1)/(size_t)result->h ||
       (size_t)result->w*(size_t)result->h > STT_MAX_BITMAP_PIXELS ||
       !(scale > 0.0f) || !(scale <= 3.4e38f) ||
       !(flatness_in_pixels > 0.0f) ||
       !(flatness_in_pixels <= 3.4e38f) ||
       !(shift_x >= -3.4e38f && shift_x <= 3.4e38f) ||
       !(shift_y >= -3.4e38f && shift_y <= 3.4e38f) ||
       y_off > 0x7fffffff-result->h ||
       y_off < (-2147483647-1)+result->h) return;
   windings = stt_FlattenCurves(vertices, num_verts,
                               flatness_in_pixels/scale,
                               &winding_lengths, &winding_count, userdata);
   if (windings) {
      stt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
      STT_free(winding_lengths, userdata);
      STT_free(windings, userdata);
   }
}

STT_DEF void stt_FreeBitmap(unsigned char *bitmap, void *userdata)
{
   STT_free(bitmap, userdata);
}

STT_DEF unsigned char *stt_GetGlyphBitmapSubpixel(const stt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   int ix0,iy0,ix1,iy1;
   stt__bitmap gbm;
   stt_vertex *vertices;
   int num_verts;

   if (width) *width = 0;
   if (height) *height = 0;
   if (xoff) *xoff = 0;
   if (yoff) *yoff = 0;
   if (!info || glyph < 0 || glyph >= info->numGlyphs) return NULL;
   num_verts = stt_GetGlyphShape(info, glyph, &vertices);

   if (scale_x == 0) scale_x = scale_y;
   if (scale_y == 0) {
      if (scale_x == 0) {
         STT_free(vertices, info->userdata);
         return NULL;
      }
      scale_y = scale_x;
   }

   stt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,&ix1,&iy1);

   // now we get the size
   if (ix1 <= ix0 || iy1 <= iy0 ||
       (long long)ix1-ix0 > 0x7fffffffLL ||
       (long long)iy1-iy0 > 0x7fffffffLL) {
      STT_free(vertices, info->userdata);
      return NULL;
   }
   gbm.w = ix1-ix0;
   gbm.h = iy1-iy0;
   gbm.pixels = NULL; // in case we error

   if (width ) *width  = gbm.w;
   if (height) *height = gbm.h;
   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   if ((size_t)gbm.w <= ((size_t)-1)/(size_t)gbm.h &&
       (size_t)gbm.w*(size_t)gbm.h <= STT_MAX_BITMAP_PIXELS) {
      gbm.pixels = (unsigned char *)STT_malloc(
         (size_t)gbm.w*(size_t)gbm.h, info->userdata);
      if (gbm.pixels) {
         gbm.stride = gbm.w;

         stt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
      }
   }
   STT_free(vertices, info->userdata);
   return gbm.pixels;
}

STT_DEF unsigned char *stt_GetGlyphBitmap(const stt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   return stt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}

STT_DEF void stt_MakeGlyphBitmapSubpixel(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
{
   int ix0,iy0;
   stt_vertex *vertices;
   int num_verts;
   stt__bitmap gbm;

   if (!info || !output || out_w <= 0 || out_h <= 0 ||
       out_stride < out_w || glyph < 0 || glyph >= info->numGlyphs) return;
   num_verts = stt_GetGlyphShape(info, glyph, &vertices);

   stt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,0,0);
   gbm.pixels = output;
   gbm.w = out_w;
   gbm.h = out_h;
   gbm.stride = out_stride;

   if (gbm.w > 0 && gbm.h > 0)
      stt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0,iy0, 1, info->userdata);

   STT_free(vertices, info->userdata);
}

STT_DEF void stt_MakeGlyphBitmap(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
   stt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, glyph);
}

STT_DEF unsigned char *stt_GetCodepointBitmapSubpixel(const stt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stt_GetGlyphBitmapSubpixel(info, scale_x, scale_y,shift_x,shift_y, stt_FindGlyphIndex(info,codepoint), width,height,xoff,yoff);
}

STT_DEF void stt_MakeCodepointBitmapSubpixelPrefilter(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint)
{
   stt_MakeGlyphBitmapSubpixelPrefilter(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, oversample_x, oversample_y, sub_x, sub_y, stt_FindGlyphIndex(info,codepoint));
}

STT_DEF void stt_MakeCodepointBitmapSubpixel(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint)
{
   stt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, stt_FindGlyphIndex(info,codepoint));
}

STT_DEF unsigned char *stt_GetCodepointBitmap(const stt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stt_GetCodepointBitmapSubpixel(info, scale_x, scale_y, 0.0f,0.0f, codepoint, width,height,xoff,yoff);
}

STT_DEF void stt_MakeCodepointBitmap(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint)
{
   stt_MakeCodepointBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, codepoint);
}

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-CRAPPY packing to keep source code small

static int stt_BakeFontBitmap_internal(unsigned char *data, int data_size, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                stt_bakedchar *chardata)
{
   float scale;
   int x,y,bottom_y, i;
   stt_fontinfo f;
   if (!data || data_size <= 0 || !pixels || !chardata || pw <= 2 ||
       ph <= 2 || pw > 65535 || ph > 65535 || num_chars <= 0 ||
       first_char < 0 || !(pixel_height > 0.0f) ||
       (size_t)pw > ((size_t)-1)/(size_t)ph ||
       (size_t)pw*(size_t)ph > STT_MAX_BITMAP_PIXELS)
      return 0;
   if (!stt_InitFont(&f, data, data_size, offset))
      return -1;
   f.userdata = NULL;
   STT_memset(pixels, 0, (size_t)pw*(size_t)ph);
   x=y=1;
   bottom_y = 1;

   scale = stt_ScaleForPixelHeight(&f, pixel_height);

   for (i=0; i < num_chars; ++i) {
      int advance, lsb, x0,y0,x1,y1,gw,gh;
      long long gw64, gh64;
      int g;
      if (first_char > 0x7fffffff-i)
         return -i;
      g = stt_FindGlyphIndex(&f, first_char + i);
      stt_GetGlyphHMetrics(&f, g, &advance, &lsb);
      stt_GetGlyphBitmapBox(&f, g, scale,scale, &x0,&y0,&x1,&y1);
      gw64 = (long long)x1-(long long)x0;
      gh64 = (long long)y1-(long long)y0;
      if (gw64 < 0 || gh64 < 0 || gw64 >= pw-1 || gh64 >= ph-1)
         return -i;
      gw = (int)gw64;
      gh = (int)gh64;
      if (gw >= pw-x-1)
         y = bottom_y, x = 1; // advance to next row
      if (y >= ph-gh-1)
         return -i;
      stt_MakeGlyphBitmap(&f, pixels+x+y*pw, gw,gh,pw, scale,scale, g);
      chardata[i].x0 = (stt_int16) x;
      chardata[i].y0 = (stt_int16) y;
      chardata[i].x1 = (stt_int16) (x + gw);
      chardata[i].y1 = (stt_int16) (y + gh);
      chardata[i].xadvance = scale * advance;
      chardata[i].xoff     = (float) x0;
      chardata[i].yoff     = (float) y0;
      x = x + gw + 1;
      if (y+gh+1 > bottom_y)
         bottom_y = y+gh+1;
   }
   return bottom_y;
}

STT_DEF void stt_GetBakedQuad(const stt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stt_aligned_quad *q, int opengl_fillrule)
{
   float d3d_bias = opengl_fillrule ? 0 : -0.5f;
   float ipw, iph;
   const stt_bakedchar *b;
   if (!chardata || !xpos || !ypos || !q || pw <= 0 || ph <= 0 ||
       char_index < 0) {
      if (q) STT_memset(q, 0, sizeof(*q));
      return;
   }
   ipw = 1.0f / pw;
   iph = 1.0f / ph;
   b = chardata + char_index;
   int round_x = STT_ifloor((*xpos + b->xoff) + 0.5f);
   int round_y = STT_ifloor((*ypos + b->yoff) + 0.5f);

   q->x0 = round_x + d3d_bias;
   q->y0 = round_y + d3d_bias;
   q->x1 = round_x + b->x1 - b->x0 + d3d_bias;
   q->y1 = round_y + b->y1 - b->y0 + d3d_bias;

   q->s0 = b->x0 * ipw;
   q->t0 = b->y0 * iph;
   q->s1 = b->x1 * ipw;
   q->t1 = b->y1 * iph;

   *xpos += b->xadvance;
}

//////////////////////////////////////////////////////////////////////////////
//
// rectangle packing replacement routines if you don't have stb_rect_pack.h
//

#ifndef STB_RECT_PACK_VERSION

typedef int stbrp_coord;

////////////////////////////////////////////////////////////////////////////////////
//                                                                                //
//                                                                                //
// COMPILER WARNING ?!?!?                                                         //
//                                                                                //
//                                                                                //
// if you get a compile warning due to these symbols being defined more than      //
// once, move #include "stb_rect_pack.h" before #include "struetype.h"            //
//                                                                                //
////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
   int width,height;
   int x,y,bottom_y;
} stbrp_context;

typedef struct
{
   unsigned char x;
} stbrp_node;

struct stbrp_rect
{
   stbrp_coord x,y;
   int id,w,h,was_packed;
};

static void stbrp_init_target(stbrp_context *con, int pw, int ph, stbrp_node *nodes, int num_nodes)
{
   if (!con) return;
   con->width  = pw;
   con->height = ph;
   con->x = 0;
   con->y = 0;
   con->bottom_y = 0;
   STT__NOTUSED(nodes);
   STT__NOTUSED(num_nodes);
}

static void stbrp_pack_rects(stbrp_context *con, stbrp_rect *rects, int num_rects)
{
   int i;
   if (!con || !rects || num_rects <= 0) return;
   for (i=0; i < num_rects; ++i) {
      rects[i].was_packed = 0;
      if (rects[i].w == 0 && rects[i].h == 0) {
         rects[i].x = con->x;
         rects[i].y = con->y;
         rects[i].was_packed = 1;
         continue;
      }
      if (rects[i].w <= 0 || rects[i].h <= 0 ||
          rects[i].w > con->width || rects[i].h > con->height)
         continue;
      if (con->x > con->width-rects[i].w) {
         con->x = 0;
         con->y = con->bottom_y;
      }
      if (con->y > con->height-rects[i].h)
         continue;
      rects[i].x = con->x;
      rects[i].y = con->y;
      rects[i].was_packed = 1;
      con->x += rects[i].w;
      if (con->y + rects[i].h > con->bottom_y)
         con->bottom_y = con->y + rects[i].h;
   }
   for (   ; i < num_rects; ++i)
      rects[i].was_packed = 0;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
// stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.

STT_DEF int stt_PackBegin(stt_pack_context *spc, unsigned char *pixels, int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
{
   stbrp_context *context;
   stbrp_node *nodes;
   int num_nodes, stride, row;

   if (!spc) return 0;
   STT_memset(spc, 0, sizeof(*spc));
   stride = stride_in_bytes != 0 ? stride_in_bytes : pw;
   if (pw <= 0 || ph <= 0 || pw > 65535 || ph > 65535 || padding < 0 ||
       padding >= pw || padding >= ph || stride < pw ||
       (size_t)pw > ((size_t)-1)/(size_t)ph ||
       (size_t)pw*(size_t)ph > STT_MAX_BITMAP_PIXELS ||
       (size_t)stride > ((size_t)-1)/(size_t)ph)
      return 0;

   num_nodes = pw-padding;
   if ((size_t)num_nodes > ((size_t)-1)/sizeof(*nodes))
      return 0;
   context = (stbrp_context *)STT_malloc(sizeof(*context), alloc_context);
   nodes = (stbrp_node *)STT_malloc(sizeof(*nodes)*(size_t)num_nodes,
                                    alloc_context);

   if (context == NULL || nodes == NULL) {
      if (context != NULL) STT_free(context, alloc_context);
      if (nodes   != NULL) STT_free(nodes  , alloc_context);
      return 0;
   }

   spc->user_allocator_context = alloc_context;
   spc->width = pw;
   spc->height = ph;
   spc->pixels = pixels;
   spc->pack_info = context;
   spc->nodes = nodes;
   spc->padding = padding;
   spc->stride_in_bytes = stride;
   spc->h_oversample = 1;
   spc->v_oversample = 1;
   spc->skip_missing = 0;

   stbrp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);

   if (pixels)
      for (row = 0; row < ph; ++row)
         STT_memset(pixels+(size_t)row*(size_t)stride, 0, (size_t)pw);

   return 1;
}

STT_DEF void stt_PackEnd  (stt_pack_context *spc)
{
   if (!spc) return;
   STT_free(spc->nodes    , spc->user_allocator_context);
   STT_free(spc->pack_info, spc->user_allocator_context);
   spc->nodes = NULL;
   spc->pack_info = NULL;
}

STT_DEF void stt_PackSetOversampling(stt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample)
{
   if (!spc) return;
   if (h_oversample > 0 && h_oversample <= STT_MAX_OVERSAMPLE)
      spc->h_oversample = h_oversample;
   if (v_oversample > 0 && v_oversample <= STT_MAX_OVERSAMPLE)
      spc->v_oversample = v_oversample;
}

STT_DEF void stt_PackSetSkipMissingCodepoints(stt_pack_context *spc, int skip)
{
   if (!spc) return;
   spc->skip_missing = skip;
}

#define STT__OVER_MASK  (STT_MAX_OVERSAMPLE-1)

static void stt__h_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
   unsigned char buffer[STT_MAX_OVERSAMPLE];
   int safe_w = w - kernel_width;
   int j;
   if (!pixels || w <= 0 || h <= 0 || stride_in_bytes < w ||
       kernel_width == 0 || kernel_width > STT_MAX_OVERSAMPLE ||
       kernel_width > (unsigned int)w) return;
   STT_memset(buffer, 0, STT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
   for (j=0; j < h; ++j) {
      int i;
      unsigned int total;
      STT_memset(buffer, 0, kernel_width);

      total = 0;

      // make kernel_width a constant in common cases so compiler can optimize out the divide
      switch (kernel_width) {
         case 2:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 4);
            }
            break;
         case 5:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 5);
            }
            break;
         default:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < w; ++i) {
         STT_assert(pixels[i] == 0);
         total -= buffer[i & STT__OVER_MASK];
         pixels[i] = (unsigned char) (total / kernel_width);
      }

      pixels += stride_in_bytes;
   }
}

static void stt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
   unsigned char buffer[STT_MAX_OVERSAMPLE];
   int safe_h = h - kernel_width;
   int j;
   if (!pixels || w <= 0 || h <= 0 || stride_in_bytes < w ||
       kernel_width == 0 || kernel_width > STT_MAX_OVERSAMPLE ||
       kernel_width > (unsigned int)h) return;
   STT_memset(buffer, 0, STT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
   for (j=0; j < w; ++j) {
      int i;
      unsigned int total;
      STT_memset(buffer, 0, kernel_width);

      total = 0;

      // make kernel_width a constant in common cases so compiler can optimize out the divide
      switch (kernel_width) {
         case 2:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 4);
            }
            break;
         case 5:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 5);
            }
            break;
         default:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STT__OVER_MASK];
               buffer[(i+kernel_width) & STT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < h; ++i) {
         STT_assert(pixels[i*stride_in_bytes] == 0);
         total -= buffer[i & STT__OVER_MASK];
         pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
      }

      pixels += 1;
   }
}

static float stt__oversample_shift(int oversample)
{
   if (!oversample)
      return 0.0f;

   // The prefilter is a box filter of width "oversample",
   // which shifts phase by (oversample - 1)/2 pixels in
   // oversampled space. We want to shift in the opposite
   // direction to counter this.
   return (float)-(oversample - 1) / (2.0f * (float)oversample);
}

// rects array must be big enough to accommodate all characters in the given ranges
STT_DEF int stt_PackFontRangesGatherRects(stt_pack_context *spc, const stt_fontinfo *info, stt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
   int i,j,k;
   int missing_glyph_added = 0;

   if (!spc || !info || !ranges || !rects || num_ranges <= 0 ||
       spc->padding < 0 || spc->h_oversample == 0 ||
       spc->v_oversample == 0 ||
       spc->h_oversample > STT_MAX_OVERSAMPLE ||
       spc->v_oversample > STT_MAX_OVERSAMPLE)
      return 0;
   k=0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stt_ScaleForPixelHeight(info, fh) : stt_ScaleForMappingEmToPixels(info, -fh);
      if (ranges[i].num_chars < 0 ||
          (ranges[i].num_chars > 0 && !ranges[i].chardata_for_range) ||
          fh == 0.0f || fh != fh)
         return 0;
      ranges[i].h_oversample = (unsigned char) spc->h_oversample;
      ranges[i].v_oversample = (unsigned char) spc->v_oversample;
      for (j=0; j < ranges[i].num_chars; ++j) {
         int x0,y0,x1,y1;
         int codepoint;
         int glyph;
         int rw, rh;
         long long rw64, rh64;
         if (ranges[i].array_of_unicode_codepoints == NULL) {
            if (ranges[i].first_unicode_codepoint_in_range >
                0x7fffffff-j)
               return 0;
            codepoint = ranges[i].first_unicode_codepoint_in_range+j;
         } else {
            codepoint = ranges[i].array_of_unicode_codepoints[j];
         }
         glyph = stt_FindGlyphIndex(info, codepoint);
         if (glyph == 0 && (spc->skip_missing || missing_glyph_added)) {
            rects[k].w = rects[k].h = 0;
         } else {
            stt_GetGlyphBitmapBoxSubpixel(info,glyph,
                                            scale * spc->h_oversample,
                                            scale * spc->v_oversample,
                                            0,0,
                                            &x0,&y0,&x1,&y1);
            rw64 = (long long)x1-(long long)x0+spc->padding+
                   (int)spc->h_oversample-1;
            rh64 = (long long)y1-(long long)y0+spc->padding+
                   (int)spc->v_oversample-1;
            if (rw64 < 0 || rh64 < 0 || rw64 > 0x7fffffffLL ||
                rh64 > 0x7fffffffLL)
               return 0;
            rw = (int)rw64;
            rh = (int)rh64;
            rects[k].w = (stbrp_coord)rw;
            rects[k].h = (stbrp_coord)rh;
            if ((int)rects[k].w != rw || (int)rects[k].h != rh)
               return 0;
            if (glyph == 0)
               missing_glyph_added = 1;
         }
         ++k;
      }
   }

   return k;
}

STT_DEF void stt_MakeGlyphBitmapSubpixelPrefilter(const stt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int prefilter_x, int prefilter_y, float *sub_x, float *sub_y, int glyph)
{
   if (sub_x) *sub_x = 0.0f;
   if (sub_y) *sub_y = 0.0f;
   if (!info || !output || !sub_x || !sub_y || out_w <= 0 || out_h <= 0 ||
       out_stride < out_w || prefilter_x <= 0 || prefilter_y <= 0 ||
       prefilter_x > STT_MAX_OVERSAMPLE ||
       prefilter_y > STT_MAX_OVERSAMPLE || prefilter_x > out_w ||
       prefilter_y > out_h || glyph < 0 || glyph >= info->numGlyphs)
      return;
   stt_MakeGlyphBitmapSubpixel(info,
                                 output,
                                 out_w - (prefilter_x - 1),
                                 out_h - (prefilter_y - 1),
                                 out_stride,
                                 scale_x,
                                 scale_y,
                                 shift_x,
                                 shift_y,
                                 glyph);

   if (prefilter_x > 1)
      stt__h_prefilter(output, out_w, out_h, out_stride, prefilter_x);

   if (prefilter_y > 1)
      stt__v_prefilter(output, out_w, out_h, out_stride, prefilter_y);

   *sub_x = stt__oversample_shift(prefilter_x);
   *sub_y = stt__oversample_shift(prefilter_y);
}

// rects array must be big enough to accommodate all characters in the given ranges
STT_DEF int stt_PackFontRangesRenderIntoRects(stt_pack_context *spc, const stt_fontinfo *info, stt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
   int i,j,k, have_missing_glyph = 0, return_value = 1;
   stt_packedchar missing_glyph;
   int old_h_over, old_v_over;

   if (!spc || !info || !ranges || !rects || !spc->pixels ||
       !spc->pack_info || num_ranges <= 0 || spc->width <= 0 ||
       spc->height <= 0 || spc->stride_in_bytes < spc->width ||
       (size_t)spc->stride_in_bytes >
          ((size_t)-1)/(size_t)spc->height ||
       spc->padding < 0 || spc->h_oversample == 0 ||
       spc->v_oversample == 0 ||
       spc->h_oversample > STT_MAX_OVERSAMPLE ||
       spc->v_oversample > STT_MAX_OVERSAMPLE)
      return 0;
   STT_memset(&missing_glyph, 0, sizeof(missing_glyph));
   old_h_over = (int)spc->h_oversample;
   old_v_over = (int)spc->v_oversample;
   k = 0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stt_ScaleForPixelHeight(info, fh) : stt_ScaleForMappingEmToPixels(info, -fh);
      float recip_h,recip_v,sub_x,sub_y;
      if (ranges[i].num_chars < 0 ||
          (ranges[i].num_chars > 0 && !ranges[i].chardata_for_range) ||
          ranges[i].h_oversample == 0 || ranges[i].v_oversample == 0 ||
          ranges[i].h_oversample > STT_MAX_OVERSAMPLE ||
          ranges[i].v_oversample > STT_MAX_OVERSAMPLE ||
          fh == 0.0f || fh != fh) {
         return_value = 0;
         break;
      }
      spc->h_oversample = ranges[i].h_oversample;
      spc->v_oversample = ranges[i].v_oversample;
      recip_h = 1.0f / spc->h_oversample;
      recip_v = 1.0f / spc->v_oversample;
      sub_x = stt__oversample_shift(spc->h_oversample);
      sub_y = stt__oversample_shift(spc->v_oversample);
      for (j=0; j < ranges[i].num_chars; ++j) {
         stbrp_rect *r = &rects[k];
         if (r->was_packed && r->w != 0 && r->h != 0) {
            stt_packedchar *bc = &ranges[i].chardata_for_range[j];
            int advance, lsb, x0,y0,x1,y1;
            int codepoint;
            int glyph;
            stbrp_coord pad = (stbrp_coord) spc->padding;

            if (r->x < 0 || r->y < 0 || r->w < pad || r->h < pad ||
                r->x > spc->width-r->w || r->y > spc->height-r->h) {
               return_value = 0;
               ++k;
               continue;
            }
            if (ranges[i].array_of_unicode_codepoints == NULL) {
               if (ranges[i].first_unicode_codepoint_in_range >
                   0x7fffffff-j) {
                  return_value = 0;
                  ++k;
                  continue;
               }
               codepoint = ranges[i].first_unicode_codepoint_in_range+j;
            } else {
               codepoint = ranges[i].array_of_unicode_codepoints[j];
            }
            glyph = stt_FindGlyphIndex(info, codepoint);

            // pad on left and top
            r->x += pad;
            r->y += pad;
            r->w -= pad;
            r->h -= pad;
            stt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
            stt_GetGlyphBitmapBox(info, glyph,
                                    scale * spc->h_oversample,
                                    scale * spc->v_oversample,
                                    &x0,&y0,&x1,&y1);
            stt_MakeGlyphBitmapSubpixel(info,
                                          spc->pixels+(size_t)r->x+
                                          (size_t)r->y*(size_t)spc->stride_in_bytes,
                                          r->w - spc->h_oversample+1,
                                          r->h - spc->v_oversample+1,
                                          spc->stride_in_bytes,
                                          scale * spc->h_oversample,
                                          scale * spc->v_oversample,
                                          0,0,
                                          glyph);

            if (spc->h_oversample > 1)
               stt__h_prefilter(spc->pixels+(size_t)r->x+
                                  (size_t)r->y*(size_t)spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->h_oversample);

            if (spc->v_oversample > 1)
               stt__v_prefilter(spc->pixels+(size_t)r->x+
                                  (size_t)r->y*(size_t)spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->v_oversample);

            bc->x0       = (stt_int16)  r->x;
            bc->y0       = (stt_int16)  r->y;
            bc->x1       = (stt_int16) (r->x + r->w);
            bc->y1       = (stt_int16) (r->y + r->h);
            bc->xadvance =                scale * advance;
            bc->xoff     =       (float)  x0 * recip_h + sub_x;
            bc->yoff     =       (float)  y0 * recip_v + sub_y;
            bc->xoff2    =                (x0 + r->w) * recip_h + sub_x;
            bc->yoff2    =                (y0 + r->h) * recip_v + sub_y;

            if (glyph == 0) {
               missing_glyph = *bc;
               have_missing_glyph = 1;
            }
         } else if (spc->skip_missing) {
            return_value = 0;
         } else if (r->was_packed && r->w == 0 && r->h == 0 &&
                    have_missing_glyph) {
            ranges[i].chardata_for_range[j] = missing_glyph;
         } else {
            return_value = 0; // if any fail, report failure
         }

         ++k;
      }
   }

   // restore original values
   spc->h_oversample = old_h_over;
   spc->v_oversample = old_v_over;

   return return_value;
}

STT_DEF void stt_PackFontRangesPackRects(stt_pack_context *spc, stbrp_rect *rects, int num_rects)
{
   if (!spc || !spc->pack_info || !rects || num_rects <= 0) return;
   stbrp_pack_rects((stbrp_context *) spc->pack_info, rects, num_rects);
}

STT_DEF int stt_PackFontRanges(stt_pack_context *spc, const unsigned char *fontdata, int fontdata_size, int font_index, stt_pack_range *ranges, int num_ranges)
{
   stt_fontinfo info;
   int i,j,n,offset, return_value = 1;
   //stbrp_context *context = (stbrp_context *) spc->pack_info;
   stbrp_rect    *rects;

   if (!spc || !spc->pack_info || !spc->nodes || !spc->pixels ||
       !fontdata || fontdata_size <= 0 || font_index < 0 || !ranges ||
       num_ranges <= 0)
      return 0;

   n = 0;
   for (i=0; i < num_ranges; ++i) {
      if (ranges[i].num_chars < 0 ||
          (ranges[i].num_chars > 0 && !ranges[i].chardata_for_range) ||
          ranges[i].num_chars > 0x7fffffff-n)
         return 0;
      n += ranges[i].num_chars;
   }
   if (n == 0) return 1;

   // flag all characters as NOT packed
   for (i=0; i < num_ranges; ++i)
      for (j=0; j < ranges[i].num_chars; ++j)
         ranges[i].chardata_for_range[j].x0 =
         ranges[i].chardata_for_range[j].y0 =
         ranges[i].chardata_for_range[j].x1 =
         ranges[i].chardata_for_range[j].y1 = 0;

   if ((size_t)n > ((size_t)-1)/sizeof(*rects)) return 0;
   rects = (stbrp_rect *)STT_malloc(sizeof(*rects)*(size_t)n,
                                    spc->user_allocator_context);
   if (rects == NULL)
      return 0;

   offset = stt_GetFontOffsetForIndex(fontdata, fontdata_size, font_index);
   if (offset < 0 || !stt_InitFont(&info, fontdata, fontdata_size, offset)) {
      STT_free(rects, spc->user_allocator_context);
      return 0;
   }
   info.userdata = spc->user_allocator_context;

   if (stt_PackFontRangesGatherRects(spc, &info, ranges, num_ranges,
                                     rects) != n) {
      STT_free(rects, spc->user_allocator_context);
      return 0;
   }

   stt_PackFontRangesPackRects(spc, rects, n);

   return_value = stt_PackFontRangesRenderIntoRects(spc, &info, ranges, num_ranges, rects);

   STT_free(rects, spc->user_allocator_context);
   return return_value;
}

STT_DEF int stt_PackFontRange(stt_pack_context *spc, const unsigned char *fontdata, int fontdata_size, int font_index, float font_size,
            int first_unicode_codepoint_in_range, int num_chars_in_range, stt_packedchar *chardata_for_range)
{
   stt_pack_range range;
   if (num_chars_in_range < 0 ||
       (num_chars_in_range > 0 && !chardata_for_range))
      return 0;
   range.first_unicode_codepoint_in_range = first_unicode_codepoint_in_range;
   range.array_of_unicode_codepoints = NULL;
   range.num_chars                   = num_chars_in_range;
   range.chardata_for_range          = chardata_for_range;
   range.font_size                   = font_size;
   return stt_PackFontRanges(spc, fontdata, fontdata_size, font_index, &range, 1);
}

STT_DEF void stt_GetScaledFontVMetrics(const unsigned char *fontdata, int fontdata_size, int index, float size, float *ascent, float *descent, float *lineGap)
{
   int i_ascent, i_descent, i_lineGap;
   float scale;
   stt_fontinfo info;
   int offset;
   if (ascent) *ascent = 0.0f;
   if (descent) *descent = 0.0f;
   if (lineGap) *lineGap = 0.0f;
   if (!fontdata || fontdata_size <= 0 || index < 0 || !ascent ||
       !descent || !lineGap || size == 0.0f || size != size) return;
   offset = stt_GetFontOffsetForIndex(fontdata, fontdata_size, index);
   if (offset < 0 || !stt_InitFont(&info, fontdata, fontdata_size, offset))
      return;
   scale = size > 0 ? stt_ScaleForPixelHeight(&info, size) : stt_ScaleForMappingEmToPixels(&info, -size);
   stt_GetFontVMetrics(&info, &i_ascent, &i_descent, &i_lineGap);
   *ascent  = (float) i_ascent  * scale;
   *descent = (float) i_descent * scale;
   *lineGap = (float) i_lineGap * scale;
}

STT_DEF void stt_GetPackedQuad(const stt_packedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stt_aligned_quad *q, int align_to_integer)
{
   float ipw, iph;
   const stt_packedchar *b;
   if (!chardata || !xpos || !ypos || !q || pw <= 0 || ph <= 0 ||
       char_index < 0) {
      if (q) STT_memset(q, 0, sizeof(*q));
      return;
   }
   ipw = 1.0f / pw;
   iph = 1.0f / ph;
   b = chardata + char_index;

   if (align_to_integer) {
      float x = (float) STT_ifloor((*xpos + b->xoff) + 0.5f);
      float y = (float) STT_ifloor((*ypos + b->yoff) + 0.5f);
      q->x0 = x;
      q->y0 = y;
      q->x1 = x + b->xoff2 - b->xoff;
      q->y1 = y + b->yoff2 - b->yoff;
   } else {
      q->x0 = *xpos + b->xoff;
      q->y0 = *ypos + b->yoff;
      q->x1 = *xpos + b->xoff2;
      q->y1 = *ypos + b->yoff2;
   }

   q->s0 = b->x0 * ipw;
   q->t0 = b->y0 * iph;
   q->s1 = b->x1 * ipw;
   q->t1 = b->y1 * iph;

   *xpos += b->xadvance;
}

//////////////////////////////////////////////////////////////////////////////
//
// sdf computation
//

#define STT_min(a,b)  ((a) < (b) ? (a) : (b))
#define STT_max(a,b)  ((a) < (b) ? (b) : (a))

static int stt__ray_intersect_bezier(float orig[2], float ray[2], float q0[2], float q1[2], float q2[2], float hits[2][2])
{
   float q0perp = q0[1]*ray[0] - q0[0]*ray[1];
   float q1perp = q1[1]*ray[0] - q1[0]*ray[1];
   float q2perp = q2[1]*ray[0] - q2[0]*ray[1];
   float roperp = orig[1]*ray[0] - orig[0]*ray[1];

   float a = q0perp - 2*q1perp + q2perp;
   float b = q1perp - q0perp;
   float c = q0perp - roperp;

   float s0 = 0., s1 = 0.;
   int num_s = 0;

   if (a != 0.0) {
      float discr = b*b - a*c;
      if (discr > 0.0) {
         float rcpna = -1 / a;
         float d = (float) STT_sqrt(discr);
         s0 = (b+d) * rcpna;
         s1 = (b-d) * rcpna;
         if (s0 >= 0.0 && s0 <= 1.0)
            num_s = 1;
         if (d > 0.0 && s1 >= 0.0 && s1 <= 1.0) {
            if (num_s == 0) s0 = s1;
            ++num_s;
         }
      }
   } else {
      // 2*b*s + c = 0
      // s = -c / (2*b)
      if (b == 0.0f) return 0;
      s0 = c / (-2 * b);
      if (s0 >= 0.0 && s0 <= 1.0)
         num_s = 1;
   }

   if (num_s == 0)
      return 0;
   else {
      float rcp_len2 = 1 / (ray[0]*ray[0] + ray[1]*ray[1]);
      float rayn_x = ray[0] * rcp_len2, rayn_y = ray[1] * rcp_len2;

      float q0d =   q0[0]*rayn_x +   q0[1]*rayn_y;
      float q1d =   q1[0]*rayn_x +   q1[1]*rayn_y;
      float q2d =   q2[0]*rayn_x +   q2[1]*rayn_y;
      float rod = orig[0]*rayn_x + orig[1]*rayn_y;

      float q10d = q1d - q0d;
      float q20d = q2d - q0d;
      float q0rd = q0d - rod;

      hits[0][0] = q0rd + s0*(2.0f - 2.0f*s0)*q10d + s0*s0*q20d;
      hits[0][1] = a*s0+b;

      if (num_s > 1) {
         hits[1][0] = q0rd + s1*(2.0f - 2.0f*s1)*q10d + s1*s1*q20d;
         hits[1][1] = a*s1+b;
         return 2;
      } else {
         return 1;
      }
   }
}

static int equal(float *a, float *b)
{
   return (a[0] == b[0] && a[1] == b[1]);
}

static int stt__compute_crossings_x(float x, float y, int nverts, stt_vertex *verts)
{
   int i;
   float orig[2], ray[2] = { 1, 0 };
   float y_frac;
   int winding = 0;

   // make sure y never passes through a vertex of the shape
   y_frac = (float) STT_fmod(y, 1.0f);
   if (y_frac < 0.01f)
      y += 0.01f;
   else if (y_frac > 0.99f)
      y -= 0.01f;

   orig[0] = x;
   orig[1] = y;

   // test a ray from (-infinity,y) to (x,y)
   for (i=0; i < nverts; ++i) {
      if (i > 0 && verts[i].type == STT_vline) {
         int x0 = (int) verts[i-1].x, y0 = (int) verts[i-1].y;
         int x1 = (int) verts[i  ].x, y1 = (int) verts[i  ].y;
         if (y > STT_min(y0,y1) && y < STT_max(y0,y1) && x > STT_min(x0,x1)) {
            float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
            if (x_inter < x)
               winding += (y0 < y1) ? 1 : -1;
         }
      }
      if (i > 0 && verts[i].type == STT_vcurve) {
         int x0 = (int) verts[i-1].x , y0 = (int) verts[i-1].y ;
         int x1 = (int) verts[i  ].cx, y1 = (int) verts[i  ].cy;
         int x2 = (int) verts[i  ].x , y2 = (int) verts[i  ].y ;
         int ax = STT_min(x0,STT_min(x1,x2)), ay = STT_min(y0,STT_min(y1,y2));
         int by = STT_max(y0,STT_max(y1,y2));
         if (y > ay && y < by && x > ax) {
            float q0[2],q1[2],q2[2];
            float hits[2][2];
            q0[0] = (float)x0;
            q0[1] = (float)y0;
            q1[0] = (float)x1;
            q1[1] = (float)y1;
            q2[0] = (float)x2;
            q2[1] = (float)y2;
            if (equal(q0,q1) || equal(q1,q2)) {
               x0 = (int)verts[i-1].x;
               y0 = (int)verts[i-1].y;
               x1 = (int)verts[i  ].x;
               y1 = (int)verts[i  ].y;
               if (y > STT_min(y0,y1) && y < STT_max(y0,y1) && x > STT_min(x0,x1)) {
                  float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
                  if (x_inter < x)
                     winding += (y0 < y1) ? 1 : -1;
               }
            } else {
               int num_hits = stt__ray_intersect_bezier(orig, ray, q0, q1, q2, hits);
               if (num_hits >= 1)
                  if (hits[0][0] < 0)
                     winding += (hits[0][1] < 0 ? -1 : 1);
               if (num_hits >= 2)
                  if (hits[1][0] < 0)
                     winding += (hits[1][1] < 0 ? -1 : 1);
            }
         }
      }
   }
   return winding;
}

static float stt__cuberoot( float x )
{
   if (x<0)
      return -(float) STT_pow(-x,1.0f/3.0f);
   else
      return  (float) STT_pow( x,1.0f/3.0f);
}

// x^3 + a*x^2 + b*x + c = 0
static int stt__solve_cubic(float a, float b, float c, float* r)
{
   float s = -a / 3;
   float p = b - a*a / 3;
   float q = a * (2*a*a - 9*b) / 27 + c;
   float p3 = p*p*p;
   float d = q*q + 4*p3 / 27;
   if (d >= 0) {
      float z = (float) STT_sqrt(d);
      float u = (-q + z) / 2;
      float v = (-q - z) / 2;
      u = stt__cuberoot(u);
      v = stt__cuberoot(v);
      r[0] = s + u + v;
      return 1;
   } else {
      float u = (float) STT_sqrt(-p/3);
      float v = (float) STT_acos(-STT_sqrt(-27/p3) * q / 2) / 3; // p3 must be negative, since d is negative
      float m = (float) STT_cos(v);
      float n = (float) STT_cos(v-3.141592/2)*1.732050808f;
      r[0] = s + u * 2 * m;
      r[1] = s - u * (m + n);
      r[2] = s - u * (m - n);

      //STT_assert( STT_fabs(((r[0]+a)*r[0]+b)*r[0]+c) < 0.05f);  // these asserts may not be safe at all scales, though they're in bezier t parameter units so maybe?
      //STT_assert( STT_fabs(((r[1]+a)*r[1]+b)*r[1]+c) < 0.05f);
      //STT_assert( STT_fabs(((r[2]+a)*r[2]+b)*r[2]+c) < 0.05f);
      return 3;
   }
}

STT_DEF unsigned char * stt_GetGlyphSDF(const stt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
   float scale_x = scale, scale_y = scale;
   int ix0,iy0,ix1,iy1;
   int w,h;
   unsigned char *data;

   if (width) *width = 0;
   if (height) *height = 0;
   if (xoff) *xoff = 0;
   if (yoff) *yoff = 0;
   if (!info || glyph < 0 || glyph >= info->numGlyphs ||
       !(scale != 0.0f) || padding < 0) return NULL;

   stt_GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.0f,0.0f, &ix0,&iy0,&ix1,&iy1);

   // if empty, return NULL
   if (ix0 == ix1 || iy0 == iy1)
      return NULL;

   if (ix0 < (-2147483647-1)+padding || iy0 < (-2147483647-1)+padding ||
       ix1 > 2147483647-padding || iy1 > 2147483647-padding)
      return NULL;
   ix0 -= padding;
   iy0 -= padding;
   ix1 += padding;
   iy1 += padding;

   if (ix1 <= ix0 || iy1 <= iy0 ||
       (long long)ix1-ix0 > 0x7fffffffLL ||
       (long long)iy1-iy0 > 0x7fffffffLL) return NULL;
   w = ix1-ix0;
   h = iy1-iy0;
   if ((size_t)w > ((size_t)-1)/(size_t)h ||
       (size_t)w*(size_t)h > STT_MAX_BITMAP_PIXELS) return NULL;

   if (width ) *width  = w;
   if (height) *height = h;
   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   // invert for y-downwards bitmaps
   scale_y = -scale_y;

   {
      // distance from singular values (in the same units as the pixel grid)
      const float eps = 1./1024, eps2 = eps*eps;
      int x,y,i,j;
      float *precompute;
      stt_vertex *verts;
      int num_verts = stt_GetGlyphShape(info, glyph, &verts);
      if (num_verts <= 0 || !verts ||
          (size_t)num_verts > ((size_t)-1)/sizeof(float)) {
         if (verts) STT_free(verts, info->userdata);
         return NULL;
      }
      data = (unsigned char *)STT_malloc((size_t)w*(size_t)h,
                                         info->userdata);
      precompute = (float *)STT_malloc((size_t)num_verts*sizeof(float),
                                       info->userdata);
      if (!data || !precompute) {
         if (data) STT_free(data, info->userdata);
         if (precompute) STT_free(precompute, info->userdata);
         STT_free(verts, info->userdata);
         return NULL;
      }

      for (i=0,j=num_verts-1; i < num_verts; j=i++) {
         if (verts[i].type == STT_vline) {
            float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;
            float x1 = verts[j].x*scale_x, y1 = verts[j].y*scale_y;
            float dist = (float) STT_sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
            precompute[i] = (dist < eps) ? 0.0f : 1.0f / dist;
         } else if (verts[i].type == STT_vcurve) {
            float x2 = verts[j].x *scale_x, y2 = verts[j].y *scale_y;
            float x1 = verts[i].cx*scale_x, y1 = verts[i].cy*scale_y;
            float x0 = verts[i].x *scale_x, y0 = verts[i].y *scale_y;
            float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
            float len2 = bx*bx + by*by;
            if (len2 >= eps2)
               precompute[i] = 1.0f / len2;
            else
               precompute[i] = 0.0f;
         } else
            precompute[i] = 0.0f;
      }

      for (y=iy0; y < iy1; ++y) {
         for (x=ix0; x < ix1; ++x) {
            float val;
            float min_dist = 999999.0f;
            float sx = (float) x + 0.5f;
            float sy = (float) y + 0.5f;
            float x_gspace = (sx / scale_x);
            float y_gspace = (sy / scale_y);

            int winding = stt__compute_crossings_x(x_gspace, y_gspace, num_verts, verts); // @OPTIMIZE: this could just be a rasterization, but needs to be line vs. non-tesselated curves so a new path

            for (i=0; i < num_verts; ++i) {
               float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;

               if (i > 0 && verts[i].type == STT_vline && precompute[i] != 0.0f) {
                  float x1 = verts[i-1].x*scale_x, y1 = verts[i-1].y*scale_y;

                  float dist,dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
                  if (dist2 < min_dist*min_dist)
                     min_dist = (float) STT_sqrt(dist2);

                  // coarse culling against bbox
                  //if (sx > STT_min(x0,x1)-min_dist && sx < STT_max(x0,x1)+min_dist &&
                  //    sy > STT_min(y0,y1)-min_dist && sy < STT_max(y0,y1)+min_dist)
                  dist = (float) STT_fabs((x1-x0)*(y0-sy) - (y1-y0)*(x0-sx)) * precompute[i];
                  if (dist < min_dist) {
                     // check position along line
                     // x' = x0 + t*(x1-x0), y' = y0 + t*(y1-y0)
                     // minimize (x'-sx)*(x'-sx)+(y'-sy)*(y'-sy)
                     float dx = x1-x0, dy = y1-y0;
                     float px = x0-sx, py = y0-sy;
                     // minimize (px+t*dx)^2 + (py+t*dy)^2 = px*px + 2*px*dx*t + t^2*dx*dx + py*py + 2*py*dy*t + t^2*dy*dy
                     // derivative: 2*px*dx + 2*py*dy + (2*dx*dx+2*dy*dy)*t, set to 0 and solve
                     float t = -(px*dx + py*dy) / (dx*dx + dy*dy);
                     if (t >= 0.0f && t <= 1.0f)
                        min_dist = dist;
                  }
               } else if (i > 0 && verts[i].type == STT_vcurve) {
                  float x2 = verts[i-1].x *scale_x, y2 = verts[i-1].y *scale_y;
                  float x1 = verts[i  ].cx*scale_x, y1 = verts[i  ].cy*scale_y;
                  float box_x0 = STT_min(STT_min(x0,x1),x2);
                  float box_y0 = STT_min(STT_min(y0,y1),y2);
                  float box_x1 = STT_max(STT_max(x0,x1),x2);
                  float box_y1 = STT_max(STT_max(y0,y1),y2);
                  // coarse culling against bbox to avoid computing cubic unnecessarily
                  if (sx > box_x0-min_dist && sx < box_x1+min_dist && sy > box_y0-min_dist && sy < box_y1+min_dist) {
                     int num=0;
                     float ax = x1-x0, ay = y1-y0;
                     float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
                     float mx = x0 - sx, my = y0 - sy;
                     float res[3] = {0.f,0.f,0.f};
                     float px,py,t,it,dist2;
                     float a_inv = precompute[i];
                     if (a_inv == 0.0) { // if a_inv is 0, it's 2nd degree so use quadratic formula
                        float a = 3*(ax*bx + ay*by);
                        float b = 2*(ax*ax + ay*ay) + (mx*bx+my*by);
                        float c = mx*ax+my*ay;
                        if (STT_fabs(a) < eps2) { // if a is 0, it's linear
                           if (STT_fabs(b) >= eps2) {
                              res[num++] = -c/b;
                           }
                        } else {
                           float discriminant = b*b - 4*a*c;
                           if (discriminant < 0)
                              num = 0;
                           else {
                              float root = (float) STT_sqrt(discriminant);
                              res[0] = (-b - root)/(2*a);
                              res[1] = (-b + root)/(2*a);
                              num = 2; // don't bother distinguishing 1-solution case, as code below will still work
                           }
                        }
                     } else {
                        float b = 3*(ax*bx + ay*by) * a_inv; // could precompute this as it doesn't depend on sample point
                        float c = (2*(ax*ax + ay*ay) + (mx*bx+my*by)) * a_inv;
                        float d = (mx*ax+my*ay) * a_inv;
                        num = stt__solve_cubic(b, c, d, res);
                     }
                     dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
                     if (dist2 < min_dist*min_dist)
                        min_dist = (float) STT_sqrt(dist2);

                     if (num >= 1 && res[0] >= 0.0f && res[0] <= 1.0f) {
                        t = res[0], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STT_sqrt(dist2);
                     }
                     if (num >= 2 && res[1] >= 0.0f && res[1] <= 1.0f) {
                        t = res[1], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STT_sqrt(dist2);
                     }
                     if (num >= 3 && res[2] >= 0.0f && res[2] <= 1.0f) {
                        t = res[2], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STT_sqrt(dist2);
                     }
                  }
               }
            }
            if (winding == 0)
               min_dist = -min_dist;  // if outside the shape, value is negative
            val = onedge_value + pixel_dist_scale * min_dist;
            if (val < 0)
               val = 0;
            else if (val > 255)
               val = 255;
            data[(y-iy0)*w+(x-ix0)] = (unsigned char) val;
         }
      }
      STT_free(precompute, info->userdata);
      STT_free(verts, info->userdata);
   }
   return data;
}

STT_DEF unsigned char * stt_GetCodepointSDF(const stt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
   return stt_GetGlyphSDF(info, scale, stt_FindGlyphIndex(info, codepoint), padding, onedge_value, pixel_dist_scale, width, height, xoff, yoff);
}

STT_DEF void stt_FreeSDF(unsigned char *bitmap, void *userdata)
{
   STT_free(bitmap, userdata);
}

//////////////////////////////////////////////////////////////////////////////
//
// font name matching -- recommended not to use this
//

// check if a utf8 string contains a prefix which is the utf16 string; if so return length of matching utf8 string
static stt_int32 stt__CompareUTF8toUTF16_bigendian_prefix(stt_uint8 *s1, stt_int32 len1, stt_uint8 *s2, stt_int32 len2)
{
   stt_int32 i=0;

   if (!s1 || !s2 || len1 < 0 || len2 < 0 || (len2 & 1))
      return -1;

   // convert utf16 to utf8 and compare the results while converting
   while (len2) {
      stt_uint16 ch = s2[0]*256 + s2[1];
      if (ch < 0x80) {
         if (i >= len1) return -1;
         if (s1[i++] != ch) return -1;
      } else if (ch < 0x800) {
         if (i+1 >= len1) return -1;
         if (s1[i++] != 0xc0 + (ch >> 6)) return -1;
         if (s1[i++] != 0x80 + (ch & 0x3f)) return -1;
      } else if (ch >= 0xd800 && ch < 0xdc00) {
         stt_uint32 c;
         if (len2 < 4) return -1;
         stt_uint16 ch2 = s2[2]*256 + s2[3];
         if (ch2 < 0xdc00 || ch2 >= 0xe000) return -1;
         if (i+3 >= len1) return -1;
         c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
         if (s1[i++] != 0xf0 + (c >> 18)) return -1;
         if (s1[i++] != 0x80 + ((c >> 12) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c >>  6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c      ) & 0x3f)) return -1;
         s2 += 2; // plus another 2 below
         len2 -= 2;
      } else if (ch >= 0xdc00 && ch < 0xe000) {
         return -1;
      } else {
         if (i+2 >= len1) return -1;
         if (s1[i++] != 0xe0 + (ch >> 12)) return -1;
         if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((ch     ) & 0x3f)) return -1;
      }
      s2 += 2;
      len2 -= 2;
   }
   return i;
}

static int stt_CompareUTF8toUTF16_bigendian_internal(char *s1, int len1, char *s2, int len2)
{
   return len1 == stt__CompareUTF8toUTF16_bigendian_prefix((stt_uint8*) s1, len1, (stt_uint8*) s2, len2);
}

// returns results in whatever encoding you request... but note that 2-byte encodings
// will be BIG-ENDIAN... use stt_CompareUTF8toUTF16_bigendian() to compare
STT_DEF const char *stt_GetFontNameString(const stt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID)
{
   stt_int32 i,count;
   stt_uint32 nm, nm_size, stringOffset;
   if (!font || !length) return NULL;
   *length = 0;
   nm = stt__find_table(font, font->fontstart, "name");
   nm_size = stt__get_table_size(font, font->fontstart, "name");
   if (!nm || nm_size < 6) return NULL;

   count = stt__safe_read16(font, nm+2);
   if ((stt_uint32)count > (nm_size-6)/12) return NULL;
   stringOffset = stt__safe_read16(font, nm+4);
   if (stringOffset > nm_size) return NULL;
   for (i=0; i < count; ++i) {
      stt_uint32 loc = nm + 6 + 12 * i;
      if (platformID == stt__safe_read16(font, loc) &&
          encodingID == stt__safe_read16(font, loc+2) &&
          languageID == stt__safe_read16(font, loc+4) &&
          nameID == stt__safe_read16(font, loc+6)) {
         stt_uint32 slen = stt__safe_read16(font, loc+8);
         stt_uint32 soff = stt__safe_read16(font, loc+10);
         if (soff > nm_size-stringOffset || slen > nm_size-stringOffset-soff)
            return NULL;
         *length = (int)slen;
         return (const char *)(font->data + nm + stringOffset + soff);
      }
   }
   return NULL;
}

static int stt__matchpair(const stt_fontinfo *info, stt_uint32 nm,
                          stt_uint32 nm_size, stt_uint8 *name,
                          stt_int32 nlen, stt_int32 target_id,
                          stt_int32 next_id)
{
   stt_int32 i;
   stt_int32 count;
   stt_uint32 stringOffset;

   if (!info || !name || nlen < 0 || nm_size < 6) return 0;
   count = stt__safe_read16(info, nm+2);
   if ((stt_uint32)count > (nm_size-6)/12) return 0;
   stringOffset = stt__safe_read16(info, nm+4);
   if (stringOffset > nm_size) return 0;

   for (i=0; i < count; ++i) {
      stt_uint32 loc = nm + 6 + 12 * i;
      stt_int32 id = stt__safe_read16(info, loc+6);
      if (id == target_id) {
         // find the encoding
         stt_int32 platform = stt__safe_read16(info, loc);
         stt_int32 encoding = stt__safe_read16(info, loc+2);
         stt_int32 language = stt__safe_read16(info, loc+4);

         // is this a Unicode encoding?
         if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10)) {
            stt_uint32 slen = stt__safe_read16(info, loc+8);
            stt_uint32 off = stt__safe_read16(info, loc+10);

            if (off > nm_size-stringOffset || slen > nm_size-stringOffset-off)
               continue;

            // check if there's a prefix match
            stt_int32 matchlen = stt__CompareUTF8toUTF16_bigendian_prefix(
               name, nlen, info->data+nm+stringOffset+off, (stt_int32)slen);
            if (matchlen >= 0) {
               // check for target_id+1 immediately following, with same encoding & language
               if (i+1 < count && stt__safe_read16(info, loc+18) == next_id &&
                   stt__safe_read16(info, loc+12) == platform &&
                   stt__safe_read16(info, loc+14) == encoding &&
                   stt__safe_read16(info, loc+16) == language) {
                  slen = stt__safe_read16(info, loc+20);
                  off = stt__safe_read16(info, loc+22);
                  if (off > nm_size-stringOffset || slen > nm_size-stringOffset-off)
                     continue;
                  if (slen == 0) {
                     if (matchlen == nlen)
                        return 1;
                  } else if (matchlen < nlen && name[matchlen] == ' ') {
                     ++matchlen;
                     if (stt_CompareUTF8toUTF16_bigendian_internal(
                            (char *)(name+matchlen), nlen-matchlen,
                            (char *)(info->data+nm+stringOffset+off),
                            (stt_int32)slen))
                        return 1;
                  }
               } else {
                  // if nothing immediately following
                  if (matchlen == nlen)
                     return 1;
               }
            }
         }

         // @TODO handle other encodings
      }
   }
   return 0;
}

static int stt__matches(stt_uint8 *fc, int data_size, stt_uint32 offset,
                        stt_uint8 *name, stt_int32 flags)
{
   stt_int32 nlen = (stt_int32) STT_strlen((char *) name);
   stt_uint32 nm,nm_size,hd;
   stt_fontinfo info;
   STT_memset(&info, 0, sizeof(info));
   if (offset > (stt_uint32)data_size ||
       !stt_InitFont_internal(&info, fc, data_size, (int)offset)) return 0;

   // check italics/bold/underline flags in macStyle...
   if (flags) {
      hd = stt__find_table(&info, offset, "head");
      if (!hd || (stt__safe_read16(&info, hd+44) & 7) != (flags & 7)) return 0;
   }

   nm = stt__find_table(&info, offset, "name");
   nm_size = stt__get_table_size(&info, offset, "name");
   if (!nm) return 0;

   if (flags) {
      // if we checked the macStyle flags, then just check the family and ignore the subfamily
      if (stt__matchpair(&info, nm, nm_size, name, nlen, 16, -1)) return 1;
      if (stt__matchpair(&info, nm, nm_size, name, nlen,  1, -1)) return 1;
      if (stt__matchpair(&info, nm, nm_size, name, nlen,  3, -1)) return 1;
   } else {
      if (stt__matchpair(&info, nm, nm_size, name, nlen, 16, 17)) return 1;
      if (stt__matchpair(&info, nm, nm_size, name, nlen,  1,  2)) return 1;
      if (stt__matchpair(&info, nm, nm_size, name, nlen,  3, -1)) return 1;
   }

   return 0;
}

static int stt_FindMatchingFont_internal(unsigned char *font_collection,
                                         int fontdata_size, char *name_utf8,
                                         stt_int32 flags)
{
   stt_int32 i, count;
   if (!font_collection || !name_utf8 || fontdata_size < 0) return -1;
   count = stt_GetNumberOfFonts_internal(font_collection, fontdata_size);
   for (i=0; i<count; ++i) {
      stt_int32 off = stt_GetFontOffsetForIndex_internal(font_collection,
                                                          fontdata_size, i);
      if (off < 0) return -1;
      if (stt__matches((stt_uint8 *)font_collection, fontdata_size,
                       (stt_uint32)off, (stt_uint8 *)name_utf8, flags))
         return off;
   }
   return -1;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

STT_DEF int stt_BakeFontBitmap(const unsigned char *data, int data_size, int offset,
                                float pixel_height, unsigned char *pixels, int pw, int ph,
                                int first_char, int num_chars, stt_bakedchar *chardata)
{
   return stt_BakeFontBitmap_internal((unsigned char *) data, data_size, offset, pixel_height, pixels, pw, ph, first_char, num_chars, chardata);
}

STT_DEF int stt_GetFontOffsetForIndex(const unsigned char *data, int data_size, int index)
{
   return stt_GetFontOffsetForIndex_internal((unsigned char *) data, data_size, index);
}

STT_DEF int stt_GetNumberOfFonts(const unsigned char *data, int data_size)
{
   return stt_GetNumberOfFonts_internal((unsigned char *) data, data_size);
}

STT_DEF int stt_InitFont(stt_fontinfo *info, const unsigned char *data, int data_size, int offset)
{
   return stt_InitFont_internal(info, (unsigned char *) data, data_size, offset);
}

STT_DEF int stt_FindMatchingFont(const unsigned char *fontdata, int fontdata_size, const char *name, int flags)
{
   return stt_FindMatchingFont_internal((unsigned char *) fontdata, fontdata_size, (char *) name, flags);
}

STT_DEF int stt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2)
{
   return stt_CompareUTF8toUTF16_bigendian_internal((char *) s1, len1, (char *) s2, len2);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // STRUETYPE_IMPLEMENTATION


// FULL VERSION HISTORY
//
//   1.25 (2021-07-11) many fixes
//   1.24 (2020-02-05) fix warning
//   1.23 (2020-02-02) query SVG data for glyphs; query whole kerning table (but only kern not GPOS)
//   1.22 (2019-08-11) minimize missing-glyph duplication; fix kerning if both 'GPOS' and 'kern' are defined
//   1.21 (2019-02-25) fix warning
//   1.20 (2019-02-07) PackFontRange skips missing codepoints; GetScaleFontVMetrics()
//   1.19 (2018-02-11) OpenType GPOS kerning (horizontal only), STBTT_fmod
//   1.18 (2018-01-29) add missing function
//   1.17 (2017-07-23) make more arguments const; doc fix
//   1.16 (2017-07-12) SDF support
//   1.15 (2017-03-03) make more arguments const
//   1.14 (2017-01-16) num-fonts-in-TTC function
//   1.13 (2017-01-02) support OpenType fonts, certain Apple fonts
//   1.12 (2016-10-25) suppress warnings about casting away const with -Wcast-qual
//   1.11 (2016-04-02) fix unused-variable warning
//   1.10 (2016-04-02) allow user-defined fabs() replacement
//                     fix memory leak if fontsize=0.0
//                     fix warning from duplicate typedef
//   1.09 (2016-01-16) warning fix; avoid crash on outofmem; use alloc userdata for PackFontRanges
//   1.08 (2015-09-13) document stbtt_Rasterize(); fixes for vertical & horizontal edges
//   1.07 (2015-08-01) allow PackFontRanges to accept arrays of sparse codepoints;
//                     allow PackFontRanges to pack and render in separate phases;
//                     fix stbtt_GetFontOFfsetForIndex (never worked for non-0 input?);
//                     fixed an assert() bug in the new rasterizer
//                     replace assert() with STBTT_assert() in new rasterizer
//   1.06 (2015-07-14) performance improvements (~35% faster on x86 and x64 on test machine)
//                     also more precise AA rasterizer, except if shapes overlap
//                     remove need for STBTT_sort
//   1.05 (2015-04-15) fix misplaced definitions for STBTT_STATIC
//   1.04 (2015-04-15) typo in example
//   1.03 (2015-04-12) STBTT_STATIC, fix memory leak in new packing, various fixes
//   1.02 (2014-12-10) fix various warnings & compile issues w/ stb_rect_pack, C++
//   1.01 (2014-12-08) fix subpixel position when oversampling to exactly match
//                        non-oversampled; STBTT_POINT_SIZE for packed case only
//   1.00 (2014-12-06) add new PackBegin etc. API, w/ support for oversampling
//   0.99 (2014-09-18) fix multiple bugs with subpixel rendering (ryg)
//   0.9  (2014-08-07) support certain mac/iOS fonts without an MS platformID
//   0.8b (2014-07-07) fix a warning
//   0.8  (2014-05-25) fix a few more warnings
//   0.7  (2013-09-25) bugfix: subpixel glyph bug fixed in 0.5 had come back
//   0.6c (2012-07-24) improve documentation
//   0.6b (2012-07-20) fix a few more warnings
//   0.6  (2012-07-17) fix warnings; added stbtt_ScaleForMappingEmToPixels,
//                        stt_GetFontBoundingBox, stt_IsGlyphEmpty
//   0.5  (2011-12-09) bugfixes:
//                        subpixel glyph renderer computed wrong bounding box
//                        first vertex of shape can be off-curve (FreeSans)
//   0.4b (2011-12-03) fixed an error in the font baking example
//   0.4  (2011-12-01) kerning, subpixel rendering (tor)
//                    bugfixes for:
//                        codepoint-to-glyph conversion using table fmt=12
//                        codepoint-to-glyph conversion using table fmt=4
//                        stt_GetBakedQuad with non-square texture (Zer)
//                    updated Hello World! sample to use kerning and subpixel
//                    fixed some warnings
//   0.3  (2009-06-24) cmap fmt=12, compound shapes (MM)
//                    userdata, malloc-from-userdata, non-zero fill (stb)
//   0.2  (2009-03-11) Fix unsigned/signed char warnings
//   0.1  (2009-03-09) First public release
//

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
