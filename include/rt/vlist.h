/*                      V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
#include "vmath.h"
#include "bu/vls.h"

__BEGIN_DECLS

/************************************************************************
 *                                                                      *
 *                      Generic BN_VLIST routines                       *
 *                                                                      *
 ************************************************************************/

/**
 * Returns the description of a vlist cmd.
 */
RT_EXPORT extern const char *rt_vlist_get_cmd_description(int cmd);

/**
 * Validate an bn_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
RT_EXPORT extern int rt_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
RT_EXPORT extern void rt_vlist_copy(struct bu_list *dest,
                                    const struct bu_list *src);


/**
 * The macro RT_FREE_VLIST() simply appends to the list
 * &RTG.rtg_vlfree.  Now, give those structures back to bu_free().
 */
RT_EXPORT extern void bn_vlist_cleanup(struct bu_list *hd);


/**
 * XXX This needs to remain a LIBRT function.
 */
RT_EXPORT extern void rt_vlist_cleanup(void);



/************************************************************************
 *                                                                      *
 *                      Binary VLIST import/export routines             *
 *                                                                      *
 ************************************************************************/

/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
RT_EXPORT extern void rt_vlist_export(struct bu_vls *vls,
                                      struct bu_list *hp,
                                      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
RT_EXPORT extern void rt_vlist_import(struct bu_list *hp,
                                      struct bu_vls *namevls,
                                      const unsigned char *buf);

/************************************************************************
 *                                                                      *
 *                      UNIX-Plot VLIST import/export routines          *
 *                                                                      *
 ************************************************************************/

/**
 * Output a bn_vlblock object in extended UNIX-plot format, including
 * color.
 */
RT_EXPORT extern void rt_plot_vlblock(FILE *fp,
                                      const struct bn_vlblock *vbp);


/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
RT_EXPORT extern void rt_vlist_to_uplot(FILE *fp,
                                        const struct bu_list *vhead);
RT_EXPORT extern int rt_process_uplot_value(struct bu_list **vhead,
                                            struct bn_vlblock *vbp,
                                            FILE *fp,
                                            int c,
                                            double char_size,
                                            int mode);


/**
 * Read a BRL-style 3-D UNIX-plot file into a vector list.  For now,
 * discard color information, only extract vectors.  This might be
 * more naturally located in mged/plot.c
 */
RT_EXPORT extern int rt_uplot_to_vlist(struct bn_vlblock *vbp,
                                       FILE *fp,
                                       double char_size,
                                       int mode);


/**
 * Used by MGED's "labelvert" command.
 */
RT_EXPORT extern void rt_label_vlist_verts(struct bn_vlblock *vbp,
                                           struct bu_list *src,
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
