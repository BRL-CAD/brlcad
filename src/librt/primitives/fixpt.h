/*                         F I X P T . H
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
/** @addtogroup librt */
/** @{ */
/** @file fixpt.h
 *
 * Data structures and macros for dealing with integer-based fixed
 * point math.
 *
 * These macros use integer instructions for a special "fixed point"
 * format.  The fractional part is stored in 28-bit integer form, to
 * prevent roundoff errors.
 *
 */
/** @} */

#ifndef FIXPT_H
#define FIXPT_H seen

#define FIXPT_SCALE ((1<<28)-1)
struct fixpt {
    int i;
    int f;		/* Ranges 0 to FIXPT_SCALE-1 */
};


#define FIXPT_FLOAT(fp, fl) { \
	register double d = fl; \
	fp.f = (d - (fp.i = (int)d)) * FIXPT_SCALE; \
	FIXPT_NORMALIZE(fp); }

#define FLOAT_FIXPT(fp)  (fp.i + ((double)fp.f)/FIXPT_SCALE)

#define FIXPT_NORMALIZE(fp) { \
	if (fp.f < 0) { \
		do {  \
			fp.i--; \
			fp.f += FIXPT_SCALE; \
		} while (fp.f < 0); \
	} else if (fp.f >= FIXPT_SCALE) { \
		do { \
			fp.i++; \
			fp.f -= FIXPT_SCALE; \
		} while (fp.f >= FIXPT_SCALE); \
	} }

#define FIXPT_ROUND(fp) { \
	if (fp.f > FIXPT_SCALE/2) { \
		if (fp.i >= 0) fp.i++; \
		else fp.i--; \
	}  fp.f = 0; }

#define FIXPT_ADD2(o, a, b) {\
	o.i = a.i + b.i; \
	o.f = a.f + b.f; \
	FIXPT_NORMALIZE(o); }

#define PR_FIX(str, fp) bu_log("%s = %2d+x%8.8x\n", str, fp.i, fp.f)

#define PR_FIX2(str, fp) bu_log("%s = (%2d+x%8.8x, %2d+x%8.8x)\n", \
				str, fp[0].i, fp[0].f, fp[1].i, fp[1].f)

#endif /* FIXPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
