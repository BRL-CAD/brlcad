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

	// Initially with a bounding box, later with vdb
	struct vdb_specific {
		vect_t minBB;
		vect_t maxBB;
		/*fastf_t *a;

		openvdb::FloatGrid* grid;*/

		struct rt_vdb_internal vdb_i;
		nanovdb::NanoGrid <float>* h_grid;
		nanovdb::GridHandle<nanovdb::HostBuffer> handler;
		openvdb::GridBase::Ptr srcGrid;
		openvdb::GridBase::Ptr srcGridAux;
		vect_t hitNormal;
		vect_t hitPoint;
	};

	const struct bu_structparse rt_vdb_parse[] = {
	{"%s", RT_VDB_NAME_LEN, "file", bu_offsetof(struct rt_vdb_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
	{"%s", RT_VDB_NAME_LEN, "name", bu_offsetof(struct rt_vdb_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
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

	int
	rt_vdb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
	{
		point_t *pts;
		struct rt_vdb_internal *aip;
		struct bu_list *vlfree = &RTG.rtg_vlfree;

		BU_CK_LIST_HEAD(vhead);
		RT_CK_DB_INTERNAL(ip);
		/*aip = (struct rt_arb_internal *)ip->idb_ptr;
		RT_ARB_CK_MAGIC(aip);*/

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
		bu_log("step import \n");
		struct rt_vdb_internal *eip;
		struct bu_vls str = BU_VLS_INIT_ZERO;

		/* must be double for import and export */
		//double vec[ELEMENTS_PER_VECT * 2]; // min and max = 2

		if (dbip) RT_CK_DBI(dbip);
		RT_CK_DB_INTERNAL(ip);
		BU_CK_EXTERNAL(ep);

		//BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * 2); // min and max = 2

		ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
		ip->idb_type = ID_VDB;
		ip->idb_meth = &OBJ[ID_VDB];
		BU_ALLOC(ip->idb_ptr, struct rt_vdb_internal);

		eip = (struct rt_vdb_internal *)ip->idb_ptr;
		eip->magic = RT_VDB_INTERNAL_MAGIC;

		bu_vls_strncpy(&str, (const char *)ep->ext_buf, ep->ext_nbytes);
		/*char message1[200] = "";
		strcat(message1, str.vls_str);
		strcat(message1, " test str \n");
		bu_log(message1);*/

		if (bu_struct_parse(&str, rt_vdb_parse, (char *)eip, NULL) < 0) {
			bu_vls_free(&str);
			return -2;
		}
		bu_vls_free(&str);

		/* Convert from database (network) to internal (host) format */
		//bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, ELEMENTS_PER_VECT * 2); // min and max = 2

		/* Apply modeling transformations */
		mat = bn_mat_identity;
		//MAT4X3PNT(eip->minBB, mat, &vec[0 * ELEMENTS_PER_VECT]);
		//MAT4X3VEC(eip->maxBB, mat, &vec[1 * ELEMENTS_PER_VECT]);

		//bu_log("min eip:  %g %g  %g \n", eip->minBB[0], eip->minBB[1], eip->minBB[2]);
		//bu_log("max eip:  %g %g  %g \n", eip->maxBB[0], eip->maxBB[1], eip->maxBB[2]);

		//bu_log("min vec:  %g %g  %g \n", vec[0], vec[1], vec[2]);
		//bu_log("max vec:  %g %g  %g \n", vec[1], vec[4], vec[5]);

		//eip->vdb = &nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(100.0f, nanovdb::Vec3f(0, 0, 0), 1.0, 3.0, nanovdb::Vec3d(0), "sphere");
		//nanovdb::GridHandle<nanovdb::HostBuffer> vdbHandler = *((nanovdb::GridHandle<nanovdb::HostBuffer> *)eip->vdb);
		////eip->vdbGrid = &nanovdb::nanoToOpenVDB(*vdbHandler);
		////auto grid = nanovdb::nanoToOpenVDB(*vdbHandler);
		////eip->vdbGrid = grid.get();

		//nanovdb::GridHandle<nanovdb::HostBuffer> vdbHandle;
		//vdbHandle = nanovdb::createLevelSetSphere<float, float, nanovdb::HostBuffer>(100.0f, nanovdb::Vec3f(0, 0, 0), 1.0, 3.0, nanovdb::Vec3d(0), "sphere");
		//vdbHandle = nanovdb::createLevelSetTorus<float, float, nanovdb::HostBuffer>(100.0f, 50.0f, nanovdb::Vec3f(0, 0, 0), 1.0, 3.0, nanovdb::Vec3d(0));
		//bu_log("min vec1:  %g %g  %g \n", vec[0], vec[1], vec[2]);

		openvdb::initialize();
		// Create a VDB file object.
		//bu_log("min vec2:  %g %g  %g \n", vec[0], vec[1], vec[2]);

		//openvdb::io::File file("armadillo2.vdb");

		char message[200] = "";
		strcat(message, eip->name);
		strcat(message, " loading \n");
		bu_log(message);

		openvdb::io::File file(eip->name);
		//bu_log("min vec3:  %g %g  %g \n", vec[0], vec[1], vec[2]);

		// Open the file.  This reads the file header, but not any grids.

		file.open();
		//bu_log("min vec4:  %g %g  %g \n", vec[0], vec[1], vec[2]);

		openvdb::GridBase::Ptr baseGrid = file.getGrids()->at(0);
		const int length2 = baseGrid->getName().length();

		// declaring character array (+1 for null terminator)
		char* char_array2 = new char[length2 + 1];

		// copying the contents of the
		// string to char array
		strcpy(char_array2, baseGrid->getName().c_str());
		bu_log(char_array2);

		// Loop over all grids in the file and retrieve a shared pointer
		// to the one named "density".  (This can also be done
		// more simply by calling file.readGrid("density").)
		//openvdb::GridBase::Ptr baseGrid;
		//for (openvdb::io::File::NameIterator nameIter = file.beginName();
		//	nameIter != file.endName(); ++nameIter)
		//{
		//	// Read in only the grid we are interested in.
		//	if (nameIter.gridName() == "ls_cube")
		//	{
		//		baseGrid = file.readGrid(nameIter.gridName());
		//		const int length = nameIter.gridName().length();

		//		// declaring character array (+1 for null terminator)
		//		char* char_array = new char[length + 1];

		//		// copying the contents of the
		//		// string to char array
		//		strcpy(char_array, nameIter.gridName().c_str());
		//		bu_log(char_array);
		//		delete[] char_array;
		//	}
		//	else
		//	{
		//		std::cout << "skipping grid " << nameIter.gridName() << std::endl;
		//		const int length = nameIter.gridName().length();

		//		// declaring character array (+1 for null terminator)
		//		char* char_array = new char[length + 1];

		//		// copying the contents of the
		//		// string to char array
		//		strcpy(char_array, nameIter.gridName().c_str());
		//		bu_log(char_array);
		//		delete[] char_array;
		//	}
		//}
		//file.close();
		//bu_log("min vec5:  %g %g  %g \n", vec[0], vec[1], vec[2]);

		// From the example above, "density" is known to be a FloatGrid,
		// so cast the generic grid pointer to a FloatGrid pointer.
		openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

		// Convert the OpenVDB grid into a NanoVDB grid handle.
		auto handle = nanovdb::openToNanoVDB(*grid);

		vdbHandle = nanovdb::openToNanoVDB(*grid);//nanovdb::io::readGrid< nanovdb::HostBuffer>("test.nvdb");

		std::string name = vdbHandle.gridMetaData()->shortGridName();
		const int length = name.length();

		// declaring character array (+1 for null terminator)
		char* char_array = new char[length + 1];

		// copying the contents of the
		// string to char array
		strcpy(char_array, name.c_str());
		bu_log(char_array);
		delete[] char_array;
		eip->vdb = &vdbHandle;


		return 0;		/* OK */
	}

	/**
	 * The external format is: min vector and max vector
	 * later it will be a vdb
	 */
	int rt_vdb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
	{
		
		struct rt_vdb_internal *vip;
		struct rt_vdb_internal vdb;	/* scaled version */
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
	 * Compute the bounding
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
		vdb->srcGrid = nanovdb::nanoToOpenVDB(*vdbHandler);
		vdb->srcGridAux = nanovdb::nanoToOpenVDB(*vdbHandler);
		nanovdb::NanoGrid <float>* h_grid = vdbHandler->grid<float>();
		
		vect_t minBoundingBox;
		VSET(minBoundingBox, h_grid->worldBBox().min()[0], h_grid->worldBBox().min()[1], h_grid->worldBBox().min()[2]);

		vect_t maxBoundingBox;
		VSET(maxBoundingBox, h_grid->worldBBox().max()[0], h_grid->worldBBox().max()[1], h_grid->worldBBox().max()[2]);

		VMINMAX(stp->st_min, stp->st_max, minBoundingBox);
		VMINMAX(stp->st_min, stp->st_max, maxBoundingBox);

		bu_log("min vec:  %g %g %g \n", stp->st_min[0], stp->st_min[1], stp->st_min[2]);
		bu_log("max vec:  %g %g %g \n", stp->st_max[0], stp->st_max[1], stp->st_max[2]);

		bu_log("min vec:  %g %g %g \n", h_grid->worldBBox().min()[0], h_grid->worldBBox().min()[1], h_grid->worldBBox().min()[2]);
		bu_log("max vec:  %g %g %g \n", h_grid->worldBBox().max()[0], h_grid->worldBBox().max()[1], h_grid->worldBBox().max()[2]);

		
		vect_t diam;
		vect_t radvec;
		VSUB2(diam, stp->st_max, stp->st_min);
		VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
		VSCALE(radvec, diam, 0.5);
		stp->st_aradius = stp->st_bradius = MAGNITUDE(radvec);

		vdb->vdb_i = *eip2;		/* struct copy */
		vdb->h_grid = h_grid;		/* struct copy */
		
		return 0;			/* OK */
	}


	/**
	 * Intersect a ray with an the bounding box - later it will be with vdb
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

		//fprintf(stderr, "shot\n");
		struct vdb_specific *vdb =
			(struct vdb_specific *)stp->st_specific;
		struct seg *segp;

		auto srcGrid = vdb->srcGrid;//nanovdb::nanoToOpenVDB(vdb->handler);
		openvdb::FloatGrid* h_grid = (openvdb::FloatGrid *)srcGrid.get();

		auto  indexBBox = h_grid->evalActiveVoxelBoundingBox();
		auto  gridXform = h_grid->transformPtr();
		auto  worldBBox = gridXform->indexToWorld(indexBBox);
		float wBBoxDimZ = (float)worldBBox.extents()[2] * 2;
		//bu_log("min vec:  %g \n", wBBoxDimZ);

		openvdb::tools::LevelSetRayIntersector<GridT, openvdb::tools::LinearSearchImpl<GridT, 0, RealT>, GridT::TreeType::RootNodeType::ChildNodeType::LEVEL, RayT> intersector(*h_grid);
		Vec3T rayEye(rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
		Vec3T rayDir(rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
		rayDir.normalize();
		RayT wRay(rayEye, rayDir);
		// transform the ray to the grid's index-space.
		RayT iRay = wRay.worldToIndex(*h_grid);
		// intersect...
		float tIn;
		Vec3T normal(0, 0, 0);
		Vec3T worldHit(0, 0, 0);
		//if (intersector.intersectsIS(iRay, t0)) {
		auto srcGridAux = vdb->srcGridAux;//nanovdb::nanoToOpenVDB(vdb->handler);
		openvdb::FloatGrid* h_grid2 = (openvdb::FloatGrid *)srcGridAux.get();
		
		if (intersector.intersectsWS(wRay, worldHit, normal, tIn)) {
			
			VSET(vdb->hitNormal, normal.x(), normal.y(), normal.z());
			VSET(vdb->hitPoint, worldHit.x(), worldHit.y(), worldHit.z());
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in.hit_dist = tIn;
			
			////shoot secondary ray to find t_out
			float offset = 0.001; // offset to avoid self intersection
			Vec3T rayOutPosition = worldHit + (offset*rayDir);
			RayT outRay(rayOutPosition, rayDir);
			Vec3T normal2(0, 0, 0);
			Vec3T worldHit2(0, 0, 0);
			float tOut;
			
			if (intersector.intersectsWS(outRay, worldHit2, normal2, tOut)) {
				float wTOut = tOut * float(h_grid->voxelSize()[0]);
				segp->seg_out.hit_dist = tOut + tIn;
				
			}
			else {
				segp->seg_out.hit_dist = tIn;
				
			}
			
			//segp->seg_out.hit_dist = tmax;
			BU_LIST_INSERT(&(seghead->l), &(segp->l));

		}
		else { // did not hit
			return 0;
		}

		return 2;

	}

	/**
	 * return a single normal
	 */
	void
		rt_vdb_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
	{

		struct vdb_specific *vdb =
			(struct vdb_specific *)stp->st_specific;

		vect_t xlated;
		fastf_t scale;

		VSET(hitp->hit_normal, vdb->hitNormal[0], vdb->hitNormal[1], vdb->hitNormal[2]);
		VSET(hitp->hit_point, vdb->hitPoint[0], vdb->hitPoint[1], vdb->hitPoint[2]);



	}

#ifdef __cplusplus
}
#endif