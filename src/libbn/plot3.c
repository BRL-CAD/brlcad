/*                         P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @addtogroup plot */
/** @{ */
/** @file libbn/plot3.c
 *
 * @brief A public-domain UNIX plot library, for 2-D and 3-D plotting
 * in 16-bit VAX signed integer spaces, or 64-bit IEEE floating point.
 *
 * These routines generate "UNIX plot" output (with the addition of
 * 3-D commands).  They behave almost exactly like the regular libplot
 * routines, except:
 *
 * -# These all take a stdio file pointer, and can thus be used to
 *    create multiple plot files simultaneously.
 * -# There are 3-D versions of most commands.
 * -# There are IEEE floating point versions of the commands.
 * -# The names have been changed.
 *
 * The 3-D extensions are those of Doug Gwyn, from his System V
 * extensions.
 *
 * These are the ascii command letters allocated to various actions.
 * Care has been taken to consistently match lowercase and uppercase
 * letters.
 *
 * @code
 2d	3d	2df	3df
 space		s	S	w	W
 move		m	M	o	O
 cont		n	N	q	Q
 point		p	P	x	X
 line		l	L	v	V
 circle		c		i
 arc		a		r
 linmod		f
 label		t
 erase		e
 color		C
 flush		F

 bd gh jk  uyz
 ABDEGHIJKRTUYZ

 @endcode
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu/cv.h"
#include "vmath.h"
#include "bn/plot3.h"


static int pl_outputMode = PL_OUTPUT_MODE_BINARY;

/* For the sake of efficiency, we trust putc() to write only one byte */
#define putsi(a)	putc(a, plotfp); putc((a>>8), plotfp)

/* Making a common pd_3 to be used in pd_3cont and pd_3move */
void
pd_3(register FILE *plotfp, double x, double y, double z, char c)
{
    size_t ret;
    double in[3];
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = z;
	bu_cv_htond(&out[1], (unsigned char *)in, 3);

	out[0] = c;
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g %g\n", c, x, y, z);
    }
}

/* Making a common pdv_3 to be used in pdv_3cont and pdv_3move */
void
pdv_3(register FILE *plotfp, const fastf_t *pt, char c)
{
    size_t ret;
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)pt, 3);

	out[0] = c;
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g %g\n", c, V3ARGS(pt));
    }
}

/* Making a common pd to be used in pd_cont and pd_move */
void
pd(register FILE *plotfp, double x, double y, char c)
{
    size_t ret;
    double in[2];
    unsigned char out[2*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	bu_cv_htond(&out[1], (unsigned char *)in, 2);

	out[0] = c;
	ret = fwrite(out, 1, 2*8+1, plotfp);
	if (ret != 2*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g\n", c, x, y);
    }
}

/* Making a common pl_3 to be used in pl_3cont and pl_3move */
void
pl_3(register FILE *plotfp, int x, int y, int z, char c)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( c, plotfp);
	putsi(x);
	putsi(y);
	putsi(z);
    } else {
	fprintf(plotfp, "%c %d %d %d\n", c, x, y, z);
    }
}

/*
 * These interfaces provide the standard UNIX-Plot functionality
 */

int
pl_getOutputMode(void) {
    return pl_outputMode;
}

void
pl_setOutputMode(int mode) {
    pl_outputMode = mode;
}

/**
 * @brief
 * plot a point
 */
void
pl_point(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('p', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "p %d %d\n", x, y);
    }
}

void
pl_line(register FILE *plotfp, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('l', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "l %d %d %d %d\n", px1, py1, px2, py2);
    }
}

void
pl_linmod(register FILE *plotfp, const char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('f', plotfp);
	while (*s)
	    putc(*s++, plotfp);
	putc('\n', plotfp);
    } else {
	fprintf(plotfp, "f %s\n", s);
    }
}

void
pl_move(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('m', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "m %d %d\n", x, y);
    }
}

void
pl_cont(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('n', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "n %d %d\n", x, y);
    }
}

void
pl_label(register FILE *plotfp, const char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('t', plotfp);
	while (*s)
	    putc(*s++, plotfp);
	putc('\n', plotfp);
    } else {
	fprintf(plotfp, "t %s\n", s);
    }
}

void
pl_space(register FILE *plotfp, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('s', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "s %d %d %d %d\n", px1, py1, px2, py2);
    }
}

void
pl_erase(register FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY)
	putc('e', plotfp);
    else
	fprintf(plotfp, "e\n");
}

void
pl_circle(register FILE *plotfp, int x, int y, int r)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('c', plotfp);
	putsi(x);
	putsi(y);
	putsi(r);
    } else {
	fprintf(plotfp, "c %d %d %d\n", x, y, r);
    }
}

void
pl_arc(register FILE *plotfp, int xc, int yc, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('a', plotfp);
	putsi(xc);
	putsi(yc);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "a %d %d %d %d %d %d\n", xc, yc, px1, py1, px2, py2);
    }
}

void
pl_box(register FILE *plotfp, int px1, int py1, int px2, int py2)
{
    pl_move(plotfp, px1, py1);
    pl_cont(plotfp, px1, py2);
    pl_cont(plotfp, px2, py2);
    pl_cont(plotfp, px2, py1);
    pl_cont(plotfp, px1, py1);
    pl_move(plotfp, px2, py2);
}

/*
 * Here lie the BRL 3-D extensions.
 */

/* Warning: r, g, b are ints.  The output is chars. */
void
pl_color(register FILE *plotfp, int r, int g, int b)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('C', plotfp);
	putc(r, plotfp);
	putc(g, plotfp);
	putc(b, plotfp);
    } else {
	fprintf(plotfp, "C %d %d %d\n", r, g, b);
    }
}

void
pl_color_buc(register FILE *plotfp, struct bu_color *c)
{
    int r = 0;
    int g = 0;
    int b = 0;
    (void)bu_color_to_rgb_ints(c, &r, &g, &b);
    pl_color(plotfp, r, g, b);
}
void
pl_flush(register FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('F', plotfp);
    } else {
	fprintf(plotfp, "F\n");
    }

    fflush(plotfp);
}

void
pl_3space(register FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('S', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(pz1);
	putsi(px2);
	putsi(py2);
	putsi(pz2);
    } else {
	fprintf(plotfp, "S %d %d %d %d %d %d\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pl_3point(register FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'P'); /* calling common function pl_3 */
}

void
pl_3move(register FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'M'); /* calling common function pl_3 */
}

void
pl_3cont(register FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'N'); /* calling common function pl_3 */
}

void
pl_3line(register FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('L', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(pz1);
	putsi(px2);
	putsi(py2);
	putsi(pz2);
    } else {
	fprintf(plotfp, "L %d %d %d %d %d %d\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pl_3box(register FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    pl_3move(plotfp, px1, py1, pz1);
    /* first side */
    pl_3cont(plotfp, px1, py2, pz1);
    pl_3cont(plotfp, px1, py2, pz2);
    pl_3cont(plotfp, px1, py1, pz2);
    pl_3cont(plotfp, px1, py1, pz1);
    /* across */
    pl_3cont(plotfp, px2, py1, pz1);
    /* second side */
    pl_3cont(plotfp, px2, py2, pz1);
    pl_3cont(plotfp, px2, py2, pz2);
    pl_3cont(plotfp, px2, py1, pz2);
    pl_3cont(plotfp, px2, py1, pz1);
    /* front edge */
    pl_3move(plotfp, px1, py2, pz1);
    pl_3cont(plotfp, px2, py2, pz1);
    /* bottom back */
    pl_3move(plotfp, px1, py1, pz2);
    pl_3cont(plotfp, px2, py1, pz2);
    /* top back */
    pl_3move(plotfp, px1, py2, pz2);
    pl_3cont(plotfp, px2, py2, pz2);
}

/*
 * Double floating point versions
 */

void
pd_point(register FILE *plotfp, double x, double y)
{
    pd( plotfp, x, y, 'x'); /* calling common function pd */
}

void
pd_line(register FILE *plotfp, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[4];
    unsigned char out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = px2;
	in[3] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 4);

	out[0] = 'v';
	ret = fwrite(out, 1, 4*8+1, plotfp);
	if (ret != 4*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "v %g %g %g %g\n", px1, py1, px2, py2);
    }
}

/* Note: no pd_linmod(), just use pl_linmod() */

void
pd_move(register FILE *plotfp, double x, double y)
{
    pd( plotfp, x, y, 'o'); /* calling common function pd */
}

void
pd_cont(register FILE *plotfp, double x, double y)
{
    pd( plotfp, x, y, 'q'); /* calling common function pd */
}

void
pd_space(register FILE *plotfp, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[4];
    unsigned char out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = px2;
	in[3] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 4);

	out[0] = 'w';
	ret = fwrite(out, 1, 4*8+1, plotfp);
	if (ret != 4*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "w %g %g %g %g\n", px1, py1, px2, py2);
    }
}

void
pd_circle(register FILE *plotfp, double x, double y, double r)
{
    size_t ret;
    double in[3];
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = r;
	bu_cv_htond(&out[1], (unsigned char *)in, 3);

	out[0] = 'i';
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "i %g %g %g\n", x, y, r);
    }
}

void
pd_arc(register FILE *plotfp, double xc, double yc, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = xc;
	in[1] = yc;
	in[2] = px1;
	in[3] = py1;
	in[4] = px2;
	in[5] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'r';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "r %g %g %g %g %g %g\n", xc, yc, px1, py1, px2, py2);
    }
}

void
pd_box(register FILE *plotfp, double px1, double py1, double px2, double py2)
{
    pd_move(plotfp, px1, py1);
    pd_cont(plotfp, px1, py2);
    pd_cont(plotfp, px2, py2);
    pd_cont(plotfp, px2, py1);
    pd_cont(plotfp, px1, py1);
    pd_move(plotfp, px2, py2);
}

/* Double 3-D, both in vector and enumerated versions */
void
pdv_3space(register FILE *plotfp, const fastf_t *min, const fastf_t *max)
{
    size_t ret;
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)min, 3);
	bu_cv_htond(&out[3*8+1], (unsigned char *)max, 3);

	out[0] = 'W';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", V3ARGS(min), V3ARGS(max));
    }
}

void
pd_3space(register FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = pz1;
	in[3] = px2;
	in[4] = py2;
	in[5] = pz2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'W';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pdv_3point(register FILE *plotfp, const fastf_t *pt)
{
    pdv_3(plotfp, pt, 'X'); /* calling common function pdv_3 */
}

void
pd_3point(register FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'X'); /* calling common function pd_3 */
}

void
pdv_3move(register FILE *plotfp, const fastf_t *pt)
{
    pdv_3(plotfp, pt, 'O'); /* calling common function pdv_3 */
}

void
pd_3move(register FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'O'); /* calling common function pd_3 */
}

void
pdv_3cont(register FILE *plotfp, const fastf_t *pt)
{
    pdv_3(plotfp, pt, 'Q'); /* calling common function pdv_3 */
}

void
pd_3cont(register FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'Q'); /* calling common function pd_3 */
}

void
pdv_3line(register FILE *plotfp, const fastf_t *a, const fastf_t *b)
{
    size_t ret;
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)a, 3);
	bu_cv_htond(&out[3*8+1], (unsigned char *)b, 3);

	out[0] = 'V';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", V3ARGS(a), V3ARGS(b));
    }
}

void
pd_3line(register FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = pz1;
	in[3] = px2;
	in[4] = py2;
	in[5] = pz2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'V';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pdv_3box(register FILE *plotfp, const fastf_t *a, const fastf_t *b)
{
    pd_3move(plotfp, a[X], a[Y], a[Z]);
    /* first side */
    pd_3cont(plotfp, a[X], b[Y], a[Z]);
    pd_3cont(plotfp, a[X], b[Y], b[Z]);
    pd_3cont(plotfp, a[X], a[Y], b[Z]);
    pd_3cont(plotfp, a[X], a[Y], a[Z]);
    /* across */
    pd_3cont(plotfp, b[X], a[Y], a[Z]);
    /* second side */
    pd_3cont(plotfp, b[X], b[Y], a[Z]);
    pd_3cont(plotfp, b[X], b[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], a[Z]);
    /* front edge */
    pd_3move(plotfp, a[X], b[Y], a[Z]);
    pd_3cont(plotfp, b[X], b[Y], a[Z]);
    /* bottom back */
    pd_3move(plotfp, a[X], a[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], b[Z]);
    /* top back */
    pd_3move(plotfp, a[X], b[Y], b[Z]);
    pd_3cont(plotfp, b[X], b[Y], b[Z]);
}

void
pd_3box(register FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    pd_3move(plotfp, px1, py1, pz1);
    /* first side */
    pd_3cont(plotfp, px1, py2, pz1);
    pd_3cont(plotfp, px1, py2, pz2);
    pd_3cont(plotfp, px1, py1, pz2);
    pd_3cont(plotfp, px1, py1, pz1);
    /* across */
    pd_3cont(plotfp, px2, py1, pz1);
    /* second side */
    pd_3cont(plotfp, px2, py2, pz1);
    pd_3cont(plotfp, px2, py2, pz2);
    pd_3cont(plotfp, px2, py1, pz2);
    pd_3cont(plotfp, px2, py1, pz1);
    /* front edge */
    pd_3move(plotfp, px1, py2, pz1);
    pd_3cont(plotfp, px2, py2, pz1);
    /* bottom back */
    pd_3move(plotfp, px1, py1, pz2);
    pd_3cont(plotfp, px2, py1, pz2);
    /* top back */
    pd_3move(plotfp, px1, py2, pz2);
    pd_3cont(plotfp, px2, py2, pz2);
}

/**
 * Draw a ray
 */
void
pdv_3ray(FILE *fp, const fastf_t *pt, const fastf_t *dir, double t)
{
    point_t tip;

    VJOIN1(tip, pt, t, dir);
    pdv_3move(fp, pt);
    pdv_3cont(fp, tip);
}


/*
 * Routines to validate a plot file
 */

static int
read_short(FILE *fp, int cnt, int mode)
{
    if (mode == PL_OUTPUT_MODE_BINARY) {
	for (int i = 0; i < cnt * 2; i++) {
	    if (getc(fp) == EOF)
		return 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	int ret;
	double val;
	for (int i = 0; i < cnt; i++) {
	    ret = fscanf(fp, "%lf", &val);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }

    return 1;
}

static int
read_ieee(FILE *fp, int cnt, int mode)
{
    int ret;
    if (mode == PL_OUTPUT_MODE_BINARY) {
	for (int i = 0; i < cnt; i++) {
	    char inbuf[SIZEOF_NETWORK_DOUBLE];
	    ret = fread(inbuf, SIZEOF_NETWORK_DOUBLE, 1, fp);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	double val;
	for (int i = 0; i < cnt; i++) {
	    ret = fscanf(fp, "%lf", &val);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }
    return 1;
}

static int
read_tstring(FILE *fp, int mode)
{
    int ret;
    if (mode == PL_OUTPUT_MODE_BINARY) {
	int str_done = 0;
	int cc;
	while (!feof(fp) && !str_done) {
	    cc = getc(fp);
	    if (cc == '\n')
		str_done = 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	char carg[256];
	ret = fscanf(fp, "%256s\n", &carg[0]);
	if (ret != 1)
	    return 1;
	return 0;
    }
    return 1;
}

int
plot3_invalid(FILE *fp, int mode)
{

    /* Only two valid modes */
    if (mode != PL_OUTPUT_MODE_BINARY && mode != PL_OUTPUT_MODE_TEXT) {
	return 1;
    }

    /* A non-readable file isn't a valid file */
    if (!fp) {
	return 1;
    }

    int i = 0;
    int c;
    unsigned int tchar = 0;

    while (!feof(fp) && (c=getc(fp)) != EOF) {
	if (c < 'A' || c > 'z') {
	    return 1;
	}
	switch (c) {
	    case 'C':
		// TCHAR, 3, "color"
		if (mode == PL_OUTPUT_MODE_BINARY) {
		    for (i = 0; i < 3; i++) {
			if (getc(fp) == EOF)
			    return 1;
		    }
		}
		if (mode == PL_OUTPUT_MODE_TEXT) {
		    i = fscanf(fp, "%u", &tchar);
		    if (i != 1)
			return 1;
		}
		break;
	    case 'F':
		// TNONE, 0, "flush"
		break;
	    case 'L':
		// TSHORT, 6, "3line"
		if (read_short(fp, 6, mode))
		    return 1;
		break;
	    case 'M':
		// TSHORT, 3, "3move"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'N':
		// TSHORT, 3, "3cont"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'O':
		// TIEEE, 3, "d_3move"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'P':
		// TSHORT, 3, "3point"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'Q':
		// TIEEE, 3, "d_3cont"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'S':
		// TSHORT, 6, "3space"
		if (read_short(fp, 6, mode))
		    return 1;
			break;
	    case 'V':
		// TIEEE, 6, "d_3line"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 'W':
		// TIEEE, 6, "d_3space"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 'X':
		// TIEEE, 3, "d_3point"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'a':
		// TSHORT, 6, "arc"
		if (read_short(fp, 6, mode))
		    return 1;
			break;
	    case 'c':
		// TSHORT, 3, "circle"
			if (read_short(fp, 3, mode))
			    return 1;
			break;
	    case 'e':
		// TNONE, 0, "erase"
		break;
	    case 'f':
		// TSTRING, 1, "linmod"
		if (read_tstring(fp, mode))
		    return 1;
		break;
	    case 'i':
		// TIEEE, 3, "d_circle"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'l':
		// TSHORT, 4, "line"
		if (read_short(fp, 4, mode))
		    return 1;
			break;
	    case 'm':
		// TSHORT, 2, "move"
		if (read_short(fp, 2, mode))
		    return 1;
			break;
	    case 'n':
		// TSHORT, 2, "cont"
		if (read_short(fp, 2, mode))
		    return 1;
			break;
	    case 'o':
		// TIEEE, 2, "d_move"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    case 'p':
		// TSHORT, 2, "point"
		if (read_short(fp, 2, mode))
		    return 1;
		break;
	    case 'q':
		// TIEEE, 2, "d_cont"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    case 'r':
		// TIEEE, 6, "d_arc"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 's':
		// TSHORT, 4, "space"
		if (read_short(fp, 4, mode))
		    return 1;
		break;
	    case 't':
		// TSTRING, 1, "label"
		if (read_tstring(fp, mode))
		    return 1;
		break;
	    case 'v':
		// TIEEE, 4, "d_line"
		if (read_ieee(fp, 4, mode))
		    return 1;
		break;
	    case 'w':
		// TIEEE, 4, "d_space"
		if (read_ieee(fp, 4, mode))
		    return 1;
		break;
	    case 'x':
		// TIEEE, 2, "d_point"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    default:
		return 1;
		break;
	};
    }

    return 0;
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
