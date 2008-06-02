/*                   G E D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file ged_private.h
 *
 * Private header for libged.
 *
 */

#ifndef __GED_PRIVATE_H__
#define __GED_PRIVATE_H__

#include "ged.h"

__BEGIN_DECLS

#define GED_MAX_LEVELS 12
#define GED_CPEVAL	0
#define GED_LISTPATH	1
#define GED_LISTEVAL	2
#define GED_EVAL_ONLY	3

/*
 * rt_comb_ifree() should NOT be used here because
 * it doesn't know how to free attributes.
 * rt_db_free_internal() should be used instead.
 */
#define USE_RT_COMB_IFREE 0

struct ged_trace_data {
    struct ged	      *gtd_gedp;
    struct directory  *gtd_path[GED_MAX_LEVELS];
    struct directory  *gtd_obj[GED_MAX_LEVELS];
    mat_t	      gtd_xform;
    int		      gtd_objpos;
    int		      gtd_prflag;
    int		      gtd_flag;
};

BU_EXTERN (int ged_get_obj_bounds,
	   (struct ged		*gedp,
	    int			argc,
	    const char		**argv,
	    int			use_air,
	    point_t		rpp_min,
	    point_t		rpp_max));

BU_EXTERN (int ged_get_obj_bounds2,
	   (struct ged			*gedp,
	    int				argc,
	    const char			**argv,
	    struct ged_trace_data	*gtdp,
	    point_t			rpp_min,
	    point_t			rpp_max));

BU_EXTERN (void ged_trace,
	   (register struct directory	*dp,
	    int				pathpos,
	    const mat_t			old_xlate,
	    struct ged_trace_data	*gtdp));

__END_DECLS

#endif /* __GED_PRIVATE_H__ */

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
