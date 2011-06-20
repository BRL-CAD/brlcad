/*                       F O R T R A N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup fort */
/** @{ */
/** @file libbn/fortran.c
 *
 * @brief
 * A FORTRAN-callable interface to libplot3.
 *
 * A FORTRAN-callable interface to libplot3, which is a public-domain
 * UNIX plot library, for 2-D and 3-D plotting in 16-bit signed
 * integer spaces, and in floating point.
 *
 * Note that all routines which expect floating point parameters
 * currently expect them to be of type "float" (single precision) so
 * that all FORTRAN constants can be written normally, rather than
 * having to insist on FORTRAN "double precision" parameters.  This is
 * at odds with the C routines and the meta-file format, which both
 * operate in "C double" precision.
 *
 * Note that on machines like the Cray,
 * (C float == C double == FORTRAN REAL) != FORTRAN DOUBLE PRECISION
 *
 * Also note that on the Cray, the only interface provision required
 * is that the subroutine name be in all upper case.  Other systems
 * may have different requirements, such as adding a leading
 * underscore.  It is not clear how to handle this in a general way.
 *
 * Note that due to the 6-character name space required to be
 * generally useful in the FORTRAN environment, the names have been
 * shortened.  At the same time, a consistency of naming has been
 * implemented; the first character or two giving a clue as to the
 * purpose of the subroutine:
 *
 *@li I	General routines, and integer-parameter routines
 *@li I2 Routines with enumerated 2-D integer parameters
 *@li I3 Routines with enumerated 3-D integer parameters
 *@li F2 Routines with enumerated 2-D float parameters
 *@li F3 Routines with enumerated 3-D float parameters
 *@li A3 Routines with arrays of 3-D float parameters
 *
 * This name space leaves the door open for a double-precision family
 * of routines, D, D2, and D3.
 *
 */

#include "common.h"

#include <stdio.h>

#include "plot3.h"


/**
 * P L _ S T R N C P Y
 *
 * Make null-terminated copy of a string in output buffer, being
 * careful not to exceed indicated buffer size Accept "$" as alternate
 * string-terminator for FORTRAN Holerith constants, because getting
 * FORTRAN to null-terminate strings is to painful (and non-portable)
 * to contemplate.
 */
void
pl_strncpy(register char *out, register char *in, register int sz)
{
    register int c = '\0';

    while (--sz > 0 && (c = *in++) != '\0' && c != '$')
	*out++ = c;
    *out++ = '\0';
}


/**
 * Macro 'F' is used to take the 'C' function name, and convert it to
 * the convention used for a particular system.  Both lower-case and
 * upper-case alternatives have to be provided because there is no way
 * to get the C preprocessor to change the case of a token.
 *
 * Lower case, with a trailing underscore.
 */
#define F(lc, uc) lc ## _

/*
 * These interfaces provide necessary access to C library routines
 * from the FORTRAN environment
 */


/**
 * I F O P E N
 *
 * Open a file (by name) for plotting.
 */
void
F(ifopen, IFOPEN)(FILE **plotfp, char *name)
{
    char buf[128];

    pl_strncpy(buf, name, (int)sizeof(buf));
    if ((*plotfp = fopen(buf, "wb")) == NULL)
	perror(buf);
}

/*
 * These interfaces provide the standard UNIX-Plot functionality
 */

void
F(i2pnt, I2PNT)(FILE **plotfp, int *x, int *y)
{
    pl_point(*plotfp, *x, *y);
}

void
F(i2line, I2LINE)(FILE **plotfp, int *px1, int *py1, int *px2, int *py2)
{
    pl_line(*plotfp, *px1, *py1, *px2, *py2);
}

void
F(ilinmd, ILINMD)(FILE **plotfp, char *s)
{
    char buf[32];
    pl_strncpy(buf, s, (int)sizeof(buf));
    pl_linmod(*plotfp, buf);
}

void
F(i2move, I2MOVE)(FILE **plotfp, int *x, int *y)
{
    pl_move(*plotfp, *x, *y);
}

void
F(i2cont, I2CONT)(FILE **plotfp, int *x, int *y)
{
    pl_cont(*plotfp, *x, *y);
}

void
F(i2labl, I2LABL)(FILE **plotfp, char *s)
{
    char buf[256];
    pl_strncpy(buf, s, (int)sizeof(buf));
    pl_label(*plotfp, buf);
}

void
F(i2spac, I2SPAC)(FILE **plotfp, int *px1, int *py1, int *px2, int *py2)
{
    pl_space(*plotfp, *px1, *py1, *px2, *py2);
}

void
F(ierase, IERASE)(FILE **plotfp)
{
    pl_erase(*plotfp);
}

void
F(i2circ, I2CIRC)(FILE **plotfp, int *x, int *y, int *r)
{
    pl_circle(*plotfp, *x, *y, *r);
}

void
F(i2arc, I2ARC)(FILE **plotfp, int *xc, int *yc, int *px1, int *py1, int *px2, int *py2)
{
    pl_arc(*plotfp, *xc, *yc, *px1, *py1, *px2, *py2);
}

void
F(i2box, I2BOX)(FILE **plotfp, int *px1, int *py1, int *px2, int *py2)
{
    pl_box(*plotfp, *px1, *py1, *px2, *py2);
}

/*
 * Here lie the BRL 3-D extensions.
 */

/* Warning: r, g, b are ints.  The output is chars. */
void
F(icolor, ICOLOR)(FILE **plotfp, int *r, int *g, int *b)
{
    pl_color(*plotfp, *r, *g, *b);
}

void
F(iflush, IFLUSH)(FILE **plotfp)
{
    pl_flush(*plotfp);
}

void
F(i3spac, I3SPAC)(FILE **plotfp, int *px1, int *py1, int *pz1, int *px2, int *py2, int *pz2)
{
    pl_3space(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

void
F(i3pnt, I3PNT)(FILE **plotfp, int *x, int *y, int *z)
{
    pl_3point(*plotfp, *x, *y, *z);

}

void
F(i3move, I3MOVE)(FILE **plotfp, int *x, int *y, int *z)
{
    pl_3move(*plotfp, *x, *y, *z);
}

void
F(i3cont, I3CONT)(FILE **plotfp, int *x, int *y, int *z)
{
    pl_3cont(*plotfp, *x, *y, *z);
}

void
F(i3line, I3LINE)(FILE **plotfp, int *px1, int *py1, int *pz1, int *px2, int *py2, int *pz2)
{
    pl_3line(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

void
F(i3box, I3BOX)(FILE **plotfp, int *px1, int *py1, int *pz1, int *px2, int *py2, int *pz2)
{
    pl_3box(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

/*
 * Floating point routines.
 */

void
F(f2pnt, F2PNT)(FILE **plotfp, float *x, float *y)
{
    pd_point(*plotfp, *x, *y);
}

void
F(f2line, F2LINE)(FILE **plotfp, float *px1, float *py1, float *px2, float *py2)
{
    pd_line(*plotfp, *px1, *py1, *px2, *py2);
}

void
F(f2move, F2MOVE)(FILE **plotfp, float *x, float *y)
{
    pd_move(*plotfp, *x, *y);
}

void
F(f2cont, F2CONT)(FILE **plotfp, float *x, float *y)
{
    pd_cont(*plotfp, *x, *y);
}

void
F(f2spac, F2SPAC)(FILE **plotfp, float *px1, float *py1, float *px2, float *py2)
{
    pd_space(*plotfp, *px1, *py1, *px2, *py2);
}

void
F(f2circ, F2CIRC)(FILE **plotfp, float *x, float *y, float *r)
{
    pd_circle(*plotfp, *x, *y, *r);
}

void
F(f2arc, F2ARC)(FILE **plotfp, float *xc, float *yc, float *px1, float *py1, float *px2, float *py2)
{
    pd_arc(*plotfp, *xc, *yc, *px1, *py1, *px2, *py2);
}

void
F(f2box, F2BOX)(FILE **plotfp, float *px1, float *py1, float *px2, float *py2)
{
    pd_box(*plotfp, *px1, *py1, *px2, *py2);
}

/*
 * Floating-point 3-D, both in array (vector) and enumerated versions.
 * The same remarks about float/double apply as above.
 */

void
F(a2spac, A3SPAC)(FILE **plotfp, float min[3], float max[3])
{
    pd_3space(*plotfp, min[0], min[1], min[2], max[0], max[1], max[2]);
}

void
F(f3spac, F3SPAC)(FILE **plotfp, float *px1, float *py1, float *pz1, float *px2, float *py2, float *pz2)
{
    pd_3space(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

void
F(a3pnt, A3PNT)(FILE **plotfp, float pt[3])
{
    pd_3point(*plotfp, pt[0], pt[1], pt[2]);
}

void
F(f3pnt, F3PNT)(FILE **plotfp, float *x, float *y, float *z)
{
    pd_3point(*plotfp, *x, *y, *z);
}

void
F(a3move, A3MOVE)(FILE **plotfp, float pt[3])
{
    pd_3move(*plotfp, pt[0], pt[1], pt[2]);
}

void
F(f3move, F3MOVE)(FILE **plotfp, float *x, float *y, float *z)
{
    pd_3move(*plotfp, *x, *y, *z);
}

void
F(a3cont, A3CONT)(FILE **plotfp, float pt[3])
{
    pd_3cont(*plotfp, pt[0], pt[1], pt[2]);
}

void
F(f3cont, F3CONT)(FILE **plotfp, float *x, float *y, float *z)
{
    pd_3cont(*plotfp, *x, *y, *z);
}

void
F(a3line, A3LINE)(FILE **plotfp, float a[3], float b[3])
{
    pd_3line(*plotfp, a[0], a[1], a[2], b[0], b[1], b[2]);
}

void
F(f3line, F3LINE)(FILE **plotfp, float *px1, float *py1, float *pz1, float *px2, float *py2, float *pz2)
{
    pd_3line(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

void
F(a3box, A3BOX)(FILE **plotfp, float a[3], float b[3])
{
    pd_3box(*plotfp, a[0], a[1], a[2], b[0], b[1], b[2]);
}

void
F(f3box, F3BOX)(FILE **plotfp, float *px1, float *py1, float *pz1, float *px2, float *py2, float *pz2)
{
    pd_3box(*plotfp, *px1, *py1, *pz1, *px2, *py2, *pz2);
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
