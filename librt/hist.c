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
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ H I S T _ F R E E
 */
void
rt_hist_free( histp )
struct histogram	*histp;
{
	/* The 'clean' command can be called before the first rt_prep(),
	 * resulting in zero magic numbers in rtip->rti_hist_cellsize.
	 */
	if( histp && histp->magic == 0 )  return;
	RT_CK_HISTOGRAM(histp);
	if( histp->hg_bins )
		rt_free( (char *)histp->hg_bins, "old histogram bins");
	histp->hg_bins = (long *)0;
	histp->hg_nbins = 0;
	histp->magic = -1;	/* sanity */
}

/*
 *			R T _ H I S T _ I N I T
 *
 *  Initialize a histogram structure.
 *  It is expected that the structure is junk upon entry.
 */
void
rt_hist_init( histp, min, max, nbins )
struct histogram	*histp;
fastf_t			min, max;
int			nbins;
{

	if( max <= min )  max = min+1;
	if( nbins < 2 )  {
		nbins = 2;
	} else if( nbins > 10000 )  {
		nbins = 10000;
	}

	histp->hg_min = floor(min);
	histp->hg_max = ceil(max);
	histp->hg_nbins = nbins;

	/* When max-min <= nbins, clumpsize should be 1 */
	histp->hg_clumpsize = ((max-min)/nbins)+1;
	if( histp->hg_clumpsize <= 0 )  histp->hg_clumpsize = 1;

	histp->hg_nsamples = 0L;
	histp->hg_bins = (long *)rt_calloc( nbins+1, sizeof(long), "histogram bins");
	histp->magic = RT_HISTOGRAM_MAGIC;
}

/*
 *			R T _ H I S T _ R A N G E
 */
void
rt_hist_range( hp, low, high )
register struct histogram	*hp;
fastf_t				low;
fastf_t				high;
{
	long		a;
	long		b;
	register int	i;

	RT_CK_HISTOGRAM(hp);
	if( low <= hp->hg_min )
		a = 0;
	else
		a = (low - hp->hg_min) / hp->hg_clumpsize;
	if( high >= hp->hg_max )
		b = hp->hg_nbins-1;
	else
		b = (high - hp->hg_min) / hp->hg_clumpsize;
	if( a < 0 || b >= hp->hg_nbins )  rt_bomb("rt_hist_range() out of range\n");
	for( i=a; i <= b; i++ )  {
		hp->hg_bins[i]++;
	}
	hp->hg_nsamples++;
}

/*
 *			R T _ H I S T _ P R
 */
void
rt_hist_pr( histp, title )
register struct histogram	*histp;
CONST char			*title;
{
	register int	i;
	long		maxcount;
	static CONST char	marks[] = "################################################################";
#define	NMARKS	50
	char		buf[256];
	int		percent;
	int		mark_count;
	int		val;
	int		nbins;

	RT_CK_HISTOGRAM(histp);

	/* Find entry with highest count */
	maxcount = 0L;
	for( i=0; i<histp->hg_nbins; i++ )  {
		if( histp->hg_bins[i] > maxcount )
			maxcount = histp->hg_bins[i];
	}
	if( maxcount <= 0 )  maxcount = 1;

	/* Supress trailing bins with zero counts.  nbins s/b >= 1 */
	for( nbins = histp->hg_nbins-1; nbins >= 1; nbins-- )
		if(histp->hg_bins[nbins] > 0)  break;

	/* 12345678 12345678 123 .... */
	rt_log("\nHistogram of %s\nmin=%g, max=%g, nbins=%d, clumpsize=%g\n%d samples collected, highest count was %d\n\n Value      Count Rel%%|  Bar Graph\n",
		title,
		histp->hg_min, histp->hg_max,
		histp->hg_nbins, histp->hg_clumpsize,
		histp->hg_nsamples, maxcount );

	/* Print each bin.  Final bins with zero counts are supressed. */
	for( i=0; i <= nbins; i++ )  {
		percent = (int)(((double)histp->hg_bins[i])*100.0/maxcount);
		mark_count = percent*NMARKS/100;
		if( mark_count <= 0 )  {
			buf[0] = '\0';
		} else {
			bcopy( marks, buf, mark_count );
			buf[mark_count] = '\0';
		}
		val = histp->hg_min + i*histp->hg_clumpsize;
		rt_log("%8d %8d %3d |%s\n",
			val,
			histp->hg_bins[i], percent, buf );
	}
}
