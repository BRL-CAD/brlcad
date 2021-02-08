/*                        P S C A L E . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file pscale.h
 *
 * Brief description
 *
 */

#ifndef GED_PSCALE_H
#define GED_PSCALE_H

#include "common.h"

#include "ged.h"

__BEGIN_DECLS

/* defined in scale_ehy.c */
GED_EXPORT extern int _ged_scale_ehy(struct ged *gedp,
			  struct rt_ehy_internal *ehy,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_ell.c */
GED_EXPORT extern int _ged_scale_ell(struct ged *gedp,
			  struct rt_ell_internal *ell,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_epa.c */
GED_EXPORT extern int _ged_scale_epa(struct ged *gedp,
			  struct rt_epa_internal *epa,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_eto.c */
GED_EXPORT extern int _ged_scale_eto(struct ged *gedp,
			  struct rt_eto_internal *eto,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_extrude.c */
GED_EXPORT extern int _ged_scale_extrude(struct ged *gedp,
			      struct rt_extrude_internal *extrude,
			      const char *attribute,
			      fastf_t sf,
			      int rflag);

/* defined in scale_hyp.c */
GED_EXPORT extern int _ged_scale_hyp(struct ged *gedp,
			  struct rt_hyp_internal *hyp,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

__END_DECLS

#endif /* LIBGED_GED_PRIVATE_H */

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
