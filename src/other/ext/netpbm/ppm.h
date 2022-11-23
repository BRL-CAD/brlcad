/* Interface header file for PPM-related functions in libnetpbm */

#ifndef _PPM_H_
#define _PPM_H_

#if defined(_WIN32)
# define COMPILER_DLLEXPORT __declspec(dllexport)
# define COMPILER_DLLIMPORT __declspec(dllimport)
#else
# define COMPILER_DLLEXPORT __attribute__ ((visibility ("default")))
# define COMPILER_DLLIMPORT __attribute__ ((visibility ("default")))
#endif

#ifndef NETPBM_EXPORT
#  if defined(NETPBM_DLL_EXPORTS) && defined(NETPBM_DLL_IMPORTS)
#    error "Only NETPBM_DLL_EXPORTS or NETPBM_DLL_IMPORTS can be defined, not both."
#  elif defined(NETPBM_DLL_EXPORTS)
#    define NETPBM_EXPORT COMPILER_DLLEXPORT
#  elif defined(NETPBM_DLL_IMPORTS)
#    define NETPBM_EXPORT COMPILER_DLLIMPORT
#  else
#    define NETPBM_EXPORT
#  endif
#endif

#include "pm_config.h"
#include "pm.h"
#include "pgm.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

typedef gray pixval;

/* These are just for use in this header file */
#define PPM_MAX(a,b) ((a) > (b) ? (a) : (b))
#define PPM_MIN(a,b) ((a) < (b) ? (a) : (b))


#define PPM_OVERALLMAXVAL PGM_OVERALLMAXVAL
#define PPM_MAXMAXVAL PGM_MAXMAXVAL
typedef struct {
    pixval r, g, b;
} pixel;
#define PPM_GETR(p) ((p).r)
#define PPM_GETG(p) ((p).g)
#define PPM_GETB(p) ((p).b)

/************* added definitions *****************/
#define PPM_PUTR(p,red) ((p).r = (red))
#define PPM_PUTG(p,grn) ((p).g = (grn))
#define PPM_PUTB(p,blu) ((p).b = (blu))
/**************************************************/

#define PPM_ASSIGN(p,red,grn,blu) \
  do { (p).r = (red); (p).g = (grn); (p).b = (blu); } while (0)
#define PPM_EQUAL(p,q) \
  ( (p).r == (q).r && (p).g == (q).g && (p).b == (q).b )


/* Magic constants. */

#define PPM_MAGIC1 'P'
#define PPM_MAGIC2 '3'
#define RPPM_MAGIC2 '6'
#define PPM_FORMAT (PPM_MAGIC1 * 256 + PPM_MAGIC2)
#define RPPM_FORMAT (PPM_MAGIC1 * 256 + RPPM_MAGIC2)
#define PPM_TYPE PPM_FORMAT


#include "ppmcmap.h"

/* Macro for turning a format number into a type number. */

#define PPM_FORMAT_TYPE(f) \
  ((f) == PPM_FORMAT || (f) == RPPM_FORMAT ? PPM_TYPE : PGM_FORMAT_TYPE(f))


static __inline__ pixel
ppm_whitepixel(pixval maxval) {

    pixel retval;
    PPM_ASSIGN(retval, maxval, maxval, maxval);

    return retval;
}

static __inline__ pixel
ppm_blackpixel(void) {

    pixel const retval = {0, 0, 0};

    return retval;
}

void ppm_init(int * const argcP, char ** const argv);

#define ppm_allocarray(cols, rows) \
  ((pixel**) pm_allocarray(cols, rows, sizeof(pixel)))

NETPBM_EXPORT pixel *
ppm_allocrow(unsigned int const cols);

#define ppm_freearray(pixels, rows) pm_freearray((char**) pixels, rows)

#define ppm_freerow(pixelrow) pm_freerow(pixelrow);

pixel**
ppm_readppm(FILE *   const fileP, 
            int *    const colsP, 
            int *    const rowsP, 
            pixval * const maxvalP);

NETPBM_EXPORT void
ppm_readppminit(FILE *   const fileP, 
                int *    const colsP, 
                int *    const rowsP, 
                pixval * const maxvalP, 
                int *    const formatP);

NETPBM_EXPORT void
ppm_readppmrow(FILE*  const fileP, 
               pixel* const pixelrow, 
               int    const cols, 
               pixval const maxval, 
               int    const format);

void
ppm_writeppm(FILE *  const fileP, 
             pixel** const pixels, 
             int     const cols, 
             int     const rows, 
             pixval  const maxval, 
             int     const forceplain);

NETPBM_EXPORT void
ppm_writeppminit(FILE*  const fileP, 
                 int    const cols, 
                 int    const rows, 
                 pixval const maxval, 
                 int    const forceplain);

NETPBM_EXPORT void
ppm_writeppmrow(FILE *        const fileP, 
                const pixel * const pixelrow, 
                int           const cols, 
                pixval        const maxval, 
                int           const forceplain);

void
ppm_check(FILE *               const fileP, 
          enum pm_check_type   const check_type, 
          int                  const format, 
          int                  const cols, 
          int                  const rows, 
          pixval               const maxval,
          enum pm_check_code * const retval_p);

void
ppm_nextimage(FILE * const fileP, 
              int *  const eofP);

pixel 
ppm_parsecolor(const char * const colorname,
               pixval       const maxval);

pixel
ppm_parsecolor2(const char * const colorname,
                pixval       const maxval,
                int          const closeOk);

char*
ppm_colorname(const pixel* const colorP, 
              pixval       const maxval, 
              int          const hexok);

void
ppm_readcolordict(const char *      const fileName,
                  int               const mustOpen,
                  unsigned int *    const nColorsP,
                  const char ***    const colornamesP,
                  pixel **          const colorsP,
                  colorhash_table * const chtP);

void
ppm_readcolornamefile(const char *      const fileName, 
                      int               const mustOpen,
                      colorhash_table * const chtP, 
                      const char ***    const colornamesP);

void
ppm_freecolornames(const char ** const colornames);

#define PPM_ISGRAY(pixel) \
    (PPM_GETR(pixel) == PPM_GETG(pixel) && PPM_GETR(pixel) == PPM_GETB(pixel))

/* Color scaling macro -- to make writing ppmtowhatever easier. */

#define PPM_DEPTH(newp,p,oldmaxval,newmaxval) \
    PPM_ASSIGN( (newp), \
	( (int) PPM_GETR(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval), \
	( (int) PPM_GETG(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval), \
	( (int) PPM_GETB(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval) )

#define PPM_SQR(x) (x)*(x)

static __inline__ unsigned int
PPM_DISTANCE(pixel const p1,
             pixel const p2) {
    return (
        PPM_SQR(PPM_GETR(p1)-PPM_GETR(p2)) +
        PPM_SQR(PPM_GETG(p1)-PPM_GETG(p2)) +
        PPM_SQR(PPM_GETB(p1)-PPM_GETB(p2)) );
}
#undef PPM_SQR

/* Note that because a sample can be at most 1 << 16 - 1, PPM_DISTANCE
   is less than UINT_MAX.  That means you can use UINT_MAX as an infinite
   distance in some applications.
*/

/* Luminance, Chrominance macros. */

/* The following are weights of the red, green, and blue components
   respectively in the luminance of a color.  Actually, it's "luma,"
   not luminance, the difference being that luminance would be a linear
   combination of intensities, whereas luma is a linear combination of
   gamma-adjusted intensities, as you would find in a Netpbm image.

   These are from ITU-R BT.601.5.  That means they probably aren't technically
   right for use with PPM images, because the PPM spec says ITU-R BT.709.
   The two are similar, though.
*/
#define PPM_LUMINR (0.2989)
#define PPM_LUMING (0.5866)
#define PPM_LUMINB (0.1145)

#define PPM_LUMIN(p) ( PPM_LUMINR * PPM_GETR(p) \
                       + PPM_LUMING * PPM_GETG(p) \
                       + PPM_LUMINB * PPM_GETB(p) )
#define PPM_CHROM_B(p) ( -0.16874 * PPM_GETR(p) \
                         - 0.33126 * PPM_GETG(p) \
                         + 0.5 * PPM_GETB(p) )
#define PPM_CHROM_R(p) ( 0.5 * PPM_GETR(p) \
                         - 0.41869 * PPM_GETG(p) \
                         - 0.08131 * PPM_GETB(p) )

pixel
ppm_color_from_ycbcr(unsigned int const y, 
                     int          const cb, 
                     int          const cr);

/* Hue/Saturation/Value calculations */

struct hsv {
    double h;  /* hue (degrees)  0..360 */
    double s;  /* saturation (0-1) */
    double v;  /* value (0-1) */
};

pixel
ppm_color_from_hsv(struct hsv const hsv,
                   pixval     const maxval);

struct hsv
ppm_hsv_from_color(pixel  const color,
                   pixval const maxval);

static __inline__ pixval
ppm_luminosity(pixel const p) {

    return (pixval)(PPM_LUMIN(p) + 0.5);
}

static __inline__ pixval
ppm_colorvalue(pixel const p) {
/*----------------------------------------------------------------------------
  The color value (V is HSV) as a pixval
-----------------------------------------------------------------------------*/
    return PPM_MAX(PPM_GETR(p), PPM_MAX(PPM_GETG(p), PPM_GETB(p)));
}

static __inline__ pixval
ppm_saturation(pixel const p,
               pixval const maxval) {
/*----------------------------------------------------------------------------
  The saturation, as a pixval (i.e. if saturation is 50% and maxval
  is 100, this is 50).
-----------------------------------------------------------------------------*/
    pixval const maxIntensity =
        PPM_MAX(PPM_GETR(p), PPM_MAX(PPM_GETG(p), PPM_GETB(p)));
    pixval const minIntensity =
        PPM_MIN(PPM_GETR(p), PPM_MIN(PPM_GETG(p), PPM_GETB(p)));
    pixval const range = maxIntensity - minIntensity;

    return (pixval)((unsigned long)range * maxval / maxIntensity);
}

typedef enum {
    /* A color from the set of universally understood colors developed
       by Brent Berlin and Paul Kay.

       Algorithms in libnetpbm depend on the numerical representations
       of these values being as follows.
    */
    BKCOLOR_GRAY = 0,
    BKCOLOR_BROWN,
    BKCOLOR_ORANGE,
    BKCOLOR_RED,
    BKCOLOR_YELLOW,
    BKCOLOR_GREEN,
    BKCOLOR_BLUE,
    BKCOLOR_VIOLET,
    BKCOLOR_PURPLE,
    BKCOLOR_WHITE,
    BKCOLOR_BLACK
} bk_color;

#define BKCOLOR_COUNT (BKCOLOR_BLACK+1)

bk_color
ppm_bk_color_from_color(pixel  const color,
                        pixval const maxval);

pixel
ppm_color_from_bk_color(bk_color const bkColor,
                        pixval   const maxval);

bk_color
ppm_bk_color_from_name(const char * const name);

const char *
ppm_name_from_bk_color(bk_color const bkColor);

#ifdef __cplusplus
}
#endif

#undef PPM_MIN
#undef PPM_MAX

#endif /*_PPM_H_*/
