/*                        T A B I N T E R P . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2016 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tabinterp.c
 *
 * This program is the crucial middle step in the key-frame animation
 * software.
 *
 * First, one or more files, on different time scales, are read into
 * internal "channels", with FILE and RATE commands.
 *
 * Next, the TIMES command is given.
 *
 * Next, a section of those times is interpolated, and multi-channel
 * output is produced, on uniform time samples.
 *
 * This multi-channel output is fed to the next stage to generate
 * control scripts, etc.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/getopt.h"
#include "bu/log.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"


struct chan {
    /* INPUTS */
    int c_ilen;		/* length of input array */
    char *c_itag;	/* description of input source */
    fastf_t *c_itime;	/* input time array */
    fastf_t *c_ival;	/* input value array */
    /* OUTPUTS */
    fastf_t *c_oval;	/* output value array */
    /* FLAGS */
    int c_interp;	/* linear or spline? */
#define INTERP_STEP   1
#define INTERP_LINEAR 2
#define INTERP_SPLINE 3
#define INTERP_RATE   4
#define INTERP_ACCEL  5
#define INTERP_QUAT   6	/* first chan of 4 that define a quaternion */
#define INTERP_QUAT2  7	/* an additional quaterion chan (2, 3, 4) */
#define INTERP_NEXT   8	/* method to look forward/backward in time */
    int c_periodic;	/* cyclic end conditions? */
    int c_sourcechan;	/* index of source chan (QUAT, NEXT) */
    int c_offset;	/* source offset (NEXT) */
};

int verbose = 1;

int o_len;		/* length of all output arrays */
fastf_t *o_time;	/* pointer to output time array */
int fps;		/* frames/sec of output */

int nchans;		/* number of chan[] elements in use */
int max_chans;		/* current size of chan[] array */
struct chan *chan;	/* ptr to array of chan structs */


/*
 * Returns -
 * -1 on error
 *  0 if data loaded, and interpolator not yet set.
 *  1 if no data, or interpolator already set (message printed)
 */
HIDDEN int
chan_not_loaded_or_specified(int ch)
{
    if (ch < 0 || ch >= nchans)  return -1;
    if (chan[ch].c_ilen <= 0) {
	bu_log("error: attempt to set interpolation type on unallocated channel %d\n", ch);
	return 1;
    }
    if (chan[ch].c_interp > 0) {
	bu_log("error: attempt to modify channel %d which already has interpolation type set\n", ch);
	return 1;
    }
    return 0;
}


HIDDEN int
create_chan(const char *num, int len, const char *itag)
{
    int n;

    n = atoi(num);
    if (n < 0)  return -1;

    if (n >= max_chans) {
	int prev = max_chans;

	if (max_chans <= 0) {
	    max_chans = 32;
	    chan = (struct chan *)bu_calloc(1,
					    max_chans * sizeof(struct chan),
					    "chan[]");
	} else {
	    while (n >= max_chans)
		max_chans *= 2;
	    if (verbose) bu_log("reallocating from %d to %d chans\n",
				prev, max_chans);
	    chan = (struct chan *)bu_realloc((char *)chan,
					     max_chans * sizeof(struct chan),
					     "chan[]");
	    memset((char *)(&chan[prev]), 0, (max_chans-prev)*sizeof(struct chan));
	}
    }
    /* Allocate and clear channels */
    while (nchans <= n) {
	if (chan[nchans].c_ilen > 0) {
	    bu_log("create_chan: internal error\n");
	    return -1;
	} else {
	    memset((char *)&chan[nchans++], 0, sizeof(struct chan));
	}
    }

    if (verbose) bu_log("chan %d:  %s\n", n, itag);
    chan[n].c_ilen = len;
    chan[n].c_itag = bu_strdup(itag);
    chan[n].c_ival = (fastf_t *)bu_malloc(len * sizeof(fastf_t), "c_ival");
    return n;
}


HIDDEN int
cm_file(const int argc, const char **argv)
{
    FILE *fp;
    const char *file;
    char lbuf[512];	/* temporary label buffer */
    int *cnum;
    int i;
    int line;
    char **iwords;
    int nlines;		/* number of lines in input file */
    int nwords;		/* number of words on each input line */
    fastf_t *times;
    double d;
    int errors = 0;
    struct bu_vls buf = BU_VLS_INIT_ZERO;	/* unlimited size input line buffer */

    file = argv[1];
    if ((fp = fopen(file, "r")) == NULL) {
	perror(file);
	return 0;
    }

    /* First step, count number of lines in file */
    nlines = 0;
    {
	int c;

	while ((c = fgetc(fp)) != EOF) {
	    if (c == '\n')
		nlines++;
	}
    }
    rewind(fp);

    /* Intermediate dynamic memory */

    cnum = (int *)bu_malloc(argc * sizeof(int), "cnum[]");
    nwords = argc - 1;
    iwords = (char **)bu_malloc((nwords+1) * sizeof(char *), "iwords[]");

    /* Retained dynamic memory */
    times = (fastf_t *)bu_malloc(nlines * sizeof(fastf_t), "times");

    /* Now, create & allocate memory for each chan */
    for (i = 1; i < nwords; i++) {
	/* See if this column is not wanted */
	if (argv[i+1][0] == '-') {
	    cnum[i] = -1;
	    continue;
	}

	snprintf(lbuf, 512, "File '%s', Column %d", file, i);
	if ((cnum[i] = create_chan(argv[i+1], nlines, lbuf)) < 0) {
	    errors = 1;
	    goto out;
	}
	/* Share array of times */
	chan[cnum[i]].c_itime = times;
    }

    for (line = 0; line < nlines; line++) {
	char *bp;

	bu_vls_trunc(&buf, 0);
	if (bu_vls_gets(&buf, fp) == -1)  break;
	bp = bu_vls_addr(&buf);

	if (bp[0] == '#') {
	    line--;
	    nlines--;
	    for (i = 1; i < nwords; i++) {
		if (cnum[i] < 0)  continue;
		chan[cnum[i]].c_ilen--;
	    }
	    continue;
	}

	i = bu_argv_from_string(iwords, nwords, bp);

	if (i != nwords) {
	    bu_log("File '%s', Line %d:  expected %d columns, got %d\n",
		   file, line, nwords, i);
	    while (i < nwords) {
		iwords[i++] = "0.123456789";
		errors++;
	    }
	}

	/* Obtain the time from the first column */
	sscanf(iwords[0], "%lf", &d);
	times[line] = d;
	if (line > 0 && times[line-1] > times[line]) {
	    bu_log("File '%s', Line %d:  time sequence error %g > %g\n",
		   file, line, times[line-1], times[line]);
	    errors++;
	}

	/* Obtain the desired values from the remaining columns, and
	 * assign them to the channels indicated in cnum[]
	 */
	for (i = 1; i < nwords; i++) {
	    if (cnum[i] < 0)  continue;
	    if (sscanf(iwords[i], "%lf", &d) != 1) {
		bu_log("File '%s', Line %d:  scanf failure on '%s'\n",
		       file, line, iwords[i]);
		d = 0.0;
		errors++;
	    }
	    chan[cnum[i]].c_ival[line] = d;
	}
    }

    /* Free intermediate dynamic memory */
 out:
    fclose(fp);
    bu_free((char *)cnum, "cnum[]");
    bu_free((char *)iwords, "iwords[]");
    bu_vls_free(&buf);

    if (errors)
	return -1;	/* abort */
    return 0;
}


/*
 * Print input channel values.
 */
HIDDEN void
pr_ichan(int ch)
{
    struct chan *cp;
    int i;

    if (ch < 0 || ch >= nchans) {
	bu_log("pr_ichan(%d) out of range\n", ch);
	return;
    }
    cp = &chan[ch];
    if (cp->c_itag == (char *)0)  cp->c_itag = "_no_file_";
    bu_log("--- Channel %d, ilen=%d (%s):\n",
	   ch, cp->c_ilen, cp->c_itag);
    for (i = 0; i < cp->c_ilen; i++) {
	bu_log(" %g\t%g\n", cp->c_itime[i], cp->c_ival[i]);
    }
}


/*
 * Dump the indicated input channels, or all, if none specified.
 */
HIDDEN int
cm_idump(const int argc, const char **argv)
{
    int ch;
    int i;

    if (argc <= 1) {
	for (ch = 0; ch < nchans; ch++) {
	    pr_ichan(ch);
	}
    } else {
	for (i = 1; i < argc; i++) {
	    pr_ichan(atoi(argv[i]));
	}
    }
    return 0;
}


HIDDEN void
output()
{
    int ch;
    struct chan *cp;
    int t;

    if (!o_time) {
	bu_log("times command not given, aborting\n");
	return;
    }

    for (t = 0; t < o_len; t++) {
	printf("%g", o_time[t]);

	for (ch = 0; ch < nchans; ch++) {
	    cp = &chan[ch];
	    if (cp->c_ilen <= 0) {
		printf("\t.");
		continue;
	    }
	    printf("\t%g", cp->c_oval[t]);
	}
	printf("\n");
    }
}


HIDDEN int
cm_times(const int argc, const char **argv)
{
    double a, b;
    int i;

    if (argc < 3)
	bu_exit(1, "Internal error.  Unexpected argument count encountered.");

    a = atof(argv[1]);
    b = atof(argv[2]);
    fps = atoi(argv[3]);

    if (a >= b) {
	bu_log("times:  %g >= %g\n", a, b);
	return 0;
    }
    if (o_len > 0) {
	bu_log("times:  already specified\n");
	return 0;	/* ignore */
    }
    o_len = ((b-a) * fps) + 0.999;
    o_len++;	/* one final step to reach endpoint */
    o_time = (fastf_t *)bu_malloc(o_len * sizeof(fastf_t), "o_time[]");

    /*
     * Don't use an incremental algorithm, to avoid accruing error
     */
    for (i = 0; i < o_len; i++)
	o_time[i] = a + ((double)i)/fps;


    return 0;
}


HIDDEN int
cm_interp(const int argc, const char **argv)
{
    int interp = 0;
    int periodic = 0;
    int i;
    int ch;
    struct chan *chp;

    if (BU_STR_EQUAL(argv[1], "step")) {
	interp = INTERP_STEP;
	periodic = 0;
    } else if (BU_STR_EQUAL(argv[1], "cstep")) {
	interp = INTERP_STEP;
	periodic = 1;
    } else if (BU_STR_EQUAL(argv[1], "linear")) {
	interp = INTERP_LINEAR;
	periodic = 0;
    } else if (BU_STR_EQUAL(argv[1], "clinear")) {
	interp = INTERP_LINEAR;
	periodic = 1;
    } else if (BU_STR_EQUAL(argv[1], "spline")) {
	interp = INTERP_SPLINE;
	periodic = 0;
    } else if (BU_STR_EQUAL(argv[1], "cspline")) {
	interp = INTERP_SPLINE;
	periodic = 1;
    } else if (BU_STR_EQUAL(argv[1], "quat")) {
	interp = INTERP_QUAT;
	periodic = 0;
    } else {
	bu_log("interpolation type '%s' unknown\n", argv[1]);
	interp = INTERP_LINEAR;
    }

    for (i = 2; i < argc; i++) {
	ch = atoi(argv[i]);
	chp = &chan[ch];
	if (chan_not_loaded_or_specified(ch))  continue;
	chp->c_interp = interp;
	chp->c_periodic = periodic;
	if (interp == INTERP_QUAT) {
	    int j;
	    for (j = 1; j < 4; j++) {
		chp = &chan[ch+j];
		if (chan_not_loaded_or_specified(ch+j))  continue;
		chp->c_interp = INTERP_QUAT2;
		chp->c_periodic = periodic;
		chp->c_sourcechan = ch;
	    }
	}
    }
    return 0;
}


HIDDEN void
next_interpolate(struct chan *chp)
{
    int t;		/* output time index */
    int i;		/* input time index */
    struct chan *ip;

    ip = &chan[chp->c_sourcechan];

    for (t = 0; t < o_len; t++) {
	i = t + chp->c_offset;
	if (i <= 0) {
	    chp->c_oval[t] = ip->c_oval[0];
	    continue;
	}
	if (i >= o_len) {
	    chp->c_oval[t] = ip->c_oval[o_len-1];
	    continue;
	}
	chp->c_oval[t] = ip->c_oval[i];
    }
}


/*
 * Simply select the value at the beginning of the interval.  This
 * allows parameters to take instantaneous jumps in value at specified
 * times.
 *
 * This routine takes advantage of (and depends on) the fact that the
 * input and output is sorted in increasing time values.
 */
HIDDEN void
step_interpolate(struct chan *chp, fastf_t *times)
{
    int t;		/* output time index */
    int i;		/* input time index */

    i = 0;
    for (t = 0; t < o_len; t++) {
	/* Check for below initial time */
	if (times[t] < chp->c_itime[0]) {
	    chp->c_oval[t] = chp->c_ival[0];
	    continue;
	}

	/* Find time range in input data. */
	while (i < chp->c_ilen-1) {
	    if (times[t] >= chp->c_itime[i] &&
		times[t] <  chp->c_itime[i+1])
		break;
	    i++;
	}

	/* Check for above final time */
	if (i >= chp->c_ilen-1) {
	    chp->c_oval[t] = chp->c_ival[chp->c_ilen-1];
	    continue;
	}

	/* Select value at beginning of interval */
	chp->c_oval[t] = chp->c_ival[i];
    }
}

/*
 * This routine takes advantage of (and depends on) the fact that the
 * input and output arrays are sorted in increasing time values.
 */
HIDDEN void
linear_interpolate(struct chan *chp, fastf_t *times)
{
    int t;		/* output time index */
    int i;		/* input time index */

    if (chp->c_ilen < 2) {
	bu_log("linear_interpolate:  need at least 2 points\n");
	return;
    }

    i = 0;
    for (t = 0; t < o_len; t++) {
	/* Check for below initial time */
	if (times[t] < chp->c_itime[0]) {
	    chp->c_oval[t] = chp->c_ival[0];
	    continue;
	}

	/* Find time range in input data. */
	while (i < chp->c_ilen-1) {
	    if (times[t] >= chp->c_itime[i] &&
		times[t] <  chp->c_itime[i+1])
		break;
	    i++;
	}

	/* Check for above final time */
	if (i >= chp->c_ilen-1) {
	    chp->c_oval[t] = chp->c_ival[chp->c_ilen-1];
	    continue;
	}

	/* Perform actual interpolation */
	chp->c_oval[t] = chp->c_ival[i] +
	    (times[t] - chp->c_itime[i]) *
	    (chp->c_ival[i+1] - chp->c_ival[i]) /
	    (chp->c_itime[i+1] - chp->c_itime[i]);
    }
}

/*
 * The one (and only) input value is interpreted as rate, in
 * unspecified units per second.  This is really just a hack to allow
 * multiplying the time by a constant.
 */
HIDDEN void
rate_interpolate(struct chan *chp, fastf_t *UNUSED(times))
{
    int t;		/* output time index */
    double ival;
    double rate;

    if (chp->c_ilen != 2) {
	bu_log("rate_interpolate:  only 2 points (ival & rate) may be specified\n");
	return;
    }
    ival = chp->c_ival[0];
    rate = chp->c_ival[1];

    for (t = 0; t < o_len; t++) {
	chp->c_oval[t] = ival + rate * t;
    }
}

HIDDEN void
accel_interpolate(struct chan *chp, fastf_t *UNUSED(times))
{
    int t;		/* output time index */
    double ival;
    double mul;
    double scale;

    if (chp->c_ilen != 2) {
	bu_log("accel_interpolate:  only 2 points (ival & mul) may be specified\n");
	return;
    }
    ival = chp->c_ival[0];
    mul = chp->c_ival[1];
    /* scale ^ fps = mul */
    scale = exp(log(mul) / fps);

    chp->c_oval[0] = ival;
    for (t = 1; t < o_len; t++) {
	chp->c_oval[t] = chp->c_oval[t-1] * scale;
    }
}


/*
 * Do linear interpolation for first and last span.  Use Bezier
 * interpolation for all the rest.
 *
 * This routine depends on the four input channels having identical
 * time stamps, because only the "x" input times are used.
 */
HIDDEN void
quat_interpolate(struct chan *x, struct chan *y, struct chan *z, struct chan *w, fastf_t *times)
{
    int t;		/* output time index */
    int i;		/* input time index */

#define QIGET(_q, _it)  QSET((_q), x->c_ival[(_it)], y->c_ival[(_it)], z->c_ival[(_it)], w->c_ival[(_it)]);
#define QPUT(_q) { x->c_oval[t] = (_q)[X]; y->c_oval[t] = (_q)[Y]; \
			z->c_oval[t] = (_q)[Z]; w->c_oval[t] = (_q)[W]; }

    i = 0;
    for (t = 0; t < o_len; t++) {
	fastf_t now = times[t];

	/* Check for below initial time */
	if (now <= x->c_itime[0]) {
	    quat_t q1;

	    QIGET(q1, 0);
	    QUNITIZE(q1);
	    QPUT(q1);
	    continue;
	}

	/* Find time range in input data. */
	while (i < x->c_ilen-1) {
	    if (now >= x->c_itime[i] &&
		now <  x->c_itime[i+1])
		break;
	    i++;
	}

	/* Check for above final time */
	if (i >= x->c_ilen-1) {
	    quat_t q1;

	    i = x->c_ilen-1;
	    QIGET(q1, i);
	    QUNITIZE(q1);
	    QPUT(q1);
	    continue;
	}

	/* Check for being in first or last time span */
	if (i == 0 || i >= x->c_ilen-2) {
	    fastf_t f;
	    quat_t qout, q1, q2, q3, qtemp1, qtemp2, qtemp3;

	    f = (now - x->c_itime[i]) /
		(x->c_itime[i+1] - x->c_itime[i]);

	    if (i == 0) {
		QIGET(q1, i);
		QIGET(q2, i+1);
		QIGET(q3, i+2);
		QUNITIZE(q1);
		QUNITIZE(q2);
		QUNITIZE(q3);
		quat_make_nearest(q2, q1);
		quat_make_nearest(q3, q2);
	    } else {
		QIGET(q1, i+1);
		QIGET(q2, i);
		QIGET(q3, i-1);
		f = 1.0 - f;
		QUNITIZE(q1);
		QUNITIZE(q2);
		QUNITIZE(q3);
		quat_make_nearest(q2, q3);
		quat_make_nearest(q1, q2);
	    }

	    /* find middle control point */
	    quat_slerp(qtemp1, q3, q2, 2.0);
	    quat_slerp(qtemp2, qtemp1, q1, 0.5);
	    quat_slerp(qtemp3, q2, qtemp2, 0.33333);

	    /* do 3-point bezier interpolation */
	    quat_slerp(qtemp1, q1, qtemp3, f);
	    quat_slerp(qtemp2, qtemp3, q2, f);
	    quat_slerp(qout, qtemp1, qtemp2, f);

	    QPUT(qout);
	    continue;
	}

	/* In an intermediate time span */
	{
	    fastf_t f;
	    quat_t qout, q1, q2, q3, q4, qa, qb, qtemp1, qtemp2;

	    f = (now - x->c_itime[i]) /
		(x->c_itime[i+1] - x->c_itime[i]);

	    QIGET(q1, i-1);
	    QIGET(q2, i);
	    QIGET(q3, i+1);
	    QIGET(q4, i+2);

	    QUNITIZE(q1);
	    QUNITIZE(q2);
	    QUNITIZE(q3);
	    QUNITIZE(q4);
	    quat_make_nearest(q2, q1);
	    quat_make_nearest(q3, q2);
	    quat_make_nearest(q4, q3);

	    /* find two middle control points */
	    quat_slerp(qtemp1, q1, q2, 2.0);
	    quat_slerp(qtemp2, qtemp1, q3, 0.5);
	    quat_slerp(qa, q2, qtemp2, 0.333333);

	    quat_slerp(qtemp1, q4, q3, 2.0);
	    quat_slerp(qtemp2, qtemp1, q2, 0.5);
	    quat_slerp(qb, q3, qtemp2, 0.333333);

	    quat_sberp(qout, q2, qa, qb, q3, f);
	    QPUT(qout);
	}

    }
}


/*
 * Fit an interpolating spline to the data points given.  Time in the
 * independent (x) variable, and the single channel of data values is
 * the dependent (y) variable.
 *
 * Returns -
 * 0	bad
 * 1	OK
 */
HIDDEN int
spline(struct chan *chp, fastf_t *times)
{
    double d, s;
    double u = 0;
    double v = 0;
    double hi;			/* horiz interval i-1 to i */
    double hi1;			/* horiz interval i to i+1 */
    double D2yi;			/* D2 of y[i] */
    double D2yi1;			/* D2 of y[i+1] */
    double D2yn1;			/* D2 of y[n-1] (last point) */
    double a;
    int end;
    double corr;
    double konst = 0.0;		/* deriv. at endpts, non-periodic */
    double *diag = (double *)0;
    double *rrr = (double *)0;
    int i;
    int t;

    if (chp->c_ilen<3) {
	bu_log("spline(%s): need at least 3 points\n", chp->c_itag);
	goto bad;
    }

    /* First, as a quick hack, do linear interpolation to fill in
     * values off the endpoints, in non-periodic case
     */
    if (chp->c_periodic == 0)
	linear_interpolate(chp, times);

    if (chp->c_periodic
	&& !ZERO(chp->c_ival[0] - chp->c_ival[chp->c_ilen-1]))
    {
	bu_log("spline(%s): endpoints don't match, replacing final data value\n", chp->c_itag);
	chp->c_ival[chp->c_ilen-1] = chp->c_ival[0];
    }

    i = (chp->c_ilen+1)*sizeof(double);
    diag = (double *)bu_malloc((unsigned)i, "diag");
    rrr = (double *)bu_malloc((unsigned)i, "rrr");
    if (!rrr || !diag) {
	bu_log("spline: malloc failure\n");
	goto bad;
    }

    if (chp->c_periodic) konst = 0;
    d = 1;
    rrr[0] = 0;
    s = chp->c_periodic ? -1 : 0;
    /* triangularize */
    for (i = 0; ++i < chp->c_ilen - !chp->c_periodic;) {
	double rhs;

	hi = chp->c_itime[i]-chp->c_itime[i-1];
	hi1 = (i == chp->c_ilen-1) ?
	    chp->c_itime[1] - chp->c_itime[0] :
	    chp->c_itime[i+1] - chp->c_itime[i];
	if (hi1 * hi <= 0) {
	    bu_log(
		"spline: Horiz. interval changed sign at i=%d, time=%g\n",
		i, chp->c_itime[i]);
	    goto bad;
	}
	if (i <= 1) {
	    u = v = 0.0;		/* First time through */
	} else {
	    u = u - s * s / d;
	    v = v - s * rrr[i-1] / d;
	}

	rhs = (i == chp->c_ilen-1) ?
	    (chp->c_ival[1] - chp->c_ival[0]) /
	    (chp->c_itime[1] - chp->c_itime[0]) :
	    (chp->c_ival[i+1] - chp->c_ival[i]) /
	    (chp->c_itime[i+1] - chp->c_itime[i]);
	rhs = 6 * (rhs  -
		   ((chp->c_ival[i] - chp->c_ival[i-1]) /
		    (chp->c_itime[i] - chp->c_itime[i-1])));

	rrr[i] = rhs - hi * rrr[i-1] / d;

	s = -hi*s/d;
	a = 2*(hi+hi1);
	if (i == 1) a += konst*hi;
	if (i == chp->c_ilen-2) a += konst*hi1;
	diag[i] = d = i == 1 ? a :
	    a - hi*hi/d;
    }
    D2yi = D2yn1 = 0;
    /* back substitute */
    for (i = chp->c_ilen - !chp->c_periodic; --i >= 0;) {
	end = i == chp->c_ilen-1;
	/* hi1 is range of time covered in this interval */
	hi1 = end ? chp->c_itime[1] - chp->c_itime[0]:
	    chp->c_itime[i+1] - chp->c_itime[i];
	D2yi1 = D2yi;
	if (i > 0) {
	    hi = chp->c_itime[i]-chp->c_itime[i-1];
	    corr = end ? 2 * s + u : 0.0;
	    D2yi = (end*v+rrr[i]-hi1*D2yi1-s*D2yn1)/
		(diag[i]+corr);
	    if (end) D2yn1 = D2yi;
	    if (i >= 1) {
		a = 2*(hi+hi1);
		if (i == 1) a += konst*hi;
		if (i == chp->c_ilen-2) a += konst*hi1;
		d = diag[i-1];
		s = -s*d/hi;
	    }
	}
	else D2yi = D2yn1;
	if (!chp->c_periodic) {
	    if (i == 0) D2yi = konst*D2yi1;
	    if (i == chp->c_ilen-2) D2yi1 = konst*D2yi;
	}
	if (end) continue;

	/* Sweep downward in times[], looking for times in this span */
	for (t = o_len - 1; t >= 0; t--) {
	    double x0;	/* fraction from [i+0] */
	    double x1;	/* fraction from [i+1] */
	    double yy;
	    double cur_t;

	    cur_t = times[t];
	    if (cur_t > chp->c_itime[i+1])
		continue;
	    if (cur_t < chp->c_itime[i])
		continue;
	    x1 = (cur_t - chp->c_itime[i]) /
		(chp->c_itime[i+1] - chp->c_itime[i]);
	    x0 = 1 - x1;
	    /* Linear interpolation, with correction */
	    yy = D2yi * (x0 - x0*x0*x0) + D2yi1 * (x1 - x1*x1*x1);
	    yy = chp->c_ival[i] * x0 + chp->c_ival[i+1] * x1 -
		hi1 * hi1 * yy / 6;
	    chp->c_oval[t] = yy;
	}
    }
    bu_free((char *)diag, "diag");
    bu_free((char *)rrr, "rrr");
    return 1;
 bad:
    if (diag) bu_free((char *)diag, "diag");
    if (rrr) bu_free((char *)rrr, "rrr");
    return 0;
}


/*
 * Perform the requested interpolation on each channel
 */
HIDDEN void
go()
{
    int ch;
    struct chan *chp;
    fastf_t *times;
    int t;

    if (!o_time) {
	bu_log("times command not given\n");
	return;
    }

    times = (fastf_t *)bu_malloc(o_len*sizeof(fastf_t), "periodic times");

    /* First, get memory for all output channels */
    for (ch = 0; ch < nchans; ch++) {
	chp = &chan[ch];
	if (chp->c_ilen <= 0)
	    continue;

	/* Allocate memory for all the output values */
	chan[ch].c_oval = (fastf_t *)bu_malloc(
	    o_len * sizeof(fastf_t), "c_oval[]");
    }

    /* Interpolate values for all "interp" channels */
    for (ch = 0; ch < nchans; ch++) {
	chp = &chan[ch];
	if (chp->c_ilen <= 0)
	    continue;
	if (chp->c_interp == INTERP_NEXT)  continue;

	/* As a service to interpolators, if this is a periodic
	 * interpolation, build the mapped time array.
	 */
	if (chp->c_periodic) {
	    for (t = 0; t < o_len; t++) {
		double cur_t;

		cur_t = o_time[t];

		while (cur_t > chp->c_itime[chp->c_ilen-1]) {
		    cur_t -= (chp->c_itime[chp->c_ilen-1] -
			      chp->c_itime[0]);
		}
		while (cur_t < chp->c_itime[0]) {
		    cur_t += (chp->c_itime[chp->c_ilen-1] -
			      chp->c_itime[0]);
		}
		times[t] = cur_t;
	    }
	} else {
	    for (t = 0; t < o_len; t++) {
		times[t] = o_time[t];
	    }
	}
    again:
	switch (chp->c_interp) {
	    default:
		bu_log("channel %d: unknown interpolation type %d\n", ch, chp->c_interp);
		break;
	    case INTERP_LINEAR:
		linear_interpolate(chp, times);
		break;
	    case INTERP_STEP:
		step_interpolate(chp, times);
		break;
	    case INTERP_SPLINE:
		if (spline(chp, times) <= 0) {
		    bu_log("spline failure, switching to linear\n");
		    chp->c_interp = INTERP_LINEAR;
		    goto again;
		}
		break;
	    case INTERP_RATE:
		rate_interpolate(chp, times);
		break;
	    case INTERP_ACCEL:
		accel_interpolate(chp, times);
		break;
	    case INTERP_QUAT:
		quat_interpolate(chp, &chan[ch+1], &chan[ch+2], &chan[ch+3], times);
		break;
	    case INTERP_QUAT2:
		/* Don't touch these here, handled above */
		continue;
	}
    }

    /* Copy out values for all "next" channels */
    for (ch = 0; ch < nchans; ch++) {
	chp = &chan[ch];
	if (chp->c_ilen <= 0)
	    continue;
	if (chp->c_interp != INTERP_NEXT)  continue;
	next_interpolate(chp);
    }

    bu_free((char *)times, "loc times");
}


/*
 * Just to communicate with the "interpolator", use two input values.
 * First is initial value, second is change PER SECOND. Input time
 * values are meaningless.
 */
HIDDEN int
cm_rate(const int argc, const char **argv)
{
    struct chan *chp;
    int ch;
    int nvals = 2;
    const char rate_chan[] = "rate chan";
    ch = create_chan(argv[1], nvals, (argc > 4 ? argv[4] : rate_chan));
    chp = &chan[ch];
    chp->c_interp = INTERP_RATE;
    chp->c_periodic = 0;
    chp->c_itime = (fastf_t *)bu_malloc(nvals * sizeof(fastf_t), "rate times");
    chp->c_itime[0] = chp->c_itime[1] = 0;
    chp->c_ival[0] = atof(argv[2]);
    chp->c_ival[1] = atof(argv[3]);
    return 0;
}


/*
 * Just to communicate with the "interpolator", use two input values.
 * First is initial value, second is change PER SECOND. Input time
 * values are meaningless.
 */
HIDDEN int
cm_accel(const int argc, const char **argv)
{
    struct chan *chp;
    int ch;
    int nvals = 2;
    const char accel_chan[] = "accel chan";
    ch = create_chan(argv[1], nvals, (argc > 4 ? argv[4] : accel_chan));
    chp = &chan[ch];
    chp->c_interp = INTERP_ACCEL;
    chp->c_periodic = 0;
    chp->c_itime = (fastf_t *)bu_malloc(nvals * sizeof(fastf_t), "accel times");
    chp->c_itime[0] = chp->c_itime[1] = 0;
    chp->c_ival[0] = atof(argv[2]);
    chp->c_ival[1] = atof(argv[3]);
    return 0;
}


HIDDEN int
cm_next(const int argc, const char **argv)
{
    int ochan, ichan;
    int offset = 1;
    char buf[128];

    ochan = atoi(argv[1]);
    ichan = atoi(argv[2]);
    if (argc > 3)  offset = atoi(argv[3]);
    /* If input channel not loaded, or not interpolated, error */
    if (chan[ichan].c_ilen <= 0 || chan[ichan].c_interp <= 0) {
	bu_log("ERROR next: ichan %d not loaded yet\n", ichan);
	return 0;
    }
    /* If output channel is loaded, error */
    if (chan[ochan].c_ilen > 0) {
	bu_log("ERROR next: ochan %d previous loaded\n", ochan);
	return 0;
    }
    sprintf(buf, "next: value of chan %d [%d]", ichan, offset);
    if (create_chan(argv[1], chan[ichan].c_ilen, buf) < 0) {
	bu_log("ERROR next: unable to create output channel\n");
	return 0;
    }
    /* c_ilen, c_itag, c_ival are now initialized */
    chan[ochan].c_interp = INTERP_NEXT;
    chan[ochan].c_sourcechan = ichan;
    chan[ochan].c_offset = offset;
    chan[ochan].c_periodic = 0;
    chan[ochan].c_itime = chan[ichan].c_itime;	/* share time array */
    /* c_ival[] will not be loaded with anything */
    return 0;
}


#define OPT_STR "q"
HIDDEN int
get_args(int argc, char **argv)
{
    int c;
    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'q':
		verbose = 0;
		break;
	    default:
		fprintf(stderr, "Unknown option: -%c\n", c);
		return 0;
	}
    }
    return 1;
}

HIDDEN int cm_help(const int argc, const char **argv);

struct command_tab cmdtab[] = {
    {"file", "filename chan_num(s)", "load channels from file",
     cm_file,	3, 999},
    {"times", "start stop fps", "specify time range and fps rate",
     cm_times,	4, 4},
    {"interp", "{step|linear|spline|cspline|quat} chan_num(s)", "set interpolation type",
     cm_interp,	3, 999},
    {"next", "dest_chan src_chan [+/- #nsamp]", "lookahead in time",
     cm_next,	3, 4},
    {"idump", "[chan_num(s)]", "dump input channel values",
     cm_idump,	1, 999},
    {"rate", "chan_num init_value incr_per_sec [comment]", "create rate based channel",
     cm_rate,	4, 5},
    {"accel", "chan_num init_value mult_per_sec [comment]", "create acceleration based channel",
     cm_accel,	4, 5},
    {"help", "", "print help message",
     cm_help,	1, 999},
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0}	/* END */
};


/*
 * XXX this really should go in librt/cmd.c as rt_help_cmd().
 */
HIDDEN int
cm_help(const int argc, const char **argv)
{
    struct command_tab *ctp;

    if (argc <= 1) {
	bu_log("The following commands are available:\n\n");
	for (ctp = cmdtab; ctp->ct_cmd != (char *)0; ctp++) {
	    bu_log("%s %s\n\t%s\n",
		   ctp->ct_cmd, ctp->ct_parms,
		   ctp->ct_comment);
	}
	return 0;
    }
    bu_log("%s: Detailed help is not yet available.\n", argv[0]);
    return -1;
}


int
main(int argc, char *argv[])
{
    char *buf;
    int ret;

    get_args(argc, argv);
    /*
     * All the work happens in the functions called by rt_do_cmd().
     * NOTE that the value of MAXWORDS in rt_do_cmd() limits the
     * maximum number of columns that a 'file' command can list.
     */
    while ((buf = rt_read_cmd(stdin)) != (char *)0) {
	if (verbose) bu_log("cmd: %s\n", buf);
	ret = rt_do_cmd(0, buf, cmdtab);
	bu_free(buf, "cmd buf");
	if (ret < 0) {
	    if (verbose) bu_log("aborting\n");
	    bu_exit(1, NULL);
	}
    }

    if (verbose) bu_log("performing interpolations\n");
    go();

    if (verbose) bu_log("writing output\n");
    output();

    return 0;
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
