/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include <cstdio>

#include "brep.h"
#include "bu/app.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "wdb.h"


static struct rt_i *
prepare_object(struct db_i *dbip, const char *name)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (rtip == NULL)
	return NULL;
    if (rt_gettree(rtip, name) != 0) {
	rt_i_destroy(rtip);
	return NULL;
    }
    rt_prep_parallel(rtip, 1);
    return rtip;
}


static int
make_difference(struct rt_wdb *writer, const char *name,
    const char *minuend, const char *subtrahend)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember(minuend, &members.l, NULL, WMOP_UNION) ||
	!mk_addmember(subtrahend, &members.l, NULL, WMOP_SUBTRACT))
    {
	mk_freemembers(&members.l);
	return -1;
    }
    return mk_lcomb(writer, name, &members, 1, NULL, NULL, NULL, 0);
}


static int
write_evaluated_brep(struct db_i *dbip, struct rt_wdb *writer,
    const char *source_name, const char *candidate_name)
{
    struct directory *entry = db_lookup(dbip, source_name, LOOKUP_QUIET);
    if (entry == NULL)
	return -1;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, entry, dbip, bn_mat_identity) < 0)
	return -1;

    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    struct rt_brep_conversion_info info = RT_BREP_CONVERSION_INFO_INIT;
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    const int status = rt_comb_brep_status_info(&converted, &internal,
	&tolerance, dbip, &info);
    rt_db_free_internal(&internal);

    const bool usable = status == BREP_BOOLEAN_SUCCESS && converted != NULL &&
	info.fidelity == RT_BREP_CONVERSION_FIDELITY_EXACT &&
	converted->IsValid() && converted->IsSolid();
    const int write_status = usable ?
	mk_brep(writer, candidate_name, converted) : -1;
    if (converted != NULL && converted != owner)
	delete converted;
    delete owner;
    return write_status;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    struct db_i *dbip = db_create_inmem();
    if (dbip == NULL)
	return 1;
    struct rt_wdb *writer = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (writer == NULL) {
	db_close(dbip);
	return 1;
    }

    /* The inner box stops 0.0001955 mm short of the outer maximum-Y face.
     * That is below BN_TOL_DIST.  CSG boolweave culls the resulting sliver;
     * Boolean evaluation collapses it onto the adjacent face and the B-Rep
     * ray path reports a floating-point-zero entry/exit pair there.  The
     * full-precision dimensions reproduce m35.g/r770 without depending on
     * the sample database. */
    const point_t outer_min = {
	2399.2841796880002, -914.90795898440001, 523.23999023440001
    };
    const point_t outer_max = {
	3166.8720703130002, -902.20795917512999, 951.73803710940001
    };
    const point_t inner_min = {
	2402.4589843745998, -911.73315429690001, 526.4150390625
    };
    const point_t inner_max = {
	3163.6970214839998, -902.20815467836997, 948.56298828130002
    };
    if (mk_rpp(writer, "outer.s", outer_min, outer_max) < 0 ||
	mk_rpp(writer, "inner.s", inner_min, inner_max) < 0 ||
	make_difference(writer, "source.r", "outer.s", "inner.s") < 0)
    {
	db_close(dbip);
	return 2;
    }
    db_update_nref(dbip);
    if (write_evaluated_brep(dbip, writer, "source.r", "candidate.brep") < 0)
    {
	db_close(dbip);
	return 3;
    }
    db_update_nref(dbip);

    struct rt_i *source = prepare_object(dbip, "source.r");
    struct rt_i *candidate = prepare_object(dbip, "candidate.brep");
    if (source == NULL || candidate == NULL) {
	if (source != NULL)
	    rt_i_destroy(source);
	if (candidate != NULL)
	    rt_i_destroy(candidate);
	db_close(dbip);
	return 4;
    }

    struct rt_crofton_params parameters = {};
    parameters.n_rays = 16384;
    struct rt_crofton_comparison comparison = RT_CROFTON_COMPARISON_INIT;
    const int comparison_status = rt_crofton_compare(&comparison, source,
	candidate, &parameters, 0, NULL, NULL);
    rt_i_destroy(candidate);
    rt_i_destroy(source);
    db_close(dbip);

    std::printf("degenerate Crofton comparison: segments %zu/%zu, "
	"numeric-zero %zu/%zu on %zu rays (maximum %.17g/%.17g), "
	"mismatch %.17g (sub-tolerance %zu, material %zu, maximum normal "
	"separation %.17g), endpoint %.17g, "
	"chord %.17g, area %.17g, volume %.17g\n",
	comparison.source_segment_count, comparison.candidate_segment_count,
	comparison.source_numerically_degenerate_segments,
	comparison.candidate_numerically_degenerate_segments,
	comparison.numerically_degenerate_rays,
	comparison.source_max_numerically_degenerate_length,
	comparison.candidate_max_numerically_degenerate_length,
	comparison.partition_mismatch_fraction,
	comparison.sub_tolerance_partition_mismatch_rays,
	comparison.material_partition_mismatch_rays,
	comparison.maximum_normal_partition_disagreement,
	comparison.endpoint_error_p99,
	comparison.chord_error_fraction, comparison.surface_area_error_fraction,
	comparison.volume_error_fraction);

    const bool exposed_culling_difference =
	comparison.candidate_segment_count > comparison.source_segment_count &&
	comparison.candidate_numerically_degenerate_segments >
	    comparison.source_numerically_degenerate_segments &&
	comparison.numerically_degenerate_rays > 0;
    const bool normalized_semantics_match = comparison_status == 0 &&
	comparison.ray_count == parameters.n_rays &&
	comparison.partition_mismatch_rays == 0 &&
	comparison.sub_tolerance_partition_mismatch_rays == 0 &&
	comparison.material_partition_mismatch_rays == 0 &&
	comparison.endpoint_error_p99 <= 1.0e-8 &&
	comparison.chord_error_fraction <= 1.0e-10 &&
	comparison.surface_area_error_fraction <= 1.0e-12 &&
	comparison.volume_error_fraction <= 1.0e-10;
    return exposed_culling_difference && normalized_semantics_match ? 0 : 5;
}
