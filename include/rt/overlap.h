/*                       O V E R L A P . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/overlap.h
 *
 */

#ifndef RT_OVERLAP_H
#define RT_OVERLAP_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/ray_partition.h"
#include "rt/region.h"

__BEGIN_DECLS

/**
 * Default version of a_multioverlap().
 *
 * Resolve the overlap of multiple regions within a single partition.
 * There are no null pointers in the table (they have been compressed
 * out by our caller).  Consider BU_PTBL_LEN(regiontable) overlapping
 * regions, and reduce to zero or one "claiming" regions, by setting
 * pointers in the bu_ptbl of non-claiming regions to NULL.
 *
 * This default routine reproduces the behavior of BRL-CAD Release 5.0
 * by considering the regions pairwise and calling the old
 * a_overlap().
 *
 * An application which knew how to handle multiple overlapping air
 * regions would provide its own very different version of this
 * routine as the a_multioverlap() handler.
 *
 * This routine is for resolving overlaps only, and should not print
 * any messages in normal operation; a_logoverlap() is for logging.
 *
 * InputHdp is the list of partitions up to this point.  It allows us
 * to look at the regions that have come before in deciding what to do
 */
RT_EXPORT extern void rt_default_multioverlap(struct application *ap,
					      struct partition *pp,
					      struct bu_ptbl *regiontable,
					      struct partition *InputHdp);

/**
 * If an application doesn't want any logging from LIBRT, it should
 * just set ap->a_logoverlap = rt_silent_logoverlap.
 */
RT_EXPORT extern void rt_silent_logoverlap(struct application *ap,
					   const struct partition *pp,
					   const struct bu_ptbl *regiontable,
					   const struct partition *InputHdp);

/**
 * Log a multiplicity of overlaps within a single partition.  This
 * function is intended for logging only, and a_multioverlap() is
 * intended for resolving the overlap, only.  This function can be
 * replaced by an application setting a_logoverlap().
 */
RT_EXPORT extern void rt_default_logoverlap(struct application *ap,
					    const struct partition *pp,
					    const struct bu_ptbl *regiontable,
					    const struct partition *InputHdp);

/**
 * XXX This routine seems to free things more than once.  For a
 * temporary measure, don't free things.
 */
RT_EXPORT extern void rt_rebuild_overlaps(struct partition      *PartHdp,
					  struct application    *ap,
					  int rebuild_fastgen_plates_only);

/**
 * Default handler for overlaps in rt_boolfinal().
 *
 * Returns -
 * 0 to eliminate partition with overlap entirely
 * 1 to retain partition in output list, claimed by reg1
 * 2 to retain partition in output list, claimed by reg2
 */
RT_EXPORT extern int    rt_defoverlap(struct application *ap,
				      struct partition *pp,
				      struct region *reg1,
				      struct region *reg2,
				      struct partition *pheadp);

__END_DECLS

#endif /* RT_OVERLAP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
