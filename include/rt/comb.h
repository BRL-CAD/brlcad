/*                      C O M B . H
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
/** @file rt/comb.h
 *
 */

#ifndef RT_COMB_H
#define RT_COMB_H

#include "common.h"

#ifdef __cplusplus
#  include "opennurbs.h"
#endif

#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* db_comb.c */

/**
 * Import a combination record from a V4 database into internal form.
 */
RT_EXPORT extern int rt_comb_import4(struct rt_db_internal      *ip,
				     const struct bu_external   *ep,
				     const mat_t                matrix,         /* NULL if identity */
				     const struct db_i          *dbip,
				     struct resource            *resp);

RT_EXPORT extern int rt_comb_export4(struct bu_external                 *ep,
				     const struct rt_db_internal        *ip,
				     double                             local2mm,
				     const struct db_i                  *dbip,
				     struct resource                    *resp);


RT_EXPORT extern void db_comb_describe(struct bu_vls    *str,
				       const struct rt_comb_internal    *comb,
				       int              verbose,
				       double           mm2local);

/**
 * OBJ[ID_COMBINATION].ft_describe() method
 */
RT_EXPORT extern int rt_comb_describe(struct bu_vls     *str,
				      const struct rt_db_internal *ip,
				      int               verbose,
				      double            mm2local);

/**
 * fills in rgb with the color for a given comb combination
 *
 * returns truthfully if a color could be got.  note that this routine
 * will not (and cannot) handle the color inherit/override flag as
 * that is set on some higher-level parent combination.
 *
 */
RT_EXPORT extern int rt_comb_get_color(unsigned char rgb[3], const struct rt_comb_internal *comb);


/**
 * change all matching object names in the comb tree from old_name to
 * new_name
 *
 * calling function must supply an initialized bu_ptbl, and free it
 * once done.
 */
RT_EXPORT extern int db_comb_mvall(struct directory *dp,
				   struct db_i *dbip,
				   const char *old_name,
				   const char *new_name,
				   struct bu_ptbl *stack);

/* db5_comb.c */

/**
 * Read a combination object in v5 external (on-disk) format, and
 * convert it into the internal format described in rtgeom.h
 *
 * This is an unusual conversion, because some of the data is taken
 * from attributes, not just from the object body.  By the time this
 * is called, the attributes will already have been cracked into
 * ip->idb_avs, we get the attributes from there.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/**
 * Return a RT_DIR_NULL terminated array of directory pointers that
 * holds the set of immediate children associated with comb.  The
 * caller is responsible for freeing the array, but not the directory
 * structures pointed to by the array.
 *
 * Optionally, pointers may also be supplied to collect arrays holding
 * the boolean operations and matrices associated with the comb entries.
 * For boolean operations, the caller is responsible for freeing the
 * array.  For matrices, both the array and the matrices themselves
 * must be freed by the caller.  The boolean operations array is zero
 * terminated, the matrix array is NULL terminated.  For example:
 *
 * @code
 * int i = 0;
 * struct directory *wdp;
 * struct directory **children = NULL;
 * int *bool_ops = NULL;
 * matp_t *matrices = NULL;
 * matp_t m;
 * db_comb_children(dbip, comb, &children, &bool_ops, &matrices);
 * if (db_comb_children(dbip, comb, &bool_ops, &matrices) > 0) {
 *    i = 0;
 *    wdp = children[0];
 *    while (wdp != RT_DIR_NULL) {
 *       char obuf[1024];
 *       bu_log("%s child %d: %d %s\n", dp->d_namep, i, bool_ops[ind], wdp->d_namep);
 *       if (mats[ind]){
 *          bn_mat_print_guts("", mats[i], obuf, 1024);
 *          bu_log("%s %s\n", wdp->d_namep, obuf);
 *       }
 *       i++;
 *       wdp = children[i];
 *    }
 * }
 * i = 0;
 * while (mats[i]) {
 *    bu_free(mats[i], "free matrix");
 *    i++;
 * }
 * bu_free(mats, "free mats array");
 * bu_free(bool_ops, "free ops");
 * bu_free(children, "free children struct directory ptr array");
 * @endcode
 */
RT_EXPORT extern int db_comb_children(struct db_i *dbip, struct rt_comb_internal *comb, struct directory ***children, int **bool_ops, matp_t **mats);

#ifdef __cplusplus
RT_EXPORT extern void
rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol, const struct db_i *dbip);
#endif

__END_DECLS

#endif /* RT_COMB_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
