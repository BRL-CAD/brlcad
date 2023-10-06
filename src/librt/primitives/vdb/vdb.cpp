/*                       V D B . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2023 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/vdb/vdb.c[[
 *
 * Intersect a ray with a VDB object.
 *
 */

#include "common.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "openvdb/tools/LevelSetSphere.h"
#include "nanovdb/util/IO.h"
#include "nanovdb/util/Primitives.h"
#include "openvdb/openvdb.h"
#include "openvdb/math/Ray.h"
#include "openvdb/tools/RayIntersector.h"
#include "nanovdb/util/NanoToOpenVDB.h"
#include "nanovdb/util/OpenToNanoVDB.h"
#include "nanovdb/util/HDDA.h"
#include "nanovdb/util/GridBuilder.h"
#include "openvdb/io/Stream.h"

#include "../../librt_private.h"


#ifdef __cplusplus
extern "C" {
#endif

	
struct vdb_specific {
	openvdb::GridBase::Ptr vdb_grid; /* pointer to the VDB grid */
	vect_t vdb_hitNormal; /* normal of the ray tracing hit */
	vect_t vdb_hitPoint; /* 3D position of the ray tracing hit */
};

const struct bu_structparse rt_vdb_parse[] = {
{"%s", RT_VDB_NAME_LEN, "file", bu_offsetof(struct rt_vdb_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
{"%s", RT_VDB_NAME_LEN, "name", bu_offsetof(struct rt_vdb_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
{"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

nanovdb::GridHandle<nanovdb::HostBuffer> vdbHandle;

point_t* get_corners(point_t min, point_t max) {

	point_t corners[8];
	
	VSET(corners[0], min[0], min[1], min[2]);
	VSET(corners[1], min[0], max[1], min[2]);
	VSET(corners[2], max[0], min[1], min[2]);
	VSET(corners[3], min[0], min[1], max[2]);
	VSET(corners[4], max[0], max[1], min[2]);
	VSET(corners[5], max[0], min[1], max[2]);
	VSET(corners[6], min[0], max[1], max[2]);
	VSET(corners[7], max[0], max[1], max[2]);
	return corners;

}

#define PLOT_FACE(vlist_vlfree, vlist_head, arb_pts, a, b, c, d) \
BV_ADD_VLIST(vlist_vlfree, vlist_head, arb_pts[a], BV_VLIST_LINE_MOVE); \
BV_ADD_VLIST(vlist_vlfree, vlist_head, arb_pts[b], BV_VLIST_LINE_DRAW); \
BV_ADD_VLIST(vlist_vlfree, vlist_head, arb_pts[c], BV_VLIST_LINE_DRAW); \
BV_ADD_VLIST(vlist_vlfree, vlist_head, arb_pts[d], BV_VLIST_LINE_DRAW); \
BV_ADD_VLIST(vlist_vlfree, vlist_head, arb_pts[a], BV_VLIST_LINE_DRAW);


/**
 * Plot the bounding box of the VDB object.
 */
int
rt_vdb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
	point_t *pts;
	struct rt_vdb_internal *aip;
	struct bu_list *vlfree = &RTG.rtg_vlfree;

	BU_CK_LIST_HEAD(vhead);
	RT_CK_DB_INTERNAL(ip);

	struct rt_vdb_internal *vdbip = (struct rt_vdb_internal *)ip->idb_ptr;
	RT_VDB_CK_MAGIC(vdbip);

	nanovdb::GridHandle<nanovdb::HostBuffer> *vdbHandler = (nanovdb::GridHandle<nanovdb::HostBuffer> *)vdbip->vdb;
	nanovdb::NanoGrid <float>* h_grid = vdbHandler->grid<float>();

	point_t minBoundingBox;
	VSET(minBoundingBox, h_grid->worldBBox().min()[0], h_grid->worldBBox().min()[1], h_grid->worldBBox().min()[2]);

	point_t maxBoundingBox;
	VSET(maxBoundingBox, h_grid->worldBBox().max()[0], h_grid->worldBBox().max()[1], h_grid->worldBBox().max()[2]);

	pts = get_corners(minBoundingBox, maxBoundingBox);

	PLOT_FACE(vlfree, vhead, pts, 0, 1, 4, 2);
	PLOT_FACE(vlfree, vhead, pts, 0, 1, 6, 3);
	PLOT_FACE(vlfree, vhead, pts, 2, 4, 7, 5);
	PLOT_FACE(vlfree, vhead, pts, 3, 6, 7, 5);

	return 0;
}

int rt_vdb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
		
	struct rt_vdb_internal *eip;
	struct bu_vls str = BU_VLS_INIT_ZERO;

	if (dbip) RT_CK_DBI(dbip);
	RT_CK_DB_INTERNAL(ip);
	BU_CK_EXTERNAL(ep);

	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_VDB;
	ip->idb_meth = &OBJ[ID_VDB];
	BU_ALLOC(ip->idb_ptr, struct rt_vdb_internal);

	eip = (struct rt_vdb_internal *)ip->idb_ptr;
	eip->magic = RT_VDB_INTERNAL_MAGIC;

	bu_vls_strncpy(&str, (const char *)ep->ext_buf, ep->ext_nbytes);

	if (bu_struct_parse(&str, rt_vdb_parse, (char *)eip, NULL) < 0) {
		bu_vls_free(&str);
		return -2;
	}
	bu_vls_free(&str);
	mat = bn_mat_identity;
		
	openvdb::initialize();

	openvdb::io::File file(eip->name);

	file.open();

	/* Get the first grid. Some modification must be done to get more than one grid in the same file. */
	openvdb::GridBase::Ptr baseGrid = file.getGrids()->at(0);
		
	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

	/* Convert the OpenVDB grid into a NanoVDB grid handle. */
	vdbHandle = nanovdb::openToNanoVDB(*grid);

	eip->vdb = &vdbHandle;

	return 0;		/* OK */
}


int rt_vdb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
		
	struct rt_vdb_internal *vip;
	struct rt_vdb_internal vdb;
	struct bu_vls str = BU_VLS_INIT_ZERO;

	if (dbip) RT_CK_DBI(dbip);

	RT_CK_DB_INTERNAL(ip);
	if (ip->idb_type != ID_VDB) return -1;
	vip = (struct rt_vdb_internal *)ip->idb_ptr;
	RT_VDB_CK_MAGIC(vip);
	vdb = *vip;			/* struct copy */

	BU_CK_EXTERNAL(ep);

	bu_vls_struct_print(&str, rt_vdb_parse, (char *)&vdb);
	ep->ext_nbytes = bu_vls_strlen(&str)+1;
	ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "vdb external");

	bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);
	bu_vls_free(&str);

	return 0;
}

/**
 * Compute the bounding box of the VDB object.
 */
int
rt_vdb_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {

	struct rt_vdb_internal *eip = (struct rt_vdb_internal *)ip->idb_ptr;
	RT_VDB_CK_MAGIC(eip);

	nanovdb::GridHandle<nanovdb::HostBuffer> *vdbHandler = (nanovdb::GridHandle<nanovdb::HostBuffer> *)eip->vdb;
	nanovdb::NanoGrid <float>* h_grid = vdbHandler->grid<float>();
		
	vect_t minBoundingBox;
	VSET(minBoundingBox, h_grid->worldBBox().min()[0], h_grid->worldBBox().min()[1], h_grid->worldBBox().min()[2]);

	vect_t maxBoundingBox;
	VSET(maxBoundingBox, h_grid->worldBBox().max()[0], h_grid->worldBBox().max()[1], h_grid->worldBBox().max()[2]);


	(*min)[X] = minBoundingBox[0];
	(*max)[X] = maxBoundingBox[0];

	(*min)[Y] = minBoundingBox[1];
	(*max)[Y] = maxBoundingBox[1];

	(*min)[Z] = minBoundingBox[2];
	(*max)[Z] = maxBoundingBox[2];

	return 0;

}

/**
 * Prepare the data to perform ray tracing.
 * The min and max values of the bouxing box is stored 
 * and the bounding sphere is stored. 
 */
int rt_vdb_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct vdb_specific *vdb;
	struct rt_vdb_internal *eip2;

	eip2 = (struct rt_vdb_internal *)ip->idb_ptr;
	RT_VDB_CK_MAGIC(eip2);

	BU_GET(vdb, struct vdb_specific);
	stp->st_specific = (void *)vdb;
		
	vect_t work;
	fastf_t f;

	nanovdb::GridHandle<nanovdb::HostBuffer> *vdbHandler = (nanovdb::GridHandle<nanovdb::HostBuffer> *)eip2->vdb;
	vdb->vdb_grid = nanovdb::nanoToOpenVDB(*vdbHandler);
		
	nanovdb::NanoGrid <float>* h_grid = vdbHandler->grid<float>();
		
	vect_t minBoundingBox;
	VSET(minBoundingBox, h_grid->worldBBox().min()[0], h_grid->worldBBox().min()[1], h_grid->worldBBox().min()[2]);

	vect_t maxBoundingBox;
	VSET(maxBoundingBox, h_grid->worldBBox().max()[0], h_grid->worldBBox().max()[1], h_grid->worldBBox().max()[2]);

	VMINMAX(stp->st_min, stp->st_max, minBoundingBox);
	VMINMAX(stp->st_min, stp->st_max, maxBoundingBox);

	vect_t diam;
	vect_t radvec;
	VSUB2(diam, stp->st_max, stp->st_min);
	VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
	VSCALE(radvec, diam, 0.5);
	stp->st_aradius = stp->st_bradius = MAGNITUDE(radvec);
		
	return 0;			/* OK */
}


/**
 * Intersect a ray with a VDB object.
 * A second ray is casted to compute the t_out.
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_vdb_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{

	using GridT = openvdb::FloatGrid;
	using CoordT = openvdb::Coord;
	using RealT = float;
	using Vec3T = openvdb::math::Vec3<RealT>;
	using RayT = openvdb::math::Ray<RealT>;

	struct vdb_specific *vdb =
		(struct vdb_specific *)stp->st_specific;
	struct seg *segp;

	auto srcGrid = vdb->vdb_grid;
	openvdb::FloatGrid* h_grid = (openvdb::FloatGrid *)srcGrid.get();

	auto  indexBBox = h_grid->evalActiveVoxelBoundingBox();
	auto  gridXform = h_grid->transformPtr();
	auto  worldBBox = gridXform->indexToWorld(indexBBox);
	float wBBoxDimZ = (float)worldBBox.extents()[2] * 2;

	openvdb::tools::LevelSetRayIntersector<GridT, openvdb::tools::LinearSearchImpl<GridT, 0, RealT>, GridT::TreeType::RootNodeType::ChildNodeType::LEVEL, RayT> intersector(*h_grid);
	
	Vec3T rayEye(rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	Vec3T rayDir(rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	rayDir.normalize();
	
	RayT wRay(rayEye, rayDir);
	float tIn;
	Vec3T normal(0, 0, 0);
	Vec3T worldHit(0, 0, 0);
	
	/* Intersect in worlds space */
	if (intersector.intersectsWS(wRay, worldHit, normal, tIn)) {
			
		VSET(vdb->vdb_hitNormal, normal.x(), normal.y(), normal.z());
		VSET(vdb->vdb_hitPoint, worldHit.x(), worldHit.y(), worldHit.z());
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = tIn;
			
		/* Shoot secondary ray to find t_out */
		float offset = 0.001; /* offset to avoid self intersection */
		Vec3T rayOutPosition = worldHit + (offset*rayDir);
		RayT outRay(rayOutPosition, rayDir);
		Vec3T normal2(0, 0, 0);
		Vec3T worldHit2(0, 0, 0);
		float tOut;
		
		/* If interct again, compute t_out*/
		if (intersector.intersectsWS(outRay, worldHit2, normal2, tOut)) {
			
			float wTOut = tOut * float(h_grid->voxelSize()[0]);
			segp->seg_out.hit_dist = tOut + tIn;
				
		}
		else {

			segp->seg_out.hit_dist = tIn; /* t_out = t_in */
				
		}
			
		BU_LIST_INSERT(&(seghead->l), &(segp->l));

	}
	else {  /* did not hit */
		return 0;
	}

	return 2;

}

/**
 * Return the normal and 3D position of the hit.
 */
void
rt_vdb_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{

	struct vdb_specific *vdb = (struct vdb_specific *)stp->st_specific;

	VSET(hitp->hit_normal, vdb->vdb_hitNormal[0], vdb->vdb_hitNormal[1], vdb->vdb_hitNormal[2]);
	VSET(hitp->hit_point, vdb->vdb_hitPoint[0], vdb->vdb_hitPoint[1], vdb->vdb_hitPoint[2]);

}

#ifdef __cplusplus
}
#endif