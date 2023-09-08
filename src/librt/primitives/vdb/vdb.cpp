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


#include "../../librt_private.h"


#ifdef __cplusplus
extern "C" {
#endif

	// Initially with a bounding box, later with vdb
	struct vdb_specific {
		vect_t minBB;
		vect_t maxBB;
		openvdb::FloatGrid* grid;
	};

	int rt_vdb_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
	{

		struct rt_vdb_internal *eip;

		/* must be double for import and export */
		double vec[ELEMENTS_PER_VECT * 2]; // min and max = 2

		if (dbip) RT_CK_DBI(dbip);
		RT_CK_DB_INTERNAL(ip);
		BU_CK_EXTERNAL(ep);

		BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * 2); // min and max = 2

		ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
		ip->idb_type = ID_VDB;
		ip->idb_meth = &OBJ[ID_VDB];
		BU_ALLOC(ip->idb_ptr, struct rt_vdb_internal);

		eip = (struct rt_vdb_internal *)ip->idb_ptr;
		eip->magic = RT_VDB_INTERNAL_MAGIC;

		/* Convert from database (network) to internal (host) format */
		bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, ELEMENTS_PER_VECT * 2); // min and max = 2

		/* Apply modeling transformations */
		mat = bn_mat_identity;
		MAT4X3PNT(eip->minBB, mat, &vec[0 * ELEMENTS_PER_VECT]);
		MAT4X3VEC(eip->maxBB, mat, &vec[1 * ELEMENTS_PER_VECT]);

		//bu_log("min eip:  %g %g  %g \n", eip->minBB[0], eip->minBB[1], eip->minBB[2]);
		//bu_log("max eip:  %g %g  %g \n", eip->maxBB[0], eip->maxBB[1], eip->maxBB[2]);

		//bu_log("min vec:  %g %g  %g \n", vec[0], vec[1], vec[2]);
		//bu_log("max vec:  %g %g  %g \n", vec[1], vec[4], vec[5]);


		return 0;		/* OK */
	}

	/**
	 * The external format is: min vector and max vector
	 * later it will be a vdb
	 */
	int rt_vdb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
	{
		struct rt_vdb_internal *eip;

		/* must be double for import and export */
		double vec[ELEMENTS_PER_VECT * 2];

		if (dbip) RT_CK_DBI(dbip);

		RT_CK_DB_INTERNAL(ip);
		if (ip->idb_type != ID_VDB) return -1;
		eip = (struct rt_vdb_internal *)ip->idb_ptr;
		RT_VDB_CK_MAGIC(eip);

		BU_CK_EXTERNAL(ep);
		ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * 2;
		ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "vdb external");

		/* scale 'em into local buffer */
		VSCALE(&vec[0 * ELEMENTS_PER_VECT], eip->minBB, local2mm);
		VSCALE(&vec[1 * ELEMENTS_PER_VECT], eip->maxBB, local2mm);


		/* Convert from internal (host) to database (network) format */
		bu_cv_htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT * 2);

		return 0;
	}

	/**
	 * Compute the bounding
	 */
	int
		rt_vdb_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {

		struct rt_vdb_internal *eip = (struct rt_vdb_internal *)ip->idb_ptr;
		RT_VDB_CK_MAGIC(eip);


		(*min)[X] = eip->minBB[X];
		(*max)[X] = eip->maxBB[X];

		(*min)[Y] = eip->minBB[Y];
		(*max)[Y] = eip->maxBB[Y];

		(*min)[Z] = eip->minBB[Z];
		(*max)[Z] = eip->maxBB[Z];

		return 0;
	}


	int rt_vdb_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
	{
		register struct vdb_specific *vdb;
		struct rt_vdb_internal *eip2;

		eip2 = (struct rt_vdb_internal *)ip->idb_ptr;
		RT_VDB_CK_MAGIC(eip2);

		/* Solid is OK, compute constant terms now */
		BU_GET(vdb, struct vdb_specific);
		stp->st_specific = (void *)vdb;

		VMOVE(vdb->minBB, eip2->minBB);
		VMOVE(vdb->maxBB, eip2->maxBB);
		/*fprintf(stderr, "bb %lf %lf\n", eip2->minBB, eip2->maxBB);
		fprintf(stderr, "bb2 %lf %lf\n", vdb->minBB, vdb->maxBB);*/
		//bu_log("eip min:  %g %g  %g \n", eip2->minBB[0], eip2->minBB[1], eip2->minBB[2]);
		//bu_log("eip max:  %g %g  %g \n", eip2->maxBB[0], eip2->maxBB[1], eip2->maxBB[2]);
		//
		//bu_log("min vec:  %g %g  %g \n", vdb->minBB[0], vdb->minBB[1], vdb->minBB[2]);
		//bu_log("max vec:  %g %g  %g \n", vdb->maxBB[0], vdb->maxBB[1], vdb->maxBB[2]);


		/* Compute bounding sphere */
		/*vect_t center;
		VSUB2(center, eip2->maxBB, eip2->minBB);
		VMOVE(stp->st_center, center);

		stp->st_aradius = stp->st_bradius = 3.4f;


		if (rt_vdb_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;*/

		vect_t work;
		register fastf_t f;

		VMINMAX(stp->st_min, stp->st_max, eip2->maxBB);
		VMINMAX(stp->st_min, stp->st_max, eip2->minBB);

		//VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
		//VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);

		/*f = work[X];
		if (work[Y] > f) f = work[Y];
		if (work[Z] > f) f = work[Z];
		stp->st_aradius = f;
		stp->st_bradius = MAGNITUDE(work);*/

		nanovdb::GridHandle<nanovdb::HostBuffer> handle;
		handle = nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(1.0f, nanovdb::Vec3f(0, 0, 0), 0.01f, 3.0f, nanovdb::Vec3d(0), "sphere");
		//handle = nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(100.0f, nanovdb::Vec3f(0, 0, 0), 1.0, 3.0, nanovdb::Vec3d(0), "sphere");

		auto srcGrid = nanovdb::nanoToOpenVDB(handle);
		openvdb::FloatGrid* h_grid = (openvdb::FloatGrid *)srcGrid.get();
		vdb->grid = h_grid;

		VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
		VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);
		
		f = work[X];
		if (work[Y] > f) f = work[Y];
		if (work[Z] > f) f = work[Z];
		stp->st_aradius = f;
		stp->st_bradius = MAGNITUDE(work);


		return 0;			/* OK */
	}	


	/**
	 * Intersect a ray with an the bounding box - later it will be with vdb
	 * Returns -
	 * 0 MISS
	 * >0 HIT
	 */
	int
	rt_vdb_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
	{

		using GridT = openvdb::FloatGrid;
		using CoordT = openvdb::Coord;
		using RealT = float;
		using Vec3T = openvdb::math::Vec3<RealT>;
		using RayT = openvdb::math::Ray<RealT>;

		//fprintf(stderr, "shot\n");
		register struct vdb_specific *vdb =
			(struct vdb_specific *)stp->st_specific;
		register struct seg *segp;


		nanovdb::GridHandle<nanovdb::HostBuffer> handle;
		//handle = nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(1.0f, nanovdb::Vec3f(0, 0, 0), 0.01f, 3.0f, nanovdb::Vec3d(0), "sphere");
		handle = nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(1.0, nanovdb::Vec3f(0, 0, 0), 0.1f, 3.0, nanovdb::Vec3d(0), "sphere");

		auto srcGrid = nanovdb::nanoToOpenVDB(handle);
		openvdb::FloatGrid* h_grid = (openvdb::FloatGrid *)srcGrid.get();

		auto  indexBBox = h_grid->evalActiveVoxelBoundingBox();
		auto  gridXform = h_grid->transformPtr();
		auto  worldBBox = gridXform->indexToWorld(indexBBox);
		float wBBoxDimZ = (float)worldBBox.extents()[2] * 2;
		//bu_log("min vec:  %g \n", wBBoxDimZ);

		//fastf_t tmin, tmax, tymin, tymax, tzmin, tzmax;
		//int sign[3];

		//sign[0] = ((1.0 / rp->r_dir[0]) < 0);
		//sign[1] = ((1.0 / rp->r_dir[1]) < 0);
		//sign[2] = ((1.0 / rp->r_dir[2]) < 0);

		//vect_t bounds[2];
		//VMOVE(bounds[0], vdb->minBB);
		//VMOVE(bounds[1], vdb->maxBB);

		///*bu_log("min vec:  %g %g  %g \n", vdb->minBB[0], vdb->minBB[1], vdb->minBB[2]);
		//bu_log("max vec:  %g %g  %g \n", vdb->maxBB[0], vdb->maxBB[1], vdb->maxBB[2]);*/

		//tmin = (bounds[sign[0]][0] - rp->r_pt[0]) * (1.0 / rp->r_dir[0]);
		//tmax = (bounds[1 - sign[0]][0] - rp->r_pt[0]) * (1.0 / rp->r_dir[0]);
		//tymin = (bounds[sign[1]][1] - rp->r_pt[1]) * (1.0 / rp->r_dir[1]);
		//tymax = (bounds[1 - sign[1]][1] - rp->r_pt[1]) * (1.0 / rp->r_dir[1]);

		//if ((tmin > tymax) || (tymin > tmax))
		//	return 0;

		//if (tymin > tmin)
		//	tmin = tymin;
		//if (tymax < tmax)
		//	tmax = tymax;

		//tzmin = (bounds[sign[2]][2] - rp->r_pt[2]) * (1.0 / rp->r_dir[2]);
		//tzmax = (bounds[1 - sign[2]][2] - rp->r_pt[2]) * (1.0 / rp->r_dir[2]);

		//if ((tmin > tzmax) || (tzmin > tmax))
		//	return 0;

		//if (tzmin > tmin)
		//	tmin = tzmin;
		//if (tzmax < tmax)
		//	tmax = tzmax;

		//fprintf(stderr, "hit\n");

		//// recording the hit in and out - tmin and tmax
		//RT_GET_SEG(segp, ap->a_resource);
		//segp->seg_stp = stp;
		//segp->seg_in.hit_dist = tmin;

		////segp->seg_out.hit_dist = tmax;
		//BU_LIST_INSERT(&(seghead->l), &(segp->l));

		openvdb::tools::LevelSetRayIntersector<GridT, openvdb::tools::LinearSearchImpl<GridT, 0, RealT>, GridT::TreeType::RootNodeType::ChildNodeType::LEVEL, RayT> intersector(*h_grid);
		Vec3T rayEye(rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
		Vec3T rayDir(rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
		RayT wRay(rayEye, rayDir);
		// transform the ray to the grid's index-space.
		RayT iRay = wRay.worldToIndex(*h_grid);
		// intersect...
		float t0;
		if (intersector.intersectsIS(iRay, t0)) {
			// write distance to surface. (we assume it is a uniform voxel)
			float wT0 = t0 * float(h_grid->voxelSize()[0]);

			fprintf(stderr, "hit\n");

			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in.hit_dist = t0;

			//segp->seg_out.hit_dist = tmax;
			BU_LIST_INSERT(&(seghead->l), &(segp->l));

		}
		else {
			// did not hit
			//fprintf(stderr, "no hit\n");
			return 0;
		}


		return 2;

	}

	/**
	 * return a single normal
	 */
	void
		rt_vdb_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
	{

		vect_t xlated;
		fastf_t scale;

		VSET(hitp->hit_normal, 1, 0, 0);
		hitp->hit_vpriv[X] = 1.0;


	}

#ifdef __cplusplus
}
#endif