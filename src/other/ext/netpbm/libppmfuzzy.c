/*=============================================================================
  This file contains fuzzy color matching code.

  It is all based on algorithms and methods by Kenan Kalajdzic
  <kenan@unix.ba> in 2006.
=============================================================================*/

#include <string.h>
#include "pm_c_util.h"
#include "ppm.h"

typedef double fzLog;
    /* fuzzy logic truth value */

/*----------------------------------------------------------------------------
   Member functions
-----------------------------------------------------------------------------*/

static fzLog
memberS(fzLog const x1,
        fzLog const x2,
        fzLog const x) {

    fzLog retval;

    if (x <= x1)
        retval = 0;
    else if (x > x1 && x <= x2)
        retval = (x - x1) / (x2 - x1);
    else
        retval = 1;

    return retval;
}



static fzLog
memberZ(fzLog const x1,
        fzLog const x2,
        fzLog const x) {

    fzLog retval;

    if (x <= x1)
        retval = 1;
    else if (x > x1 && x <= x2)
        retval = (x2 - x) / (x2 - x1);
    else
        retval = 0;

    return retval;
}



static fzLog
memberTrapez(fzLog const x1,
             fzLog const x2,
             fzLog const x3,
             fzLog const x4,
             fzLog const x) {

    fzLog retval;
    
    if (x <= x1 || x > x4)
        retval = 0;
    else if (x > x1 && x <= x2)
        retval = (x - x1) / (x2 - x1);
    else if (x > x2 && x <= x3)
        retval = 1;
    else
        retval = (x4 - x) / (x4 - x3);

    return retval;
}



/*----------------------------------------------------------------------------
   Hue membership functions
-----------------------------------------------------------------------------*/

static fzLog
hueIsAround000(double const hue) {

    return memberZ(10, 30, hue);
}



static fzLog
hueIsAround015(double const hue) {

    return memberZ(30, 40, hue);
}



static fzLog
hueIsAround030(double const hue) {

    return memberTrapez(10, 30, 40, 60, hue);
}



static fzLog
hueIsAround060(double const hue) {

    return memberTrapez(40, 60, 60, 80, hue);
}



static fzLog
hueIsAround120(double const hue) {

    return memberTrapez(60, 80, 150, 180, hue);
}



static fzLog
hueIsAround180(double const hue) {

    return memberTrapez(150, 180, 240, 260, hue);
}



static fzLog
hueIsAround270(double const hue) {

    return memberTrapez(240, 260, 290, 310, hue);
}



static fzLog
hueIsAround320(double const hue) {

    return memberTrapez(290, 310, 320, 350, hue);
}



static fzLog
hueIsAround360(double const hue) {

    return memberS(320, 350, hue);
}



/*----------------------------------------------------------------------------
   Saturation membership functions
-----------------------------------------------------------------------------*/

static fzLog
satIsVeryLow(double const sat) {

    return memberZ(0.02, 0.1, sat);
}



static fzLog
satIsLow(double const sat) {
    return memberTrapez(0.02, 0.1, 0.2, 0.3, sat);
}



static fzLog
satIsMedium(double const sat) {

    return memberTrapez(0.2, 0.3, 0.6, 0.7, sat);
}



static fzLog
satIsHigh(double const sat) {

    return memberS(0.6, 0.7, sat);
}



/*----------------------------------------------------------------------------
   Value membership functions
-----------------------------------------------------------------------------*/

static fzLog
valIsVeryLow(double const val) {

    return memberZ(0.1, 0.2, val);
}



static fzLog
valIsLow(double const val) {

    return memberTrapez(0.1, 0.2, 0.3, 0.6, val);
}



static fzLog
valIsMedium(double const val) {

    return memberTrapez(0.3, 0.6, 0.7, 0.8, val);
}



static fzLog
valIsHigh(double const val) {

    return memberS(0.7, 0.8, val);
}



/*----------------------------------------------------------------------------
   Fuzzy logic functions
-----------------------------------------------------------------------------*/

static fzLog
fzAnd(fzLog const opLeft,
      fzLog const opRight) {

    return MIN(opLeft, opRight);
}



static fzLog
fzOr(fzLog const opLeft,
     fzLog const opRight) {

    return MAX(opLeft, opRight);
}



static fzLog
fzNot(fzLog const op) {
    return 1.0 - op;
}



/*----------------------------------------------------------------------------
   Fuzzy color matching
-----------------------------------------------------------------------------*/

static void
matchBk(pixel     const color,
        pixval    const maxval,
        fzLog (* const bkMatchP)[BKCOLOR_COUNT]) {

    struct hsv const hsv = ppm_hsv_from_color(color, maxval);

    fzLog const satVeryLow = satIsVeryLow(hsv.s);
    fzLog const satLow     = satIsLow(hsv.s);
    fzLog const satMedium  = satIsMedium(hsv.s);
    fzLog const satHigh    = satIsHigh(hsv.s);

    fzLog const valVeryLow = valIsVeryLow(hsv.v);
    fzLog const valLow     = valIsLow(hsv.v);
    fzLog const valMedium  = valIsMedium(hsv.v);
    fzLog const valHigh    = valIsHigh(hsv.v);

    fzLog const hueAround000 = hueIsAround000(hsv.h);
    fzLog const hueAround015 = hueIsAround015(hsv.h);
    fzLog const hueAround030 = hueIsAround030(hsv.h);
    fzLog const hueAround060 = hueIsAround060(hsv.h);
    fzLog const hueAround120 = hueIsAround120(hsv.h);
    fzLog const hueAround180 = hueIsAround180(hsv.h);
    fzLog const hueAround270 = hueIsAround270(hsv.h);
    fzLog const hueAround320 = hueIsAround320(hsv.h);
    fzLog const hueAround360 = hueIsAround360(hsv.h);

    (*bkMatchP)[BKCOLOR_BLACK]  =
        fzAnd(fzOr(satVeryLow, satLow), valVeryLow);

    (*bkMatchP)[BKCOLOR_GRAY]   =
        fzAnd(fzOr(satVeryLow, satLow), fzOr(valLow, valMedium));

    (*bkMatchP)[BKCOLOR_WHITE]  =
        fzAnd(fzOr(satVeryLow, satLow), valHigh);
    
    (*bkMatchP)[BKCOLOR_RED]    =
        fzAnd(fzAnd(fzOr(hueAround000, hueAround360), fzNot(satVeryLow)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_ORANGE] =
        fzAnd(fzAnd(hueAround030, fzOr(satMedium, satHigh)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_YELLOW] =
        fzAnd(fzAnd(hueAround060, fzOr(satMedium, satHigh)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_GREEN]  =
        fzAnd(fzAnd(hueAround120, fzNot(satVeryLow)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_BLUE]   =
        fzAnd(fzAnd(hueAround180, fzAnd(fzNot(satVeryLow), fzNot(satLow))),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_VIOLET] =
        fzAnd(fzAnd(hueAround270, fzNot(satVeryLow)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_PURPLE] =
        fzAnd(fzAnd(hueAround320, fzNot(satVeryLow)),
              fzOr(valMedium, valHigh)
             );

    (*bkMatchP)[BKCOLOR_BROWN]  =
        fzAnd(fzOr(hueAround015, hueAround360),
              fzAnd(fzNot(satVeryLow), fzNot(valHigh))
             );
}



bk_color
ppm_bk_color_from_color(pixel  const color,
                        pixval const maxval) {

    fzLog bkmatch[BKCOLOR_COUNT];
    bk_color i;
    bk_color bestSoFar;
    fzLog bestMatch;

    matchBk(color, maxval, &bkmatch);

    for (i = 0, bestSoFar = 0, bestMatch = 0.0; i < BKCOLOR_COUNT; ++i) {
        if (bkmatch[i] > bestMatch) {
            bestSoFar = i;
            bestMatch = bkmatch[i];
        }
    }
    return bestSoFar;
}



static pixel const bkColorMap[BKCOLOR_COUNT] = {
    {  0,   0,   0}, /* BKCOLOR_BLACK  */
    {174, 174, 174}, /* BKCOLOR_GRAY   */
    {255, 255, 255}, /* BKCOLOR_WHITE  */
    {255,   0,   0}, /* BKCOLOR_RED    */
    {255, 128,   0}, /* BKCOLOR_ORANGE */
    {255, 255,   0}, /* BKCOLOR_YELLOW */
    {  0, 255,   0}, /* BKCOLOR_GREEN  */
    {  0,   0, 255}, /* BKCOLOR_BLUE   */
    {143,  94, 153}, /* BKCOLOR_VIOLET */
    {160,  32, 240}, /* BKCOLOR_PURPLE */
    {128,  42,  42}  /* BKCOLOR_BROWN  */
};



pixel
ppm_color_from_bk_color(bk_color const bkColor,
                        pixval   const maxval) {

    pixel const color255 = bkColorMap[bkColor];

    pixel retval;

    if (maxval != 255) {
        PPM_DEPTH(retval, color255, 255, maxval);
    } else
        retval = color255;

    return retval;
}



static const char * const bkColorNameMap[BKCOLOR_COUNT] = {
    "black",
    "gray",
    "white",
    "red",
    "orange",
    "yellow",
    "green",
    "blue",
    "violet",
    "purple",
    "brown"
};



bk_color
ppm_bk_color_from_name(const char * const name) {

    bk_color i;

    for (i = 0; i < BKCOLOR_COUNT; ++i) {
        if (!strcmp(name, bkColorNameMap[i]))
            return i;
    }
    pm_error("Invalid Berlin-Kay color name: '%s'", name);
    return 0;  /* quiet compiler warning */
}



const char *
ppm_name_from_bk_color(bk_color const bkColor) {

    if (bkColor >= BKCOLOR_COUNT)
        pm_error("Invalid color passed to name_from_bk_color(): %u",
                 bkColor);

    return bkColorNameMap[bkColor];
}
