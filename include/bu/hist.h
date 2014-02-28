/*                         H I S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/**  @defgroup data Data Management */
/**   @defgroup hist Histogram Handling */

/** @file hist.h
 *
 */
#ifndef BU_HIST_H
#define BU_HIST_H

#include "common.h"

#include <stddef.h> /* for size_t */
#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#include "bu/defines.h"
#include "bu/magic.h"

/** @addtogroup hist */
/** @{ */
/** @file libbu/hist.c
 *
 * General purpose histogram handling routines.
 *
 * The subroutine bu_hist_range() is used to record items that may
 * extend across multiple "bin"s.
 *
 */

/**
 * histogram support
 */
struct bu_hist  {
    uint32_t magic;		/**< magic # for id/check */
    fastf_t hg_min;		/**< minimum value */
    fastf_t hg_max;		/**< maximum value */
    fastf_t hg_clumpsize;	/**< (max-min+1)/nbins+1 */
    size_t hg_nsamples;		/**< total number of samples spread into histogram */
    size_t hg_nbins;		/**< # of bins in hg_bins[]  */
    long *hg_bins;		/**< array of counters */
};
typedef struct bu_hist bu_hist_t;
#define BU_HIST_NULL ((struct bu_hist *)0)

/**
 * assert the integrity of a bu_hist struct.
 */
#define BU_CK_HIST(_p) BU_CKMAG(_p, BU_HIST_MAGIC, "struct bu_hist")

/**
 * initialize a bu_hist struct without allocating any memory.
 */
#define BU_HIST_INIT(_hp) { \
	(_hp)->magic = BU_HIST_MAGIC; \
	(_hp)->hg_min = (_hp)->hg_max = (_hp)->hg_clumpsize = 0.0; \
	(_hp)->hg_nsamples = (_hp)->hg_nbins = 0; \
	(_hp)->hg_bins = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hist struct.  does not allocate memory.
 */
#define BU_HIST_INIT_ZERO {BU_HIST_MAGIC, 0.0, 0.0, 0.0, 0, 0, NULL}

/**
 * returns truthfully whether a bu_hist has been initialized via
 * BU_HIST_INIT() or BU_HIST_INIT_ZERO.
 */
#define BU_HIST_IS_INITIALIZED(_hp) (((struct bu_hist *)(_hp) != BU_HIST_NULL) && LIKELY((_hp)->magic == BU_HIST_MAGIC))

#define BU_HIST_TALLY(_hp, _val) { \
	if ((_val) <= (_hp)->hg_min) { \
	    (_hp)->hg_bins[0]++; \
	} else if ((_val) >= (_hp)->hg_max) { \
	    (_hp)->hg_bins[(_hp)->hg_nbins]++; \
	} else { \
	    (_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)]++; \
	} \
	(_hp)->hg_nsamples++;  }

#define BU_HIST_TALLY_MULTIPLE(_hp, _val, _count) { \
	int __count = (_count); \
	if ((_val) <= (_hp)->hg_min) { \
	    (_hp)->hg_bins[0] += __count; \
	} else if ((_val) >= (_hp)->hg_max) { \
	    (_hp)->hg_bins[(_hp)->hg_nbins] += __count; \
	} else { \
	    (_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)] += __count; \
	} \
	(_hp)->hg_nsamples += __count;  }

/* hist.c */
/* These are a set of data histogramming routines. */

BU_EXPORT extern void bu_hist_free(struct bu_hist *histp);

/**
 * Initialize a bu_hist structure.
 *
 * It is expected that the structure is junk upon entry.
 */
BU_EXPORT extern void bu_hist_init(struct bu_hist *histp, fastf_t min, fastf_t max, size_t nbins);

BU_EXPORT extern void bu_hist_range(struct bu_hist *hp, fastf_t low, fastf_t high);

/**
 * Print a histogram.
 */
BU_EXPORT extern void bu_hist_pr(const struct bu_hist *histp, const char *title);

/** @} */


#endif  /* BU_HIST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
