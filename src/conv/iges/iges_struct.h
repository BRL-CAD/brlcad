/*                   I G E S _ S T R U C T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef CONV_IGES_IGES_STRUCT_H
#define CONV_IGES_IGES_STRUCT_H

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"


#define NAMESIZE 16 /* from db.h */

#define TOL 0.0005

#define Union 1
#define Intersect 2
#define Subtract 3

#ifdef __cplusplus
extern "C" {
#endif

/* Circularly linked list of files and names for external references */
struct file_list
{
    struct bu_list l;			/* for links */
    char *file_name;		/* Name of external file */
    char obj_name[NAMESIZE+1];	/* BRL-CAD name for top level object */
};


/* linked list of used BRL-CAD object names */
struct name_list
{
    char name[NAMESIZE+1];
    struct name_list *next;
};


/* Structure for storing info from the IGES file directory section along with
   transformation matrices */
struct iges_directory
{
    int type; /* IGES entity type */
    int form; /* IGES form number */
    int view; /* View field from DE, indicates which views this entity is in */
    int param; /* record number for parameter entry */
    int paramlines; /* number of lines for this entity in parameter section */
    int direct; /* directory entry sequence number */
    int status; /* status entry from directory entry */
/*
 * Note that the directory entry sequence number and the directory structure
 * array index are not the same.  The index into the array can be calculated
 * as (sequence number - 1)/2.
 */
    int trans; /* index into directory array for transformation matrix */
    int colorp; /* pointer to color definition entity (or color number) */
    unsigned char rgb[3]; /* Actual color */
    char *mater;	/* material parameter string */
    int referenced;
/*
 * uses of the "referenced" field:
 *	for solid objects - number of times this entity is referenced by
 *		boolean trees or solid assemblies.
 *	For transformation entities - it indicates whether the matrix list
 *		has been evaluated.
 *	for attribute instances - It contains the DE for the attribute
 *		definition entity.
 */
    char *name; /* entity name */
    mat_t *rot; /* transformation matrix. */
};


/* Structure used in building boolean trees in memory */
struct node
{
    int op; /* if positive, this is an operator (Union, Intersect, or Subtract)
	       if negative, this is a directory entry sequence number (operand) */
    struct node *left, *right, *parent;
};


/* structure for storing attributes */
struct brlcad_att
{
    char *material_name;
    char *material_params;
    int region_flag;
    int ident;
    int air_code;
    int material_code;
    int los_density;
    int inherit;
    int color_defined;
};


/* Structure for linked list of points */
struct ptlist
{
    point_t pt;
    struct ptlist *next, *prev;
};


/* Structures for Parametric Splines */
struct segment
{
    int segno;
    fastf_t tmin, tmax;
    fastf_t cx[4], cy[4], cz[4];
    struct segment *next;
};


struct spline
{
    int ndim, nsegs;
    struct segment *start;
};


struct reglist
{
    char name[NAMESIZE+1];
    struct node *tree;
    struct reglist *next;
};


struct types
{
    int type;
    char *name;
    int count;
};


struct iges_edge_use
{
    int edge_de;		/* directory sequence number for edge list or vertex list */
    int edge_is_vertex;		/* flag */
    int index;			/* index into list for this edge */
    int orient;			/* orientation flag (1 => agrees with geometry */
    struct iges_param_curve *root;
};


struct iges_vertex
{
    point_t pt;
    struct vertex *v;
};


struct iges_vertex_list
{
    int vert_de;
    int no_of_verts;
    struct iges_vertex *i_verts;
    struct iges_vertex_list *next;
};


struct iges_edge
{
    int curve_de;
    int start_vert_de;
    int start_vert_index;
    int end_vert_de;
    int end_vert_index;
};


struct iges_param_curve
{
    int curve_de;
    struct iges_param_curve *next;
};


struct iges_edge_list
{
    int edge_de;
    int no_of_edges;
    struct iges_edge *i_edge;
    struct iges_edge_list *next;
};


char *iges_type(int type_no);
char *Make_unique_brl_name(char *name);
int Add_loop_to_face(struct shell *s, struct faceuse *fu, int entityno, int face_orient);
int Add_nurb_loop_to_face(struct shell *s, struct faceuse *fu, int loop_entityno);
int arb_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int ell_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int nmg_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int nmgregion_to_iges(char *name, struct nmgregion *r, int dependent, FILE *fp_dir, FILE *fp_param);
int null_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int planar_nurb(int entityno);
int sph_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int tgc_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int tor_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
int write_shell_face_loop(struct nmgregion *r, int edge_de, struct bu_ptbl *etab, int vert_de, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
struct edge_g_cnurb *Get_cnurb(int entity_no);
struct edge_g_cnurb *Get_cnurb_curve(int curve_de, int *linear);
struct faceuse *Add_face_to_shell(struct shell *s, int entityno, int face_orient);
struct faceuse *Make_nurb_face(struct shell *s, int surf_entityno);
struct faceuse *Make_planar_face(struct shell *s, int entityno, int face_orient);
struct iges_edge_list *Get_edge_list(struct iges_edge_use *edge);
struct iges_edge_list *Read_edge_list(struct iges_edge_use *edge);
struct iges_vertex *Get_iges_vertex(struct vertex *v);
struct iges_vertex_list *Get_vertex_list(int vert_de);
struct iges_vertex_list *Read_vertex_list(int vert_de);
struct loopuse *Make_loop(int entity_no, int orientation, int on_surf_de, struct face_g_snurb *surf, struct faceuse *fu);
struct shell *Add_inner_shell(struct nmgregion *r, int entityno);
struct shell *Get_outer_shell(struct nmgregion *r, int entityno);
struct face_g_snurb *Get_nurb_surf(int entity_no, struct model *m);
struct vertex **Get_vertex(struct iges_edge_use *edge);
union tree *do_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree);
void count_refs(struct db_i *dbip, struct directory *dp);
void csg_comb_func(struct db_i *dbip, struct directory *dp);
void csg_leaf_func(struct db_i *dbip, struct directory *dp);
void nmg_region_edge_list(struct bu_ptbl *tab, struct nmgregion *r);
void set_iges_tolerances(struct bn_tol *set_tol, struct rt_tess_tol *set_ttol);
void w_start_global(FILE *fp_dir, FILE *fp_param, char *db_name, char *prog_name, char *output_file, char *id, char *version);
void w_terminate(FILE *fp);
void write_edge_list(struct nmgregion *r, int vert_de, struct bu_ptbl *etab, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
void write_vertex_list(struct nmgregion *r, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
struct iges_edge *Get_edge(struct iges_edge_use *e_use);
struct vertex *Get_edge_start_vertex(struct iges_edge *e);
struct vertex *Get_edge_end_vertex(struct iges_edge *e);
void usage();
void Initstack();
void Push(union tree *ptr);
union tree *Pop();
void Freestack();
int Recsize();
void Zero_counts();
void Summary();
void Readstart();
void Readglobal(int file_count);
int Findp();
void Free_dir();
void Makedir();
void Docolor();
void Gett_att();
void Evalxform();
void Check_names();
void Conv_drawings();
void Do_subfigs();
void Convtrimsurfs();
void Convsurfs();
void Convinst();
void Convsolids();
void Get_att();
void Convtree();
void Convassem();
int Readrec(int recno);
void Readint(int *inum, char *id);
void Readflt(fastf_t *inum, char *id);
void Readdbl(double *inum, char *id);
void Readstrg(char *id);
void Readname(char **ptr, char *id);
void Readcnv(fastf_t *inum, char *id);
void Assign_surface_to_fu(struct faceuse *fu, struct face_g_snurb *srf);
void Assign_cnurb_to_eu(struct edgeuse *eu, struct edge_g_cnurb *crv);
int Put_vertex(struct vertex *v, struct iges_edge_use *edge);
int Getcurve(int curve, struct ptlist **curv_pts);
void Orient_loops(struct nmgregion *r);
int read_spline(int entityno, struct face_g_snurb **b_patch);
void Freeknots();

/* temp defs while working on replacing local function Matmult with libbn functions */
/* #define USE_BN_MULT_ */
#if defined(USE_BN_MULT_)
#include "bn.h"
#else
void Matmult(mat_t a, mat_t b, mat_t c);
#endif

int Extrudcon(int entityno, int curve, vect_t evect);
int Extrudcirc(int entityno, int curve, vect_t evect);
void Read_att(int att_de, struct brlcad_att *att);
int block(int entityno);
int wedge(int entityno);
int cyl(int entityno);
int cone(int entityno);
int sphere(int entityno);
int torus(int entityno);
int revolve(int entityno);
int extrude(int entityno);
int ell(int entityno);
int brep(int entityno);
void Readtime(char *id);
void Readcols(char *id, int cols);
void Readmatrix(int xform, mat_t rot);

#ifdef __cplusplus
}
#endif

#endif /* CONV_IGES_IGES_STRUCT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
