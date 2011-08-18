/*			G _ M E T A B A L L . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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

int rt_metaball_bbox(struct rt_db_internal *ip, point_t *min, point_t *max);
fastf_t rt_metaball_get_bounding_sphere(point_t *center, fastf_t threshold, struct rt_metaball_internal *mb);
int rt_metaball_find_intersection(point_t *intersect, const struct rt_metaball_internal *mb, const point_t *a, const point_t *b, fastf_t step, const fastf_t finalstep);
void rt_metaball_norm_internal(vect_t *n, point_t *p, struct rt_metaball_internal *mb);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
