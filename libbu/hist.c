/*
 *			H I S T . C
 *
 *  General purpose histogram handling routines
 *
 *  The macro RT_HISTOGRAM_TALLY is used to record items that
 *  live in a single "bin", while the subroutine rt_hist_range()
 *  is used to record items that may extend across multiple "bin"s.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCShist[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"

/*
 *			B U _ H I S T _ F R E E
 */
void
bu_hist_free( histp )
struct bu_hist	*histp;
{
	if( histp && histp->magic == 0 )  return;
	BU_CK_HIST(histp);
	if( histp->hg_bins )
		bu_free( (char *)histp->hg_bins, "old bu_hist bins");
	histp->hg_bins = (long *)0;
	histp->hg_nbins = 0;
	histp->magic = -1;	/* sanity */
}

/*
 *			B U _ H I S T _ I N I T
 *
 *  Initialize a bu_hist structure.
 *  It is expected that the structure is junk upon entry.
 */
void
bu_hist_init( histp, min, max, nbins )
struct bu_hist	*histp;
fastf_t			min, max;
int			nbins;
{

	if( max <= min )  max = min+1;
	if( nbins < 1 )  {
		nbins = 1;	/* nbins=1 makes for a nice 2-bin binary histogram */
	} else if( nbins > 10000 )  {
		nbins = 10000;	/* This is a lot of lines to print out */
	}

	histp->hg_min = floor(min);
	histp->hg_max = ceil(max);
	histp->hg_nbins = nbins;

	histp->hg_clumpsize = ((max-min)/nbins);
	if( histp->hg_clumpsize <= 0 )  histp->hg_clumpsize = 1;

	histp->hg_nsamples = 0L;
	histp->hg_bins = (long *)bu_calloc( nbins+1, sizeof(long), "bu_hist bins");
	histp->magic = BU_HIST_MAGIC;
}

/*
 *			B U _ H I S T _ R A N G E
 */
void
bu_hist_range( hp, low, high )
register struct bu_hist	*hp;
fastf_t				low;
fastf_t				high;
{
	long		a;
	long		b;
	register int	i;

	BU_CK_HIST(hp);
	if( low <= hp->hg_min )
		a = 0;
	else
		a = (low - hp->hg_min) / hp->hg_clumpsize;
	if( high >= hp->hg_max )
		b = hp->hg_nbins-1;
	else
		b = (high - hp->hg_min) / hp->hg_clumpsize;
	if( a < 0 || b >= hp->hg_nbins )  bu_bomb("bu_hist_range() out of range\n");
	for( i=a; i <= b; i++ )  {
		hp->hg_bins[i]++;
	}
	hp->hg_nsamples++;
}

/*
 *			B U _ H I S T _ P R _ S U P P R E S S
 *
 *  Allows caller control over zero-suppression feature.
 */
void
bu_hist_pr_suppress( histp, title, zero_suppress )
register struct bu_hist	*histp;
CONST char		*title;
int			zero_suppress;
{
	register int	i;
	long		maxcount;
	static CONST char	marks[] = "################################################################";
#define	NMARKS	50
	char		buf[256];
	int		percent;
	int		mark_count;
	double		val;
	int		nbins;

	BU_CK_HIST(histp);

	/* Find entry with highest count */
	maxcount = 0L;
	for( i=0; i<=histp->hg_nbins; i++ )  {
		if( histp->hg_bins[i] > maxcount )
			maxcount = histp->hg_bins[i];
	}
	if( maxcount <= 0 )  maxcount = 1;

	nbins = histp->hg_nbins;
	if( zero_suppress )  {
		/* Supress trailing bins with zero counts.  nbins s/b >= 1 */
		for( ; nbins >= 1; nbins-- )
			if(histp->hg_bins[nbins] > 0)  break;
	}

	/* 12345678 12345678 123 .... */
	bu_log("\nHistogram of %s\nmin=%g, max=%g, nbins=%d, clumpsize=%g\n%d samples collected, highest count was %d\n\n Value      Count Rel%%|  Bar Graph\n",
		title,
		histp->hg_min, histp->hg_max,
		histp->hg_nbins, histp->hg_clumpsize,
		histp->hg_nsamples, maxcount );

	/* Print each bin. */
	i = 0;
	if( zero_suppress )  {
		/* Leading bins with zero counts are supressed. */
		for( ; i <= nbins; i++ )  {
			if(histp->hg_bins[i] > 0)  break;
		}
	}
	for( ; i <= nbins; i++ )  {
		percent = (int)(((double)histp->hg_bins[i])*100.0/maxcount);
		mark_count = percent*NMARKS/100;
		if( mark_count <= 0 && histp->hg_bins[i] > 0 )
			mark_count = 1;
		if( mark_count > NMARKS )  {
			bu_log("mark_count = %d, NMARKS=%d, hg_bins[%d]=%d, maxcount\n",
				mark_count, NMARKS, i, histp->hg_bins[i], maxcount);
			bu_bomb("bu_hist_pr() bogus mark_count\n");
		}
		if( mark_count <= 0 )  {
			buf[0] = '\0';
		} else {
			bcopy( marks, buf, mark_count );
			buf[mark_count] = '\0';
		}
		val = histp->hg_min + i*histp->hg_clumpsize;
		bu_log("%8g %8d %3d |%s\n",
			val,
			histp->hg_bins[i], percent, buf );
	}
}

/*
 *			B U _ H I S T _ P R
 *
 *  The original interface.
 */
void
bu_hist_pr( histp, title )
register struct bu_hist	*histp;
CONST char			*title;
{
	bu_hist_pr_suppress( histp, title, 1 );
}
