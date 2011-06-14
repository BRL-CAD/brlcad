/*                   I G E S _ S T R U C T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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


/* Structure used in builing boolean trees in memory */
struct node
{
    int op; /* if positive, this is an operator (Union, Intersect, or Subtract)
	       if negative, this is a directory entry sequence number (operand) */
    struct node *left, *right, *parent;
};


/* structure for storing atributes */
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


extern char *iges_type(int type_no);
extern char *Make_unique_brl_name(char *name);
extern int Add_loop_to_face(struct shell *s, struct faceuse *fu, int entityno, int face_orient);
extern int Add_nurb_loop_to_face(struct shell *s, struct faceuse *fu, int loop_entityno);
extern int arb_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int ell_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int nmg_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int nmgregion_to_iges(char *name, struct nmgregion *r, int dependent, FILE *fp_dir, FILE *fp_param);
extern int null_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int planar_nurb(int entityno);
extern int sph_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int tgc_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int tor_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int write_shell_face_loop(struct nmgregion *r, int edge_de, struct bu_ptbl *etab, int vert_de, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern struct edge_g_cnurb *Get_cnurb(int entity_no);
extern struct edge_g_cnurb *Get_cnurb_curve(int curve_de, int *linear);
extern struct faceuse *Add_face_to_shell(struct shell *s, int entityno, int face_orient);
extern struct faceuse *Make_nurb_face(struct shell *s, int surf_entityno);
extern struct faceuse *Make_planar_face(struct shell *s, int entityno, int face_orient);
extern struct iges_edge_list *Get_edge_list(struct iges_edge_use *edge);
extern struct iges_edge_list *Read_edge_list(struct iges_edge_use *edge);
extern struct iges_vertex *Get_iges_vertex(struct vertex *v);
extern struct iges_vertex_list *Get_vertex_list(int vert_de);
extern struct iges_vertex_list *Read_vertex_list(int vert_de);
extern struct loopuse *Make_loop(int entity_no, int orientation, int on_surf_de, struct face_g_snurb *surf, struct faceuse *fu);
extern struct shell *Add_inner_shell(struct nmgregion *r, int entityno);
extern struct shell *Get_outer_shell(struct nmgregion *r, int entityno);
extern struct face_g_snurb *Get_nurb_surf(int entity_no, struct model *m);
extern struct vertex **Get_vertex(struct iges_edge_use *edge);
extern union tree *do_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree);
extern void count_refs(struct db_i *dbip, struct directory *dp);
extern void csg_comb_func(struct db_i *dbip, struct directory *dp);
extern void csg_leaf_func(struct db_i *dbip, struct directory *dp);
extern void nmg_region_edge_list(struct bu_ptbl *tab, struct nmgregion *r);
extern void set_iges_tolerances(struct bn_tol *set_tol, struct rt_tess_tol *set_ttol);
extern void w_start_global(FILE *fp_dir, FILE *fp_param, char *db_name, char *prog_name, char *output_file, char *id, char *version);
extern void w_terminate(FILE *fp);
extern void write_edge_list(struct nmgregion *r, int vert_de, struct bu_ptbl *etab, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern void write_vertex_list(struct nmgregion *r, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern struct iges_edge *Get_edge(struct iges_edge_use *e_use);
extern struct vertex *Get_edge_start_vertex(struct iges_edge *e);
extern struct vertex *Get_edge_end_vertex(struct iges_edge *e);
extern void usage();
extern void Initstack();
extern void Push(union tree *ptr);
extern union tree *Pop();
extern void Freestack();
extern int Recsize();
extern void Zero_counts();
extern void Summary();
extern void Readstart();
extern void Readglobal(int file_count);
extern int Findp();
extern void Free_dir();
extern void Makedir();
extern void Docolor();
extern void Gett_att();
extern void Evalxform();
extern void Check_names();
extern void Conv_drawings();
extern void Do_subfigs();
extern void Convtrimsurfs();
extern void Convsurfs();
extern void Convinst();
extern void Convsolids();
extern void Get_att();
extern void Convtree();
extern void Convassem();
extern int Readrec(int recno);
extern void Readint(int *inum, char *id);
extern void Readflt(fastf_t *inum, char *id);
extern void Readdbl(double *inum, char *id);
extern void Readstrg(char *id);
extern void Readname(char **ptr, char *id);
extern void Readcnv(fastf_t *inum, char *id);
extern void Assign_surface_to_fu(struct faceuse *fu, struct face_g_snurb *srf);
extern void Assign_cnurb_to_eu(struct edgeuse *eu, struct edge_g_cnurb *crv);
extern int Put_vertex(struct vertex *v, struct iges_edge_use *edge);
extern int Getcurve(int curve, struct ptlist **curv_pts);
extern void Orient_loops(struct nmgregion *r);
extern int spline(int entityno, struct face_g_snurb **b_patch);
extern void Freeknots();
extern void Matmult(mat_t a, mat_t b, mat_t c);
extern int Extrudcon(int entityno, int curve, vect_t evect);
extern int Extrudcirc(int entityno, int curve, vect_t evect);
extern void Read_att(int att_de, struct brlcad_att *att);
extern int block(int entityno);
extern int wedge(int entityno);
extern int cyl(int entityno);
extern int cone(int entityno);
extern int sphere(int entityno);
extern int torus(int entityno);
extern int revolve(int entityno);
extern int extrude(int entityno);
extern int ell(int entityno);
extern int brep(int entityno);
extern void Readtime(char *id);
extern void Readcols(char *id, int cols);
extern void Readmatrix(int xform, mat_t rot);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
