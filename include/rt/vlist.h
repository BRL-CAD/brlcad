/*                      V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file rt/vlist.h
 *
 */

#ifndef RT_VLIST_H
#define RT_VLIST_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * TODO - replace these with the appropriate libbn calls specifically
 * passing &RTG.rtg_vlfree
 */
RT_EXPORT extern void rt_vlist_copy(struct bu_list *dest,
				    const struct bu_list *src);
RT_EXPORT extern void rt_vlist_cleanup(void);
RT_EXPORT extern void rt_vlist_import(struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);
RT_EXPORT extern struct bv_vlblock *    rt_vlblock_init(void);



/************************************************************************
 *                                                                      *
 *                      UNIX-Plot VLIST import/export routines          *
 *                                                                      *
 ************************************************************************/

RT_EXPORT extern int rt_process_uplot_value(struct bu_list **vhead,
					    struct bv_vlblock *vbp,
					    FILE *fp,
					    int c,
					    double char_size,
					    int mode);


/**
 * Read a BRL-style 3-D UNIX-plot file into a vector list.  For now,
 * discard color information, only extract vectors.  This might be
 * more naturally located in mged/plot.c
 */
RT_EXPORT extern int rt_uplot_to_vlist(struct bv_vlblock *vbp,
				       FILE *fp,
				       double char_size,
				       int mode);


/**
 * Used by MGED's "labelvert" command.
 */
RT_EXPORT extern void rt_label_vlist_verts(struct bv_vlblock *vbp,
					   struct bu_list *src,
					   mat_t mat,
					   double sz,
					   double mm2local);

/**
 * Used by MGED's "labelface" command.
 */
RT_EXPORT extern void rt_label_vlist_faces(struct bv_vlblock *vbp,
					   struct bu_list* f_list,
					   mat_t mat,
					   double sz,
					   double mm2local);


__END_DECLS

#endif /* RT_VLIST_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
