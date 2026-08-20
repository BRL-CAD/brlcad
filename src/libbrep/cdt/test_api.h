/*                    C D T _ T E S T _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef LIBBREP_CDT_TEST_API_H
#define LIBBREP_CDT_TEST_API_H

#include "brep/defines.h"

/* These entry points are used by libbrep's regression executables.  Keep
 * them out of the installed public cdt.h interface; BREP_EXPORT is retained
 * only so shared-library test executables can resolve them on Windows. */
struct cdt_bmesh_impl;
struct cdt_bmesh {
    struct cdt_bmesh_impl *i;
};

__BEGIN_DECLS

extern BREP_EXPORT int cdt_bmesh_create(struct cdt_bmesh **m);
extern BREP_EXPORT void cdt_bmesh_destroy(struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_deserialize(const char *fname,
	struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_repair(struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_test_boundary_start(void);
extern BREP_EXPORT int cdt_test_boundary_steiner_filter(void);
extern BREP_EXPORT int cdt_test_spurious_components(void);
extern BREP_EXPORT int cdt_test_local_defects(void);
extern BREP_EXPORT int cdt_test_edge_singular_pair(void);
extern BREP_EXPORT int cdt_test_closed_edge_seed_policy(void);
extern BREP_EXPORT int cdt_test_linear_edge_spacing(void);
extern BREP_EXPORT int cdt_test_bounded_edge_midpoint(void);
extern BREP_EXPORT int cdt_test_assembled_mesh_validation(void);
extern BREP_EXPORT int cdt_test_assembled_shared_chords(void);
extern BREP_EXPORT int cdt_test_repair_edge_tube(void);
extern BREP_EXPORT int cdt_test_repair_triangle_split(void);
extern BREP_EXPORT int cdt_test_repair_patch_limits(void);
extern BREP_EXPORT int cdt_test_repair_duplicate_quarantine(void);
extern BREP_EXPORT int cdt_test_repair_periodic_strip(void);
extern BREP_EXPORT int cdt_test_repair_rigorous_boundary(void);
extern BREP_EXPORT int cdt_test_repair_patch_boundary(void);
extern BREP_EXPORT int cdt_test_subtolerance_edge_collapse(void);
extern BREP_EXPORT int cdt_test_subtolerance_ring(void);
extern BREP_EXPORT int cdt_test_developable_clean(void);

__END_DECLS

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
