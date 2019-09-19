/*
 * Translation to libbn data types of Franklin's point-in-polygon test.
 * See https://wrf.ecse.rpi.edu//Research/Short_Notes/pnpoly.html
 * for a discussion of the subtleties involved with the inequality tests.
 * The below copyright applies to just the function bg_pnt_in_polygon,
 * not the whole of polygon.c
 *
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimers.  Redistributions
 * in binary form must reproduce the above copyright notice in the
 * documentation and/or other materials provided with the distribution.
 * The name of W. Randolph Franklin may not be used to endorse or promote
 * products derived from this Software without specific prior written
 * permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "common.h"
#include <stdlib.h>
#include "bg/polygon.h"

int
bg_pnt_in_polygon(size_t nvert, const point2d_t *pnts, const point2d_t *test)
{
    size_t i = 0;
    size_t j = 0;
    int c = 0;
    for (i = 0, j = nvert-1; i < nvert; j = i++) {
	if ( ((pnts[i][1] > (*test)[1]) != (pnts[j][1] > (*test)[1])) &&
	     ((*test)[0] < (pnts[j][0]-pnts[i][0]) * ((*test)[1]-pnts[i][1]) / (pnts[j][1]-pnts[i][1]) + pnts[i][0]) )
	    c = !c;
    }
    return c;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
