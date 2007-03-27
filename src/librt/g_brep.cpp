/*                     G _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @addtogroup g_  */
/** @{ */
/** @file g_brep.cpp
 *
 */

#include "brep.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "vector.h"

#ifdef write
#   undef write
#endif

#ifdef read
#   undef read
#endif

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

int
rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip);
void
rt_brep_print(register const struct soltab *stp);
int
rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
void
rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp);
void
rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp);
int
rt_brep_class();
void
rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp);
void
rt_brep_free(register struct soltab *stp);
int
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);
int
rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip);
void
rt_brep_ifree(struct rt_db_internal *ip);
int
rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
int
rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr);
int
rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv);

#ifdef __cplusplus
  }
#endif

/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/

#include <list>
#include <iostream>

#define TRACE(s) std::cout << "TRACE: " << s << std::endl

//--------------------------------------------------------------------------------
// Bounding volume classes
namespace brep {
    inline void swap(fastf_t& a, fastf_t& b) {
	fastf_t t = b;
	b = a;
	a = t;
    }
    
    class BoundingVolume {	
    public:

	BoundingVolume(const point_t min, const point_t max);
	BoundingVolume(const point_t mina, const point_t maxa, const point_t minb, const point_t maxb);
	BoundingVolume(const BoundingVolume& bv);
	virtual ~BoundingVolume();

	BoundingVolume& operator=(const BoundingVolume& bv);

	virtual bool is_leaf() const;

	fastf_t width() const;
	fastf_t height() const;
	fastf_t depth() const;
	void get_bbox(point_t min, point_t max) const;

	fastf_t surface_area() const;	
	fastf_t combined_surface_area(const BoundingVolume& vol) const;
	BoundingVolume combine(const BoundingVolume& vol) const;

	// slab intersection routine
	bool intersected_by(struct xray* r);
	bool intersected_by(struct xray* r, fastf_t* tnear, fastf_t* far);

	// Goldsmith & Salmon "Automatic generation of trees"
	BoundingVolume* gs_insert(BoundingVolume* node);
	
	std::list<BoundingVolume*> children;

    private:
	point_t _min;
	point_t _max;
	fastf_t _area;
    };

    class SurfaceBV : public BoundingVolume {
    public:

	SurfaceBV(const ON_BrepFace& face, point_t min, point_t max, const ON_Interval& u, const ON_Interval& v);
	bool is_leaf() const; // override BoundingVolume::is_leaf();
	const ON_BrepFace& face() const { return _face; }
	const fastf_t u_center() const { return _u.Mid(); }
	const fastf_t v_center() const { return _v.Mid(); }

    private:
	const ON_BrepFace& _face;
	ON_Interval _u;
	ON_Interval _v;
    }; 
    
    //--------------------------------------------------------------------------------
    // implementation
    
    inline
    BoundingVolume::BoundingVolume(const point_t min, const point_t max)
    {
	VMOVE(_min, min);
	VMOVE(_max, max);
	_area = 2*width()*height() + 2*width()*depth() + 2*height()*depth();
    }

    inline
    BoundingVolume::BoundingVolume(const point_t mina, const point_t maxa, const point_t minb, const point_t maxb)
    {
	VMOVE(_min,mina);
	VMOVE(_max,maxa);
	VMIN(_min,minb);
	VMAX(_max,maxb);
	_area = 2*width()*height() + 2*width()*depth() + 2*height()*depth();
    }

    inline BoundingVolume& 
    BoundingVolume::operator=(const BoundingVolume& bv) {
	VMOVE(_min,bv._min);
	VMOVE(_max,bv._max);
	_area = bv._area;
    }

    inline 
    BoundingVolume::~BoundingVolume() {
	for (std::list<BoundingVolume*>::iterator i = children.begin(); i != children.end(); i++) {
	    delete *i;
	}
    }

    inline bool
    BoundingVolume::is_leaf() const { return false; }

    inline fastf_t
    BoundingVolume::width() const { return _max[X]-_min[X]; }	
    inline fastf_t
    BoundingVolume::height() const { return _max[Y]-_min[Y]; }
    inline fastf_t
    BoundingVolume::depth() const { return _max[Z]-_min[Z]; }

    inline void 
    BoundingVolume::get_bbox(point_t min, point_t max) const {
	VMOVE(min, _min);
	VMOVE(max, _max);
    }
    
    inline bool 
    BoundingVolume::intersected_by(struct xray* r) {
	fastf_t tnear, tfar;
	return intersected_by(r, &tnear, &tfar);
    }

    inline bool 
    BoundingVolume::intersected_by(struct xray* r, fastf_t* tnear, fastf_t* tfar) {
	*tnear = -MAX_FASTF;
	*tfar = MAX_FASTF;
	for (int i = 0; i < 3; i++) {
	    if (NEAR_ZERO(r->r_dir[i],VUNITIZE_TOL)) {
		if (r->r_pt[i] < _min[i] || r->r_pt[i] > _max[i])
		    return false;
	    }
	    else {
		fastf_t t1 = (_min[i]-r->r_pt[i]) / r->r_dir[i];
		fastf_t t2 = (_max[i]-r->r_pt[i]) / r->r_dir[i];
		if (t1 > t2) swap(t1,t2);
		if (t1 > *tnear) *tnear = t1;
		if (t2 < *tfar) *tfar = t2;
		if (*tnear > *tfar) /* box is missed */ return false;
		if (*tfar < 0) /* box is behind ray */ return false;
	    }
	}
	return true;
    }

    inline fastf_t
    BoundingVolume::surface_area() const { return _area; }
    
    inline fastf_t
    BoundingVolume::combined_surface_area(const BoundingVolume& vol) const { 
	BoundingVolume combined(_min,_max,vol._min,vol._max);
	return combined.surface_area();
    }

    inline BoundingVolume
    BoundingVolume::combine(const BoundingVolume& vol) const {
	return BoundingVolume(_min,_max,vol._min,vol._max);
    }

    BoundingVolume*
    BoundingVolume::gs_insert(BoundingVolume* node)
    {
	// XXX todo - later!
	if (is_leaf()) {
	    // create a new parent 
	} else {

	}

        return this;
    }

    inline 
    SurfaceBV::SurfaceBV(const ON_BrepFace& face, point_t min, point_t max, const ON_Interval& u, const ON_Interval& v) 
	: BoundingVolume(min,max), _face(face), _u(u), _v(v)
    {
    }

    inline bool
    SurfaceBV::is_leaf() const { return true; }

    typedef std::list<BoundingVolume*> BVList;
};

using namespace brep;

//--------------------------------------------------------------------------------
// specific
struct brep_specific*
brep_specific_new()
{
    return (struct brep_specific*)bu_calloc(1, sizeof(struct brep_specific),"brep_specific_new");
}

void
brep_specific_delete(struct brep_specific* bs)
{
    if (bs != NULL) {
	delete bs->bvh;
	bu_free(bs,"brep_specific_delete");
    }
}

//--------------------------------------------------------------------------------
// prep

/**
 * Given a list of face bounding volumes for an entire brep, build up
 * an appropriate hierarchy to make it efficient (binary may be a
 * reasonable choice, for example).
 */
void
brep_bvh_subdivide(BoundingVolume* parent, const BVList& face_bvs)
{
    // XXX this needs to handle a threshold and some reasonable space
    // partitioning
//     for (BVList::const_iterator i = face_bvs.begin(); i != face_bvs.end(); i++) {
// 	parent->gs_insert(*i);
//     }
    for (BVList::const_iterator i = face_bvs.begin(); i != face_bvs.end(); i++) {
	parent->children.push_back(*i);
    }
}

inline void distribute(const int count, const ON_3dVector* v, double x[], double y[], double z[])
{
    for (int i = 0; i < count; i++) {
	x[i] = v[i].x;
	y[i] = v[i].y;
	z[i] = v[i].z;
    }
}

/**
 * Determine whether a given surface is flat enough, i.e. it falls
 * beneath our simple flatness constraints. The flatness constraint in
 * this case is a sampling of normals across the surface such that
 * the product of their combined dot products is close to 1.
 *
 * \Product_{i=1}^{7} n_i \dot n_{i+1} = 1
 *
 * Would be a perfectly flat surface. Generally something in the range
 * 0.8-0.9 should suffice (according to Abert, 2005).
 *
 *   	 +-------------------------+
 *	 |           	           |
 *	 |            +            |
 *	 |                         |
 *    V  |       +         +       |
 *	 |                         |
 *	 |            +            |
 *	 |                         |
 *	 +-------------------------+
 *                    U
 *                     
 * The "+" indicates the normal sample.
 */
bool
brep_is_flat(const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v)
{
    ON_3dVector normals[8];    

    bool fail = false;
    // corners    
    if (!surf->EvNormal(u.Min(),v.Min(),normals[0]) ||
	!surf->EvNormal(u.Max(),v.Min(),normals[1]) ||
	!surf->EvNormal(u.Max(),v.Max(),normals[2]) ||
	!surf->EvNormal(u.Min(),v.Max(),normals[3]) ||

	// interior
	!surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.25),normals[4]) ||
	!surf->EvNormal(u.ParameterAt(0.75),v.ParameterAt(0.5),normals[5]) ||
	!surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.75),normals[6]) ||
	!surf->EvNormal(u.ParameterAt(0.25),v.ParameterAt(0.5),normals[7])) {
	bu_bomb("Could not evaluate a normal on the surface"); // XXX fix this
    }

    double product = 1.0;

#ifdef DO_VECTOR    
    double ax[4] VEC_ALIGN;
    double ay[4] VEC_ALIGN;
    double az[4] VEC_ALIGN;

    double bx[4] VEC_ALIGN;
    double by[4] VEC_ALIGN;
    double bz[4] VEC_ALIGN;

    distribute(4, normals, ax, ay, az);
    distribute(4, &normals[1], bx, by, bz);
    
    // how to get the normals in here?
    {
	dvec<4> xa(ax);
	dvec<4> ya(ay);
	dvec<4> za(az);
	dvec<4> xb(bx);
	dvec<4> yb(by);
	dvec<4> zb(bz);
	dvec<4> dots = xa * xb + ya * yb + za * zb;
	product *= dots.foldr(1,dvec<4>::mul());
	if (product < 0.0) return false;
    }    
    // try the next set of normals
    {
	distribute(3, &normals[4], ax, ay, az);
	distribute(3, &normals[5], bx, by, bz);
	dvec<4> xa(ax);
	dvec<4> xb(bx);
	dvec<4> ya(ay); 
	dvec<4> yb(by);
	dvec<4> za(az);
	dvec<4> zb(bz);
	dvec<4> dots = xa * xb + ya * yb + za * zb;
	product *= dots.foldr(1,dvec<4>::mul(),3);
    }
#else
    for (int i = 0; i < 7; i++) {
	product *= (normals[i] * normals[i+1]);
    }
#endif

    return product >= BREP_SURFACE_FLATNESS;
}

BoundingVolume*
brep_surface_bbox(const ON_BrepFace& face, const ON_Interval& u, const ON_Interval& v)
{
    TRACE("brep_surface_bbox");
    ON_3dPoint corners[4];
    const ON_Surface* surf = face.SurfaceOf();

    TRACE("  surf: " << surf);
    if (!surf->EvPoint(u.Min(),v.Min(),corners[0]) ||
	!surf->EvPoint(u.Max(),v.Min(),corners[1]) ||
	!surf->EvPoint(u.Max(),v.Max(),corners[2]) ||
	!surf->EvPoint(u.Min(),v.Max(),corners[3])) {
	bu_bomb("Could not evaluate a point on surface"); // XXX fix this message
    }

    point_t min, max;
    VSETALL(min, MAX_FASTF);
    VSETALL(max, -MAX_FASTF);
    for (int i = 0; i < 4; i++) 
	VMINMAX(min,max,((double*)corners[i]));
    return new SurfaceBV(face,min,max,u,v);
}

BoundingVolume*
brep_surface_subdivide(const ON_BrepFace& face, const ON_Interval& u, const ON_Interval& v, int depth)
{
    TRACE("brep_surface_subdivide");
    const ON_Surface* surf = face.SurfaceOf();
    BoundingVolume* parent = brep_surface_bbox(face, u, v);
    if (brep_is_flat(surf, u, v) || depth >= BREP_MAX_FT_DEPTH) {
	return parent;
    } else {
	BoundingVolume* quads[4];
	ON_Interval first(0,0.5);
	ON_Interval second(0.5,1.0);
	quads[0] = brep_surface_subdivide(face, u.ParameterAt(first),  v.ParameterAt(first),  depth+1);
	quads[1] = brep_surface_subdivide(face, u.ParameterAt(second), v.ParameterAt(first),  depth+1);
	quads[2] = brep_surface_subdivide(face, u.ParameterAt(second), v.ParameterAt(second), depth+1);
	quads[3] = brep_surface_subdivide(face, u.ParameterAt(first),  v.ParameterAt(second), depth+1);

	for (int i = 0; i < 4; i++)
	    parent->children.push_back(quads[i]);

	return parent;
    }
}

int
brep_build_bvh(struct brep_specific* bs, struct rt_brep_internal* bi)
{
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
	return -1;
    }

    point_t min, max;
    brep->GetBBox(min, max);

    bs->bvh = new BoundingVolume(min,max);

    // need to extract faces, and build bounding boxes for each face,
    // then combine the face BBs back up, combining them together to
    // better split the hierarchy
    BVList surface_bvs;
    ON_BrepFaceArray& faces = brep->m_F;
    for (int i = 0; i < faces.Count(); i++) {
        TRACE("Face: " << i);
	ON_BrepFace& face = faces[i];
	const ON_Surface* surf = face.SurfaceOf();
	TRACE("Surf: " << surf);

	ON_Interval u = surf->Domain(0);
	ON_Interval v = surf->Domain(1);
	BoundingVolume* bv = brep_surface_subdivide(face, u, v, 0);

	// add the surface bounding volumes to a list, so we can build
	// down a hierarchy from the brep bounding volume
	surface_bvs.push_back(bv);
    }

    brep_bvh_subdivide(bs->bvh, surface_bvs);

    return 0;
}

void
brep_calculate_cdbitems(struct brep_specific* bs, struct rt_brep_internal* bi)
{

}


/********************************************************************************
 * BRL-CAD Primitive interface
 ********************************************************************************/
/**
 *   			R T _ B R E P _ P R E P
 *
 *  Given a pointer of a GED database record, and a transformation
 *  matrix, determine if this is a valid NURB, and if so, prepare the
 *  surface so the intersections will work.
 */
int
rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip)
{
    TRACE("rt_brep_prep");
    /* This prepares the NURBS specific data structures to be used
       during intersection... i.e. acceleration data structures and
       whatever else is needed.

       Abert's paper (Direct and Fast Ray Tracing of NURBS Surfaces)
       suggests using a bounding volume hierarchy (instead of KD-tree)
       and building it down to a satisfactory flatness criterion
       (which they do not give information about).
    */
    struct rt_brep_internal* bi;
    struct brep_specific* bs;

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    if ((bs = (struct brep_specific*)stp->st_specific) == NULL) {
	bs = (struct brep_specific*)bu_malloc(sizeof(struct brep_specific), "brep_specific");
	bs->brep = bi->brep;
	bi->brep = NULL;
	stp->st_specific = (genptr_t)bs;
    }

    if (brep_build_bvh(bs, bi) < 0) {
	return -1;
    }

    point_t adjust;
    VSETALL(adjust,0.02);
    bs->bvh->get_bbox(stp->st_min, stp->st_max);
    // expand outer bounding box...
    VSUB2(stp->st_min,stp->st_min,adjust);
    VADD2(stp->st_max,stp->st_max,adjust);
    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    vect_t work;
    VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);
    fastf_t f = fmax(work[X],work[Y]);
    f = fmax(f,work[Z]);
    stp->st_aradius = f;
    stp->st_bradius = MAGNITUDE(work);

    brep_calculate_cdbitems(bs, bi);

    return 0;
}


/**
 *			R T _ B R E P _ P R I N T
 */
void
rt_brep_print(register const struct soltab *stp)
{
}

//================================================================================
// shot support

class plane_ray {
public:
    vect_t n1;
    fastf_t d1;
    
    vect_t n2;
    fastf_t d2;
};

void brep_intersect_bv(BVList& inters, struct xray* r, BoundingVolume* bv) 
{
    bool intersects = bv->intersected_by(r);
    if (intersects && bv->is_leaf()) {	
	inters.push_back(bv);
    }
    else if (intersects)
	for (BVList::iterator i = bv->children.begin(); i != bv->children.end(); i++) {
	    brep_intersect_bv(inters, r, *i);
	}
}

void brep_get_plane_ray(struct xray* r, plane_ray& pr)
{
    vect_t v1;
    VMOVE(v1, r->r_dir);
    fastf_t min = MAX_FASTF;
    int index = -1;
    for (int i = 0; i < 3; i++) { // find the smallest component
	if (v1[i] < min) {
	    min = v1[i];
	    index = i;
	}
    }
    v1[index] += 1; // alter the smallest component
    VCROSS(pr.n1, v1, r->r_dir); // n1 is perpendicular to v1
    VCROSS(pr.n2, v1, pr.n1);       // n2 is perpendicular to v1 and n1
    // ??? do they need to be normalized - Ed doesn't think so...
    pr.d1 = VDOT(pr.n1,r->r_pt);
    pr.d2 = VDOT(pr.n2,r->r_pt);
}

#define ON_PRINT4(p) "[" << (p)[0] << "," << (p)[1] << "," << (p)[2] << "," << (p)[3] << "]"
#define ON_PRINT3(p) "(" << (p)[0] << "," << (p)[1] << "," << (p)[2] << ")"
#define ON_PRINT2(p) "(" << (p)[0] << "," << (p)[1] << ")"

// XXX put in VMATH?
typedef fastf_t pt2d_t[2] VEC_ALIGN;
typedef fastf_t mat2d_t[4] VEC_ALIGN; // row-major 
inline
bool mat2d_inverse(mat2d_t inv, mat2d_t m) {
    pt2d_t _a = {m[0],m[1]};
    pt2d_t _b = {m[3],m[2]};
    dvec<2> a(_a);
    dvec<2> b(_b);
    dvec<2> c = a*b;
    fastf_t det = c.foldr(0,dvec<2>::sub());
    if (NEAR_ZERO(det,VUNITIZE_TOL)) return false;
    fastf_t scale = 1.0 / det;
    double tmp[4] VEC_ALIGN = {m[3],-m[1],-m[2],m[0]};
    dvec<4> iv(tmp);
    dvec<4> sv(scale);
    dvec<4> r = iv * sv;
    r.a_store(inv);
    return true;
}
inline 
void mat2d_pt2d_mul(pt2d_t r, mat2d_t m, pt2d_t p) {
    pt2d_t _a = {m[0],m[2]};
    pt2d_t _b = {m[1],m[3]};
    dvec<2> x(p[0]);
    dvec<2> y(p[1]);
    dvec<2> a(_a);
    dvec<2> b(_b);
    dvec<2> c = a*x + b*y;
    c.a_store(r);
}
inline
void pt2dsub(pt2d_t r, pt2d_t a, pt2d_t b) {
    dvec<2> va(a);
    dvec<2> vb(b);
    dvec<2> vr = va - vb;
    vr.a_store(r);
}

inline
fastf_t v2mag(pt2d_t p) {
    dvec<2> a(p);
    dvec<2> sq = a*a;
    return sqrt(sq.foldr(0,dvec<2>::add()));
}
inline
fastf_t move(pt2d_t a, pt2d_t b) {
    a[0] = b[0];
    a[1] = b[1];
}


class brep_hit {
public:
    const ON_BrepFace& face;
    point_t point;
    vect_t  normal;
    pt2d_t  uv;
    
    brep_hit(const ON_BrepFace& f, point_t p, vect_t n, pt2d_t _uv) 
	: face(f)
    {
	VMOVE(point, p);
	VMOVE(normal, n);
	move(uv, _uv);
	TRACE("hit: " << ON_PRINT3(p) << " " << ON_PRINT3(n));
    }
};
typedef std::list<brep_hit*> HitList;

void
brep_newton_iterate(const ON_Surface* surf, plane_ray& pr, pt2d_t old_uv, pt2d_t new_uv, pt2d_t new_R) 
{
    TRACE("brep_newton_iterate: " << ON_PRINT2(old_uv));
    ON_3dPoint _pt;
    ON_3dVector _su, _sv;
    surf->Ev1Der(old_uv[0], old_uv[1], _pt, _su, _sv);
    TRACE("\tpt" << ON_PRINT3(_pt) << ", su" << ON_PRINT3(_su) << ", sv" ON_PRINT3(_sv));

    new_R[0] = VDOT(pr.n1,((fastf_t*)_pt)) + pr.d1;
    new_R[1] = VDOT(pr.n2,((fastf_t*)_pt)) + pr.d2;

    mat2d_t jacob = { VDOT(pr.n1,((fastf_t*)_su)), VDOT(pr.n1,((fastf_t*)_sv)),
		      VDOT(pr.n2,((fastf_t*)_su)), VDOT(pr.n2,((fastf_t*)_sv)) };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) { // check inverse validity
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, new_R);
	pt2dsub(new_uv, old_uv, tmp);
	fastf_t l,h;
	surf->GetDomain(0,&l,&h);
	if (new_uv[0] < l || new_uv[0] > h) move(new_uv,new_R);
	else {
	    surf->GetDomain(1,&l,&h);
	    if (new_uv[1] < l || new_uv[1] > h) move(new_uv,new_R);
	}
    }
    else {
	move(new_uv,new_R);
    }
    TRACE("\t" << ON_PRINT2(new_R) << ", " << ON_PRINT2(new_uv));
}


bool 
brep_intersect(const ON_BrepFace& face, const ON_Surface* surf, pt2d_t uv, plane_ray& pr, brep_hit** hit)
{
    bool found = false;
    fastf_t Dlast = MAX_FASTF;
    pt2d_t Rcurr;
    pt2d_t new_uv;
    for (int i = 0; i < BREP_MAX_ITERATIONS; i++) {
	brep_newton_iterate(surf, pr, uv, new_uv, Rcurr);
	fastf_t d = v2mag(Rcurr);
	if (d < BREP_INTERSECTION_ROOT_EPSILON) {
	    found = true; break; 
	} else if (d == Dlast) {
	    found = true; break;
	} else if (d > Dlast) {
	    break;
	}
	move(uv, new_uv);
    }
    if (found) {
	ON_3dPoint _pt;
	ON_3dVector _norm;
	surf->EvNormal(new_uv[0],new_uv[1],_pt,_norm);
	*hit = new brep_hit(face, (fastf_t*)_pt,(fastf_t*)_norm, new_uv);
    }    
    return found;
}

/**
 *  			R T _ B R E P _ S H O T
 *
 *  Intersect a ray with a brep.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    mat2d_t test = {2,3,4,5};
    mat2d_t inv;
    mat2d_inverse(inv, test);
    TRACE("inverse: " << ON_PRINT4(inv));

    TRACE("rt_brep_shot origin:" << ON_PRINT3(rp->r_pt) << " dir:" << ON_PRINT3(rp->r_dir));
    vect_t invdir;
    struct brep_specific* bs = (struct brep_specific*)stp->st_specific;

    // check the hierarchy to see if we have a hit at a leaf node
    BVList inters;
    brep_intersect_bv(inters, rp, bs->bvh);
    TRACE("found " << inters.size() << " intersections!");

    if (inters.size() == 0) return 0; // MISS

    plane_ray pr;
    brep_get_plane_ray(rp, pr);    

    HitList hits;
    for (BVList::iterator i = inters.begin(); i != inters.end(); i++) {
	const SurfaceBV* sbv = dynamic_cast<SurfaceBV*>((*i));
	const ON_BrepFace& f = sbv->face();
	const ON_Surface* surf = f.SurfaceOf();       
	brep_hit* hit; 
	pt2d_t uv = {sbv->u_center(),sbv->v_center()};
	if (brep_intersect(f, surf, uv, pr, &hit)) {
	    hits.push_back(hit);
	}
    }

    if (hits.size() > 0) {
	if (hits.size() % 2 == 0) {
	    // take each pair as a segment
	    for (HitList::iterator i = hits.begin(); i != hits.end(); i++) {
		brep_hit* in = *i;
		i++;
		brep_hit* out = *i;
		register struct seg* segp;
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;

		VMOVE(segp->seg_in.hit_point, in->point);
		VMOVE(segp->seg_in.hit_normal, in->normal);
		segp->seg_in.hit_dist = DIST_PT_PT(rp->r_pt,in->point);
		segp->seg_in.hit_surfno = in->face.m_face_index;
		VSET(segp->seg_in.hit_vpriv,in->uv[0],in->uv[1],0.0);

		VMOVE(segp->seg_out.hit_point, out->point);
		VMOVE(segp->seg_out.hit_normal, out->normal);
		segp->seg_out.hit_dist = DIST_PT_PT(rp->r_pt,out->point);
		segp->seg_out.hit_surfno = out->face.m_face_index;
		VSET(segp->seg_out.hit_vpriv,out->uv[0],out->uv[1],0.0);

		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	    }
	    return hits.size();
	} else {
	    bu_log("ACK! found odd number of intersection points. XXX Handle this!\n");
	    bu_log("It's really not acceptable to just print inane statements :-)\n");
	}
    }

    return 0; // MISS
}


/**
 *  			R T _ B R E P _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    
}


/**
 *			R T _ B R E P _ C U R V E
 *
 *  Return the curvature of the nurb.
 */
void
rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    /* XXX todo */
}

/**
 *                      R T _ B R E P _ C L A S S
 *
 *  Don't know what this is supposed to do...
 *
 *  Looking at g_arb.c, seems the actual signature is:
 *    class(const struct soltab* stp,
 *          const fastf_t* min,
 *          const fastf_t* max,
 *          const struct bn_tol* tol)
 *
 *  Hmmm...
 */
int
rt_brep_class()
{
  return RT_CLASSIFY_UNIMPLEMENTED;
}


/**
 *  			R T _ B R E P _ U V
 *
 *  For a hit on the surface of an nurb, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    uvp->uv_u = hitp->hit_vpriv[0];
    uvp->uv_v = hitp->hit_vpriv[1];
}


/**
 *		R T _ B R E P _ F R E E
 */
void
rt_brep_free(register struct soltab *stp)
{
    TRACE("rt_brep_free");
    struct brep_specific* bs = (struct brep_specific*)stp->st_specific;
    brep_specific_delete(bs);
}


/**
 *			R T _ B R E P _ P L O T
 */
int
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    TRACE("rt_brep_plot");
    struct rt_brep_internal* bi;

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    // XXX currently not handling the tolerance
    ON_MeshParameters mp;
    mp.JaggedAndFasterMeshParameters();

    ON_SimpleArray<ON_Mesh*> mesh_list;
    bi->brep->CreateMesh(mp, mesh_list);

    point_t pt1, pt2;
    ON_SimpleArray<ON_2dex> edges;
    for (int i = 0; i < mesh_list.Count(); i++) {
	const ON_Mesh* mesh = mesh_list[i];
	mesh->GetMeshEdges(edges);
	for (int j = 0; j < edges.Count(); j++) {
	    ON_MeshVertexRef v1 = mesh->VertexRef(edges[j].i);
	    ON_MeshVertexRef v2 = mesh->VertexRef(edges[j].j);
	    VSET(pt1, v1.Point().x, v1.Point().y, v1.Point().z);
	    VSET(pt2, v2.Point().x, v2.Point().y, v2.Point().z);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
	edges.Empty();
    }

    return 0;
}


/**
 *			R T _ B R E P _ T E S S
 */
int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    return -1;
}


/* XXX In order to facilitate exporting the ON_Brep object without a
 * whole lot of effort, we're going to (for now) extend the
 * ON_BinaryArchive to support an "in-memory" representation of a
 * binary archive. Currently, the openNURBS library only supports
 * file-based archiving operations. This implies the
 */

class ON_CLASS RT_MemoryArchive : public ON_BinaryArchive
{
public:
  RT_MemoryArchive();
  RT_MemoryArchive(genptr_t memory, size_t len);
  virtual ~RT_MemoryArchive();

  // ON_BinaryArchive overrides
  size_t CurrentPosition() const;
  bool SeekFromCurrentPosition(int);
  bool SeekFromStart(size_t);
  bool AtEnd() const;

  size_t Size() const;
  /**
   * Generate a byte-array copy of this memory archive.
   * Allocates memory using bu_malloc, so must be freed with bu_free
   */
  genptr_t CreateCopy() const;

protected:
  size_t Read(size_t, void*);
  size_t Write(size_t, const void*);
  bool Flush();

private:
  size_t pos;
  std::vector<char> m_buffer;
};

RT_MemoryArchive::RT_MemoryArchive()
  : ON_BinaryArchive(ON::write3dm), pos(0)
{
}

RT_MemoryArchive::RT_MemoryArchive(genptr_t memory, size_t len)
  : ON_BinaryArchive(ON::read3dm), pos(0)
{
  m_buffer.reserve(len);
  for (int i = 0; i < len; i++) {
    m_buffer.push_back(((char*)memory)[i]);
  }
}

RT_MemoryArchive::~RT_MemoryArchive()
{
}

size_t
RT_MemoryArchive::CurrentPosition() const
{
  return pos;
}

bool
RT_MemoryArchive::SeekFromCurrentPosition(int seek_to)
{
  if (pos + seek_to > m_buffer.size()) return false;
  pos += seek_to;
  return true;
}

bool
RT_MemoryArchive::SeekFromStart(size_t seek_to)
{
  if (seek_to > m_buffer.size()) return false;
  pos = seek_to;
  return true;
}

bool
RT_MemoryArchive::AtEnd() const
{
  return pos >= m_buffer.size();
}

size_t
RT_MemoryArchive::Size() const
{
  return m_buffer.size();
}

genptr_t
RT_MemoryArchive::CreateCopy() const
{
  genptr_t memory = (genptr_t)bu_malloc(m_buffer.size()*sizeof(char),"rt_memoryarchive createcopy");
  const int size = m_buffer.size();
  for (int i = 0; i < size; i++) {
    ((char*)memory)[i] = m_buffer[i];
  }
  return memory;
}

size_t
RT_MemoryArchive::Read(size_t amount, void* buf)
{
    const int read_amount = (pos + amount > m_buffer.size()) ? m_buffer.size()-pos : amount;
    const int start = pos;
    for (; pos < (start+read_amount); pos++) {
	((char*)buf)[pos-start] = m_buffer[pos];
    }
    return read_amount;
}

size_t
RT_MemoryArchive::Write(const size_t amount, const void* buf)
{
    // the write can come in at any position!
    const int start = pos;
    // resize if needed to support new data
    if (m_buffer.size() < (start+amount)) {
	m_buffer.resize(start+amount);
    }
    for (; pos < (start+amount); pos++) {
	m_buffer[pos] = ((char*)buf)[pos-start];
    }
    return amount;
}

bool
RT_MemoryArchive::Flush()
{
  return true;
}

#include <iostream>
/**
 *			R T _ B R E P _ E X P O R T 5
 */
int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    TRACE("rt_brep_export5");
    struct rt_brep_internal* bi;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP) return -1;
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    BU_INIT_EXTERNAL(ep);

    RT_MemoryArchive archive;
    /* XXX what to do about the version */
    ONX_Model model;

    {
	ON_Layer default_layer;
	default_layer.SetLayerIndex(0);
	default_layer.SetLayerName("Default");
	model.m_layer_table.Reserve(1);
	model.m_layer_table.Append(default_layer);
    }

    ONX_Model_Object& mo = model.m_object_table.AppendNew();
    mo.m_object = bi->brep;
    mo.m_attributes.m_layer_index = 0;
    mo.m_attributes.m_name = "brep";
    fprintf(stderr, "m_object_table count: %d\n", model.m_object_table.Count());

    model.m_properties.m_RevisionHistory.NewRevision();
    model.m_properties.m_Application.m_application_name = "BRL-CAD B-Rep primitive";

    model.Polish();
    ON_TextLog err(stderr);
    bool ok = model.Write(archive, 4, "export5", &err);
    if (ok) {
	ep->ext_nbytes = archive.Size();
	ep->ext_buf = archive.CreateCopy();
	return 0;
    } else {
	return -1;
    }
}


/**
 *			R T _ B R E P _ I M P O R T 5
 */
int
rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    ON::Begin();
    TRACE("rt_brep_import5");
    struct rt_brep_internal* bi;
    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BREP;
    ip->idb_meth = &rt_functab[ID_BREP];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_brep_internal), "rt_brep_internal");
    
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    
    RT_MemoryArchive archive(ep->ext_buf, ep->ext_nbytes);
    ONX_Model model;
    ON_TextLog dump(stdout);
    //archive.Dump3dmChunk(dump);
    model.Read(archive, &dump);
    
    if (model.IsValid(&dump)) {
	ONX_Model_Object mo = model.m_object_table[0];
	// XXX does openNURBS force us to copy? it seems the answer is
	// YES due to the const-ness
	bi->brep = new ON_Brep(*ON_Brep::Cast(mo.m_object));
	return 0;
    } else {
	return -1;
    }
}


/**
 *			R T _ B R E P _ I F R E E
 */
void
rt_brep_ifree(struct rt_db_internal *ip)
{
    TRACE("rt_brep_ifree");
    struct rt_brep_internal* bi;
    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    if (bi->brep != NULL)
	delete bi->brep;
    bu_free(bi, "rt_brep_internal free");
    ip->idb_ptr = GENPTR_NULL;
}


/**
 *			R T _ B R E P _ D E S C R I B E
 */
int
rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    return 0;
}

/**
 *                      R T _ B R E P _ T C L G E T
 */
int
rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
    return 0;
}


/**
 *                      R T _ B R E P _ T C L A D J U S T
 */
int
rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
    return 0;
}

/** @} */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
