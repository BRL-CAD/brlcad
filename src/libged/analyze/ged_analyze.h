/*                    G E D _ A N A L Y Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file ged_cg.h
 *
 * Brief description
 *
 */

#ifndef LIBGED_ANALYZE_GED_PRIVATE_H
#define LIBGED_ANALYZE_GED_PRIVATE_H

#include "common.h"

#include "vmath.h"
#include "./ged/defines.h"

__BEGIN_DECLS

/* Conversion factor for Gallons to cubic millimeters */
#define GALLONS_TO_MM3 3785411.784
#define NOT_A_PLANE -1

#define FBUFSIZ 100
#define NFIELDS 9

typedef struct row_field
{
    int nchars;
    char buf[FBUFSIZ];
} field_t;

typedef struct table_row
{
    int nfields;
    field_t fields[NFIELDS];
} row_t;

typedef struct table
{
    int nrows;
    row_t *rows;
} table_t;

#define ADD_PT(face, pt) do { VMOVE((face).pts[(face).npts], (pt)); (face).npts++; } while (0)

/* contains information used to analyze a polygonal face */
struct poly_face
{
    char label[5];
    size_t npts;
    point_t *pts;
    plane_t plane_eqn;
    fastf_t area;
};

GED_EXPORT extern void
print_edges_table(struct ged *gedp, table_t *table);

GED_EXPORT extern void
print_faces_table(struct ged *gedp, table_t *table);

GED_EXPORT extern void
print_volume_table(
	struct ged *gedp,
       	const fastf_t tot_vol,
       	const fastf_t tot_area,
       	const fastf_t tot_gallons
	);

#define POLY_FACE_INIT_ZERO { { 0, 0, 0, 0, 0 }, 0, NULL, HINIT_ZERO, 0.0 }

GED_EXPORT extern void
analyze_poly_face(struct ged *gedp, struct poly_face *face, row_t *row);

GED_EXPORT extern void
print_faces_table(struct ged *gedp, table_t *table);


GED_EXPORT extern void
analyze_arb8(struct ged *gedp, const struct rt_db_internal *ip);

GED_EXPORT extern void
analyze_arbn(struct ged *gedp, const struct rt_db_internal *ip);

GED_EXPORT extern void
analyze_ars(struct ged *gedp, const struct rt_db_internal *ip);

GED_EXPORT extern void
analyze_superell(struct ged *gedp, const struct rt_db_internal *ip);

GED_EXPORT extern void
analyze_sketch(struct ged *gedp, const struct rt_db_internal *ip);

GED_EXPORT extern void
analyze_general(struct ged *gedp, const struct rt_db_internal *ip);

__END_DECLS

#endif /* LIBGED_ANALYZE_GED_PRIVATE_H */

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
