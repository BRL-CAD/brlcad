/*                          H I S T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2009 United States Government as represented by
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
/** @addtogroup bu_hist */
/** @{ */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"


void
bu_hist_free(struct bu_hist *histp)
{
    if ((histp==(struct bu_hist *)NULL) ||
	(histp && (histp->magic == 0 || histp->magic == (unsigned int)-1)))
    {
	return;
    }

    BU_CK_HIST(histp);

    if (histp->hg_bins)
	bu_free((char *)histp->hg_bins, "old bu_hist bins");
    histp->hg_bins = (long *)0;
    histp->hg_nbins = 0;
    histp->magic = (unsigned int)-1;	/* sanity */
}


void
bu_hist_init(struct bu_hist *histp, fastf_t min, fastf_t max, unsigned int nbins)
{

    if (max <= min)  max = min+1;
    if (nbins < 1) {
	nbins = 1;	/* nbins=1 makes for a nice 2-bin binary histogram */
    } else if (nbins > 10000) {
	nbins = 10000;	/* This is a lot of lines to print out */
    }

    histp->hg_min = floor(min);
    histp->hg_max = ceil(max);
    histp->hg_nbins = nbins;

    histp->hg_clumpsize = ((max-min)/nbins);
    if (histp->hg_clumpsize <= 0)  histp->hg_clumpsize = 1;

    histp->hg_nsamples = 0L;
    histp->hg_bins = (long *)bu_calloc(nbins+1, sizeof(long), "bu_hist bins");
    histp->magic = BU_HIST_MAGIC;
}


void
bu_hist_range(register struct bu_hist *hp, fastf_t low, fastf_t high)
{
    long a;
    long b;
    register int i;

    BU_CK_HIST(hp);
    if (low <= hp->hg_min)
	a = 0;
    else
	a = (low - hp->hg_min) / hp->hg_clumpsize;
    if (high >= hp->hg_max)
	b = hp->hg_nbins-1;
    else
	b = (high - hp->hg_min) / hp->hg_clumpsize;
    if (b >= hp->hg_nbins)  b = hp->hg_nbins-1;

    for (i=a; i <= b; i++) {
	hp->hg_bins[i]++;
    }
    hp->hg_nsamples++;
}


/**
 * _ B U _ H I S T _ P R _ S U P P R E S S
 *
 * Allows caller control over zero-suppression feature.
 */
HIDDEN void
_bu_hist_pr_suppress(register const struct bu_hist *histp, const char *title, int zero_suppress)
{
    register int i;
    long maxcount;
    static const char marks[] = "################################################################";
#define NMARKS 50
    char buf[256];
    int percent;
    unsigned int mark_count;
    double val;
    int nbins;

    BU_CK_HIST(histp);

    /* Find entry with highest count */
    maxcount = 0L;
    for (i=0; i<=histp->hg_nbins; i++) {
	if (histp->hg_bins[i] > maxcount)
	    maxcount = histp->hg_bins[i];
    }
    if (maxcount <= 0)  maxcount = 1;

    nbins = histp->hg_nbins;
    if (zero_suppress) {
	/* Supress trailing bins with zero counts.  nbins s/b >= 1 */
	for (; nbins >= 1; nbins--)
	    if (histp->hg_bins[nbins] > 0)  break;
    }

    /* 12345678 12345678 123 .... */
    bu_log("\nHistogram of %s\nmin=%g, max=%g, nbins=%ld, clumpsize=%g\n%ld samples collected, highest count was %ld\n\n Value      Count Rel%%|  Bar Graph\n",
	   title,
	   histp->hg_min, histp->hg_max,
	   histp->hg_nbins, histp->hg_clumpsize,
	   histp->hg_nsamples, maxcount);

    /* Print each bin. */
    i = 0;
    if (zero_suppress) {
	/* Leading bins with zero counts are supressed. */
	for (; i <= nbins; i++) {
	    if (histp->hg_bins[i] > 0)  break;
	}
    }
    for (; i <= nbins; i++) {
	percent = (int)(((double)histp->hg_bins[i])*100.0/maxcount);
	mark_count = percent*NMARKS/100;
	if (mark_count <= 0 && histp->hg_bins[i] > 0)
	    mark_count = 1;
	if (mark_count > NMARKS) {
	    bu_log("mark_count=%d, NMARKS=%d, hg_bins[%d]=%ld, maxcount=%ld\n",
		   mark_count, NMARKS, i, histp->hg_bins[i], maxcount);
	    bu_bomb("bu_hist_pr() bogus mark_count\n");
	}
	if (mark_count <= 0) {
	    buf[0] = '\0';
	} else {
	    memcpy(buf, marks, mark_count);
	    buf[mark_count] = '\0';
	}
	val = histp->hg_min + i*histp->hg_clumpsize;
	bu_log("%8g %8ld %3d |%s\n",
	       val,
	       histp->hg_bins[i], percent, buf);
    }
}


void
bu_hist_pr(register const struct bu_hist *histp, const char *title)
{
    _bu_hist_pr_suppress(histp, title, 1);
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
