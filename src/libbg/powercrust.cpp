/*                  P O W E R C R U S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file powercrust.cpp
 *
 * Classical-style Power Crust reconstruction with:
 *  - Convex hull prefilter (bg_3d_chull)
 *  - Local normal estimation (PCA over Delaunay neighbors)
 *  - Pole classification (inner vs outer)
 *  - Face selection using inner/outer pole separation across adjacent weighted tets
 *  - Circumradius vs local feature size filtering
 */

#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"

#include "bg/pca.h"
#include "bg/chull.h"

#include "geogram/basic/common.h"
#include "geogram/basic/geometry.h"
#include "geogram/delaunay/delaunay.h"
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/mesh/mesh_repair.h"
#include "geogram/mesh/mesh_fill_holes.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cfloat>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

struct bg_3d_powercrust_opts {
    double min_component_area_ratio = 0.02;
    double max_hole_area = 1e30;
    double pole_opposite_cos_threshold = -0.2;
    double degenerate_tet_volume_eps = 1e-14;
    int    normal_k_min = 8;
    double lfs_radius_ratio_max = 2.0;
    double feature_eps = 1e-9;
    bool   orient_faces = true;
    bool   verbose = false;
};

using P3 = std::array<fastf_t, 3>;

enum PoleClass : unsigned char {
    POLE_UNSET = 0,
    POLE_INNER = 1,
    POLE_OUTER = 2
};

/* Suppress Geogram initialization console output */
static void suppress_geogram_init()
{
    int serr=-1,sout=-1,stderr_stashed=-1,stdout_stashed=-1,fnull=-1;
#if defined(_WIN32)
    fnull = open("nul", O_WRONLY);
#else
    fnull = open("/dev/null", O_WRONLY);
#endif
    if (fnull != -1) {
	serr = fileno(stderr);
	sout = fileno(stdout);
	stderr_stashed = dup(serr);
	stdout_stashed = dup(sout);
	dup2(fnull, serr);
	dup2(fnull, sout);
	close(fnull);
    }
    GEO::initialize();
    GEO::Logger::instance()->unregister_all_clients();
    if (fnull != -1) {
	fflush(stderr);
	dup2(stderr_stashed, serr);
	close(stderr_stashed);
	fflush(stdout);
	dup2(stdout_stashed, sout);
	close(stdout_stashed);
    }
}

/* Helpers */
static inline void sub3(const double *a, const double *b, double *r)
{
    r[0]=a[0]-b[0];
    r[1]=a[1]-b[1];
    r[2]=a[2]-b[2];
}
static inline double dot3(const double *a, const double *b)
{
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static inline double len3(const double *a)
{
    return sqrt(dot3(a,a));
}
static inline void cross3(const double *a,const double *b,double*r)
{
    r[0]=a[1]*b[2]-a[2]*b[1];
    r[1]=a[2]*b[0]-a[0]*b[2];
    r[2]=a[0]*b[1]-a[1]*b[0];
}

static bool tet_circumcenter(
    const double *p0,const double *p1,const double *p2,const double *p3,
    double cc[3], double volume_eps)
{
    GEO::vec3 v0(p0[0],p0[1],p0[2]);
    GEO::vec3 v1(p1[0],p1[1],p1[2]);
    GEO::vec3 v2(p2[0],p2[1],p2[2]);
    GEO::vec3 v3(p3[0],p3[1],p3[2]);
    double vol = GEO::Geom::tetra_volume(v0,v1,v2,v3);
    if (vol < volume_eps) return false;
    GEO::vec3 c = GEO::Geom::tetra_circum_center(v0,v1,v2,v3);
    cc[0]=c.x;
    cc[1]=c.y;
    cc[2]=c.z;
    return true;
}

static void incident_tets(GEO::Delaunay *del, GEO::index_t v, std::vector<GEO::index_t>& out)
{
    out.clear();
    GEO::index_t c = del->vertex_cell(v);
    if (c == GEO::NO_INDEX) return;
    GEO::index_t start = c;
    bool first=true;
    do {
	if (del->cell_is_finite(c)) out.push_back(c);
	GEO::index_t lv=0;
	for (; lv<del->cell_size(); ++lv)
	    if (del->cell_vertex(c, lv)==v) break;
	c = del->next_around_vertex(c, lv);
	if (!first && c==start) break;
	first=false;
    } while (c != GEO::NO_INDEX);
}

/* Raw poles */
static void find_raw_poles(
    GEO::Delaunay *del,
    const point_t *samples, int n,
    std::vector<P3>& poleA,
    std::vector<P3>& poleB,
    double opposite_cos_threshold,
    double vol_eps)
{
    poleA.assign(n, P3{0,0,0});
    poleB.assign(n, P3{0,0,0});
    std::vector<GEO::index_t> tets;
    for (int i=0; i<n; ++i) {
	const double *pi = samples[i];
	incident_tets(del, (GEO::index_t)i,tets);
	struct CC {
	    double c[3];
	    double d2;
	};
	std::vector<CC> centers;
	centers.reserve(tets.size());
	for (auto t : tets) {
	    double cc[3];
	    double p[4][3];
	    for (GEO::index_t lv=0; lv<4; ++lv) {
		GEO::index_t gv = del->cell_vertex(t, lv);
		const double *vp = del->vertex_ptr(gv);
		p[lv][0]=vp[0];
		p[lv][1]=vp[1];
		p[lv][2]=vp[2];
	    }
	    if (!tet_circumcenter(p[0],p[1],p[2],p[3],cc,vol_eps))
		continue;
	    double dx=cc[0]-pi[0], dy=cc[1]-pi[1], dz=cc[2]-pi[2];
	    double d2=dx*dx+dy*dy+dz*dz;
	    if (std::isfinite(d2))
		centers.push_back({{cc[0],cc[1],cc[2]}, d2});
	}
	if (centers.empty()) {
	    poleA[i] = {pi[0],pi[1],pi[2]};
	    poleB[i] = poleA[i];
	    continue;
	}
	std::sort(centers.begin(), centers.end(), [](const CC&a,const CC&b) {
	    return a.d2>b.d2;
	});
	poleA[i] = { (fastf_t)centers[0].c[0], (fastf_t)centers[0].c[1], (fastf_t)centers[0].c[2] };
	double dir1[3] = { centers[0].c[0]-pi[0], centers[0].c[1]-pi[1], centers[0].c[2]-pi[2] };
	double n1 = len3(dir1);
	if (n1<1e-30) n1=1.0;
	bool found=false;
	for (size_t k=1; k<centers.size(); ++k) {
	    double dir2[3] = { centers[k].c[0]-pi[0], centers[k].c[1]-pi[1], centers[k].c[2]-pi[2] };
	    double n2=len3(dir2);
	    if (n2<1e-30) continue;
	    double cosang = dot3(dir1,dir2)/(n1*n2);
	    if (cosang < opposite_cos_threshold) {
		poleB[i] = { (fastf_t)centers[k].c[0],
			     (fastf_t)centers[k].c[1],
			     (fastf_t)centers[k].c[2]
			   };
		found=true;
		break;
	    }
	}
	if (!found) {
	    if (centers.size()>1)
		poleB[i] = { (fastf_t)centers[1].c[0],
			     (fastf_t)centers[1].c[1],
			     (fastf_t)centers[1].c[2]
			   };
	    else
		poleB[i] = poleA[i];
	}
    }
}

/* Convex hull prefilter */
static void convex_hull_prefilter(
    const point_t *pts, int n,
    std::vector<char>& is_hull_vertex,
    point_t &hull_centroid)
{
    is_hull_vertex.assign(n,0);
    int *faces=nullptr, num_faces=0;
    point_t *hverts=nullptr;
    int num_hv=0;
    (void)bg_3d_chull(&faces, &num_faces, &hverts, &num_hv, pts, n);
    if (num_hv>0) {
	double cx=0,cy=0,cz=0;
	for (int i=0; i<num_hv; ++i) {
	    int best=-1;
	    double best_d2=std::numeric_limits<double>::max();
	    for (int j=0; j<n; ++j) {
		double d2 = DIST_PNT_PNT_SQ(hverts[i], pts[j]);
		if (d2 < best_d2) {
		    best_d2=d2;
		    best=j;
		}
	    }
	    if (best>=0) {
		is_hull_vertex[best]=1;
		cx+=hverts[i][0];
		cy+=hverts[i][1];
		cz+=hverts[i][2];
	    }
	}
	cx/=num_hv;
	cy/=num_hv;
	cz/=num_hv;
	VSET(hull_centroid, cx,cy,cz);
    } else {
	VSET(hull_centroid,0,0,0);
    }
    if (faces) bu_free(faces,"hfaces");
    if (hverts) bu_free(hverts,"hverts");
}

/* Estimate normals: store in std::vector<P3> */
static void estimate_normals(
    GEO::Delaunay *del,
    const point_t *samples,
    int n_samples,
    const std::vector<char>& is_hull_vertex,
    const point_t hull_centroid,
    std::vector<P3>& normals,
    int k_min)
{
    normals.assign(n_samples, P3{0,0,1});
    GEO::vector<GEO::index_t> neigh;
    for (int i=0; i<n_samples; ++i) {
	neigh.clear();
	del->get_neighbors((GEO::index_t)i, neigh);
	std::vector<P3> local;
	local.reserve(neigh.size()+1);
	local.push_back({samples[i][0],samples[i][1],samples[i][2]});
	for (auto v : neigh) {
	    if ((int)v==i) continue;
	    local.push_back({samples[v][0],samples[v][1],samples[v][2]});
	}
	if ((int)local.size() < std::max(k_min,3)) {
	    // fallback orientation using hull direction if on hull
	    if (is_hull_vertex[i]) {
		double hc_dir[3] = { samples[i][0]-hull_centroid[0],
				     samples[i][1]-hull_centroid[1],
				     samples[i][2]-hull_centroid[2]
				   };
		double L = len3(hc_dir);
		if (L>1e-30) {
		    hc_dir[0]/=L;
		    hc_dir[1]/=L;
		    hc_dir[2]/=L;
		}
		normals[i] = { (fastf_t)hc_dir[0], (fastf_t)hc_dir[1], (fastf_t)hc_dir[2] };
	    }
	    continue;
	}
	// Build temporary contiguous point_t array for PCA
	point_t *tmp = (point_t*)bu_calloc(local.size(), sizeof(point_t), "pca local");
	for (size_t k=0; k<local.size(); ++k) {
	    tmp[k][0]=local[k][0];
	    tmp[k][1]=local[k][1];
	    tmp[k][2]=local[k][2];
	}
	point_t center;
	vect_t xaxis,yaxis,zaxis;
	if (bg_pca(&center, &xaxis, &yaxis, &zaxis, local.size(), tmp) != BRLCAD_OK) {
	    bu_free(tmp,"pca local");
	    continue;
	}
	bu_free(tmp,"pca local");
	// zaxis treated as normal
	double nrm[3] = { zaxis[0], zaxis[1], zaxis[2] };
	double L = len3(nrm);
	if (L>1e-30) {
	    nrm[0]/=L;
	    nrm[1]/=L;
	    nrm[2]/=L;
	}
	if (is_hull_vertex[i]) {
	    double hc_dir[3] = { samples[i][0]-hull_centroid[0],
				 samples[i][1]-hull_centroid[1],
				 samples[i][2]-hull_centroid[2]
			       };
	    double LH=len3(hc_dir);
	    if (LH>1e-30) {
		hc_dir[0]/=LH;
		hc_dir[1]/=LH;
		hc_dir[2]/=LH;
	    }
	    if (dot3(nrm,hc_dir) < 0.0) {
		nrm[0]=-nrm[0];
		nrm[1]=-nrm[1];
		nrm[2]=-nrm[2];
	    }
	}
	normals[i] = { (fastf_t)nrm[0], (fastf_t)nrm[1], (fastf_t)nrm[2] };
    }
}

/* Classify poles inner vs outer */
static void classify_poles(
    const point_t *samples,
    int n_samples,
    const std::vector<P3>& poleA,
    const std::vector<P3>& poleB,
    const std::vector<P3>& normals,
    std::vector<P3>& inner_pole,
    std::vector<P3>& outer_pole)
{
    inner_pole.resize(n_samples);
    outer_pole.resize(n_samples);
    for (int i=0; i<n_samples; ++i) {
	double vA[3] = { poleA[i][0]-samples[i][0],
			 poleA[i][1]-samples[i][1],
			 poleA[i][2]-samples[i][2]
		       };
	double vB[3] = { poleB[i][0]-samples[i][0],
			 poleB[i][1]-samples[i][1],
			 poleB[i][2]-samples[i][2]
		       };
	double nrm[3] = { normals[i][0], normals[i][1], normals[i][2] };
	double dA = dot3(vA,nrm);
	double dB = dot3(vB,nrm);
	double lA = len3(vA), lB = len3(vB);
	if (dA >= 0 && dB < 0) {
	    outer_pole[i]=poleA[i];
	    inner_pole[i]=poleB[i];
	} else if (dB >= 0 && dA < 0) {
	    outer_pole[i]=poleB[i];
	    inner_pole[i]=poleA[i];
	} else {
	    if (lA >= lB) {
		outer_pole[i]=poleA[i];
		inner_pole[i]=poleB[i];
	    } else {
		outer_pole[i]=poleB[i];
		inner_pole[i]=poleA[i];
	    }
	}
    }
}

/* Triangle circumradius */
static double triangle_circumradius(const double *a,const double* b,const double* c)
{
    double ab[3], bc[3], ca[3];
    sub3(a,b,ab);
    sub3(b,c,bc);
    sub3(c,a,ca);
    double lab=len3(ab), lbc=len3(bc), lca=len3(ca);
    double n[3];
    cross3(ab,ca,n);
    double area2=len3(n);
    if (area2 < 1e-30) return std::numeric_limits<double>::infinity();
    return (lab*lbc*lca)/area2;
}

/* Extract crust faces */
static void extract_crust_faces(
    GEO::Delaunay *regular,
    int num_samples,
    const std::vector<PoleClass>& pole_class,
    const std::vector<P3>& inner_pole,
    const std::vector<P3>& outer_pole,
    const point_t *samples,
    double lfs_ratio_max,
    double lfs_eps,
    bool orient_faces,
    std::vector<std::array<int, 3>>& faces_out)
{
    struct FaceData {
	int mask=0;     // bit1 inner, bit2 outer
	int count=0;
	int pole_vid[2] = {-1, -1};
	int oriented[3] = {-1, -1, -1};
    };
    struct H {
	size_t operator()(const std::array<int, 3>&k) const
	{
	    return (size_t)k[0]*73856093u ^ (size_t)k[1]*19349663u ^ (size_t)k[2]*83492791u;
	}
    };
    std::unordered_map<std::array<int, 3>, FaceData, H> fmap;

    GEO::index_t nc = regular->nb_cells();
    GEO::index_t cs = regular->cell_size();

    for (GEO::index_t t=0; t<nc; ++t) {
	if (!regular->cell_is_finite(t)) continue;
	int smp[4];
	int sc=0;
	int poles[4];
	int pc=0;
	for (GEO::index_t lv=0; lv<cs; ++lv) {
	    int gv = (int)regular->cell_vertex(t,lv);
	    if (gv < num_samples) smp[sc++]=gv;
	    else poles[pc++]=gv;
	}
	if (sc!=3 || pc!=1) continue;
	int pvid = poles[0];
	PoleClass pcclass = pole_class[pvid];
	if (pcclass==POLE_UNSET) continue;
	int a=smp[0], b=smp[1], c=smp[2];
	std::array<int, 3> key{a,b,c};
	std::sort(key.begin(), key.end());
	auto &fd = fmap[key];
	if (fd.count < 2) {
	    fd.pole_vid[fd.count] = pvid;
	    if (fd.count==0) {
		fd.oriented[0]=a;
		fd.oriented[1]=b;
		fd.oriented[2]=c;
	    }
	}
	fd.count++;
	if (pcclass==POLE_INNER) fd.mask |= 1;
	else if (pcclass==POLE_OUTER) fd.mask |= 2;
    }

    faces_out.clear();
    faces_out.reserve(fmap.size());

    // local feature size
    std::vector<double> lfs(num_samples,0.0);
    for (int i=0; i<num_samples; ++i) {
	double dx=outer_pole[i][0]-inner_pole[i][0];
	double dy=outer_pole[i][1]-inner_pole[i][1];
	double dz=outer_pole[i][2]-inner_pole[i][2];
	lfs[i] = 0.5*sqrt(dx*dx+dy*dy+dz*dz);
    }

    for (auto &kv : fmap) {
	const auto &key = kv.first;
	const FaceData &fd = kv.second;
	if (fd.count!=2) continue;
	if (fd.mask != (1|2)) continue; // need both inner+outer
	const double *pa = samples[key[0]];
	const double *pb = samples[key[1]];
	const double *pc = samples[key[2]];
	double R = triangle_circumradius(pa,pb,pc);
	double avg_lfs = (lfs[key[0]] + lfs[key[1]] + lfs[key[2]])/3.0;
	if (!(avg_lfs > lfs_eps) || R > lfs_ratio_max*avg_lfs) continue;

	std::array<int, 3> tri;
	if (orient_faces) {
	    double va[3] = { outer_pole[key[0]][0]-pa[0],
			     outer_pole[key[0]][1]-pa[1],
			     outer_pole[key[0]][2]-pa[2]
			   };
	    double vb[3] = { outer_pole[key[1]][0]-pb[0],
			     outer_pole[key[1]][1]-pb[1],
			     outer_pole[key[1]][2]-pb[2]
			   };
	    double vc[3] = { outer_pole[key[2]][0]-pc[0],
			     outer_pole[key[2]][1]-pc[1],
			     outer_pole[key[2]][2]-pc[2]
			   };
	    double vavg[3] = { (va[0]+vb[0]+vc[0])/3.0,
			       (va[1]+vb[1]+vc[1])/3.0,
			       (va[2]+vb[2]+vc[2])/3.0
			     };
	    double ab[3], ac[3], n[3];
	    sub3(pb,pa,ab);
	    sub3(pc,pa,ac);
	    cross3(ab,ac,n);
	    if (dot3(n,vavg) < 0.0)
		tri = { fd.oriented[0], fd.oriented[2], fd.oriented[1] };
	    else
		tri = { fd.oriented[0], fd.oriented[1], fd.oriented[2] };
	} else {
	    tri = key;
	}
	faces_out.push_back(tri);
    }
}

static void to_mesh(
    GEO::Mesh &M,
    const point_t *pts, int npts,
    const std::vector<std::array<int, 3>>& tris)
{
    M.clear();
    M.vertices.clear();
    M.vertices.set_dimension(3);
    for (int i=0; i<npts; ++i) {
	GEO::index_t v = M.vertices.create_vertex();
	double *p = M.vertices.point_ptr(v);
	p[0]=pts[i][0];
	p[1]=pts[i][1];
	p[2]=pts[i][2];
    }
    for (auto &tr : tris) {
	GEO::index_t f = M.facets.create_polygon(3);
	M.facets.set_vertex(f,0,tr[0]);
	M.facets.set_vertex(f,1,tr[1]);
	M.facets.set_vertex(f,2,tr[2]);
    }
    M.facets.connect();
}

static void mesh_to_arrays(
    const GEO::Mesh &M,
    point_t **out_pts, int *npts,
    int **out_faces, int *nfaces)
{
    int nv=(int)M.vertices.nb();
    int nf=(int)M.facets.nb();
    *npts=nv;
    *nfaces=nf;
    *out_pts = (point_t*)bu_calloc(nv,sizeof(point_t),"pc pts");
    *out_faces = (int*)bu_calloc(nf*3,sizeof(int),"pc faces");
    for (int i=0; i<nv; ++i) {
	const double *p = M.vertices.point_ptr(i);
	(*out_pts)[i][0]=p[0];
	(*out_pts)[i][1]=p[1];
	(*out_pts)[i][2]=p[2];
    }
    for (int f=0; f<nf; ++f) {
	(*out_faces)[3*f+0]=(int)M.facets.vertex(f,0);
	(*out_faces)[3*f+1]=(int)M.facets.vertex(f,1);
	(*out_faces)[3*f+2]=(int)M.facets.vertex(f,2);
    }
}

/* Main API */
extern "C" int
bg_3d_powercrust(
    int **faces, int *num_faces,
    point_t **points, int *num_pnts,
    const point_t *input_points_3d, int num_input_pnts,
    struct bg_3d_powercrust_opts *user_opts)
{
    if (!faces || !num_faces || !points || !num_pnts ||
	!input_points_3d || num_input_pnts < 4) {
	bu_log("bg_3d_powercrust: invalid input\n");
	return -1;
    }
    *faces=NULL;
    *num_faces=0;
    *points=NULL;
    *num_pnts=0;

    bg_3d_powercrust_opts opts;
    if (user_opts) opts = *user_opts;

    suppress_geogram_init();

    /* Delaunay on samples */
    GEO::vector<double> coords;
    coords.reserve(num_input_pnts*3);
    for (int i=0; i<num_input_pnts; ++i) {
	coords.push_back(input_points_3d[i][0]);
	coords.push_back(input_points_3d[i][1]);
	coords.push_back(input_points_3d[i][2]);
    }
    GEO::Delaunay_var del = GEO::Delaunay::create(3,"BDEL");
    if (!del) {
	bu_log("PowerCrust: cannot create BDEL\n");
	return -1;
    }
    del->set_stores_cicl(true);
    del->set_vertices((GEO::index_t)num_input_pnts, coords.data());

    /* Convex hull */
    std::vector<char> is_hull_vertex;
    point_t hull_centroid;
    convex_hull_prefilter(input_points_3d, num_input_pnts, is_hull_vertex, hull_centroid);

    /* Normals */
    std::vector<P3> normals;
    estimate_normals(del.get(), input_points_3d, num_input_pnts,
		     is_hull_vertex, hull_centroid,
		     normals, opts.normal_k_min);

    /* Raw poles */
    std::vector<P3> rawA, rawB;
    find_raw_poles(del.get(), input_points_3d, num_input_pnts,
		   rawA, rawB,
		   opts.pole_opposite_cos_threshold,
		   opts.degenerate_tet_volume_eps);

    /* Classify poles */
    std::vector<P3> inner_pole, outer_pole;
    classify_poles(input_points_3d, num_input_pnts,
		   rawA, rawB, normals,
		   inner_pole, outer_pole);

    /* Weighted regular triangulation (samples + inner + outer) */
    size_t total_vertices = (size_t)num_input_pnts * 3;
    GEO::vector<double> reg4d;
    reg4d.reserve(total_vertices*4);
    double W=0.0;
    std::vector<double> raw_weights(total_vertices,0.0);

    for (int i=0; i<num_input_pnts; ++i) raw_weights[i]=0.0;
    for (int i=0; i<num_input_pnts; ++i) {
	double d2 = DIST_PNT_PNT_SQ(inner_pole[i].data(), input_points_3d[i]);
	raw_weights[num_input_pnts + i] = d2;
	if (d2>W) W=d2;
    }
    for (int i=0; i<num_input_pnts; ++i) {
	double d2 = DIST_PNT_PNT_SQ(outer_pole[i].data(), input_points_3d[i]);
	raw_weights[2*num_input_pnts + i] = d2;
	if (d2>W) W=d2;
    }
    W += 1e-16;

    auto push4d = [&](const double xyz[3], double w) {
	double t = sqrt(std::max(0.0, W - w));
	reg4d.push_back(xyz[0]);
	reg4d.push_back(xyz[1]);
	reg4d.push_back(xyz[2]);
	reg4d.push_back(t);
    };
    for (int i=0; i<num_input_pnts; ++i) push4d(input_points_3d[i], 0.0);
    for (int i=0; i<num_input_pnts; ++i) push4d(inner_pole[i].data(), raw_weights[num_input_pnts+i]);
    for (int i=0; i<num_input_pnts; ++i) push4d(outer_pole[i].data(), raw_weights[2*num_input_pnts+i]);

    GEO::Delaunay_var regular = GEO::Delaunay::create(4,"BPOW");
    if (!regular) {
	bu_log("PowerCrust: cannot create BPOW\n");
	return -1;
    }
    regular->set_vertices((GEO::index_t)total_vertices, reg4d.data());

    /* Pole class tag array */
    std::vector<PoleClass> pole_class(total_vertices, POLE_UNSET);
    for (int i=0; i<num_input_pnts; ++i) {
	pole_class[num_input_pnts + i] = POLE_INNER;
	pole_class[2*num_input_pnts + i] = POLE_OUTER;
    }

    /* Extract crust faces */
    std::vector<std::array<int, 3>> crust_faces;
    extract_crust_faces(
	regular.get(),
	num_input_pnts,
	pole_class,
	inner_pole,
	outer_pole,
	input_points_3d,
	opts.lfs_radius_ratio_max,
	opts.feature_eps,
	opts.orient_faces,
	crust_faces
    );

    if (crust_faces.empty()) {
	bu_log("PowerCrust: no faces extracted\n");
	return -2;
    }

    /* Build mesh & clean */
    GEO::Mesh mesh;
    to_mesh(mesh, input_points_3d, num_input_pnts, crust_faces);
    double total_area = GEO::Geom::mesh_area(mesh,3);
    double min_area = std::max(0.0, opts.min_component_area_ratio) * total_area;
    if (min_area > 0.0)
	GEO::remove_small_connected_components(mesh, min_area);
    GEO::fill_holes(mesh, opts.max_hole_area);
    mesh.facets.triangulate();

    /* Export */
    mesh_to_arrays(mesh, points, num_pnts, faces, num_faces);

    if (opts.verbose) {
	bu_log("PowerCrust: input_pts=%d output_faces=%d output_verts=%d\n",
	       num_input_pnts, *num_faces, *num_pnts);
    }
    return 0;
}

/* Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
