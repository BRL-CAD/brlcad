#include "conf.h"

extern char	version[];

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"

extern fastf_t brlabs();

#define	NAMESIZE	16	/* from db.h */

#define	TOL		0.0005
#define EQUAL( a , b )		((brlabs( (a)-(b) ) < TOL) ? 1 : 0 )
#define	SAMEPT( pt1 , pt2 )	(EQUAL( pt1[X] , pt2[X] ) && \
				 EQUAL( pt1[Y] , pt2[Y] ) && \
				 EQUAL( pt1[Z] , pt2[Z] ) )

#define	Union		1
#define	Intersect	2
#define	Subtract	3

/* Circularly linked list of files and names for external references */
struct file_list
{
	struct rt_list		l;			/* for links */
	char			*file_name;		/* Name of external file */
	char			obj_name[NAMESIZE+1];	/* BRL-CAD name for top level object */
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
	mat_t *rot; /*  transformation matrix. */
};

/* Structure used in builing boolean trees in memory */
struct node
{
	int op; /* if positive, this is an operator (Union, Intersect, or Subtract)
		   if negative, this is a directory entry sequence number (operand) */
	struct node *left,*right,*parent;
};

/* structure for storing atributes */
struct brlcad_att
{
	char			*material_name;
	char			*material_params;
	int			region_flag;
	int			ident;
	int			air_code;
	int			material_code;
	int			los_density;
	int			inherit;
	int			color_defined;
};

/* Structure for linked list of points */
struct ptlist
{
	point_t pt;
	struct ptlist *next,*prev;
};

/* Structures for Parametric Splines */
struct segment
{
	int segno;
	fastf_t tmin,tmax;
	fastf_t cx[4],cy[4],cz[4];
	struct segment *next;
};

struct spline
{
	int ndim,nsegs;
	struct segment *start;
};

struct reglist
{
	char name[NAMESIZE];
	struct node *tree;
	struct reglist *next;
};

struct types
{
	int	type;
	char	*name;
	int	count;
};

struct iges_edge_use
{
	int	edge_de;		/* directory sequence number for edge list or vertex list */
	int	edge_is_vertex;		/* flag */
	int	index;			/* index into list for this edge */
	int	orient;			/* orientation flag (1 => agrees with geometry */
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

RT_EXTERN( char *iges_type, (int type_no ) );
RT_EXTERN( char *Make_unique_brl_name, (char *name ) );
RT_EXTERN( int Add_loop_to_face , (struct shell *s , struct faceuse *fu , int entityno , int face_orient ));
RT_EXTERN( int Add_nurb_loop_to_face, ( struct shell *s, struct faceuse *fu, int loop_entityno, int face_orient ) );
RT_EXTERN( int arb_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int ell_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int nmg_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int nmgregion_to_iges , ( char *name , struct nmgregion *r , int dependent , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( int null_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int planar_nurb , ( int entityno ) );
RT_EXTERN( int sph_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int tgc_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int tor_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int write_shell_face_loop , ( struct nmgregion *r , int edge_de , struct nmg_ptbl *etab , int vert_de , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( struct edge_g_cnurb *Get_cnurb, ( int entity_no ) );
RT_EXTERN( struct edge_g_cnurb *Get_cnurb_curve, (int curve_de, int *linear ) );
RT_EXTERN( struct edge_g_cnurb *rt_arc2d_to_cnurb, ( point_t center, point_t start, point_t end, int pt_type, struct rt_tol *tol ) );
RT_EXTERN( struct face *nmg_find_top_face , (struct shell *s , long *flags ) );
RT_EXTERN( struct faceuse *Add_face_to_shell , ( struct shell *s , int entityno , int face_orient) );
RT_EXTERN( struct faceuse *Make_nurb_face, ( struct shell *s, int surf_entityno ) );
RT_EXTERN( struct faceuse *Make_planar_face , ( struct shell *s , int entityno , int face_orient ) );
RT_EXTERN( struct faceuse *nmg_cmface , ( struct shell *s, struct vertex ***verts, int no_of_edges ) );
RT_EXTERN( struct iges_edge_list *Get_edge_list , (struct iges_edge_use *edge ) );
RT_EXTERN( struct iges_edge_list *Read_edge_list , ( struct iges_edge_use *edge ) );
RT_EXTERN( struct iges_vertex *Get_iges_vertex, (struct vertex *v ) );
RT_EXTERN( struct iges_vertex_list *Get_vertex_list , (int vert_de ) );
RT_EXTERN( struct iges_vertex_list *Read_vertex_list , ( int vert_de ) );
RT_EXTERN( struct loopuse *Make_loop, (int entity_no, int orientation, int on_surf_de, struct face_g_snurb *surf, struct faceuse *fu ) );
RT_EXTERN( struct model *nmg_mmr , () );
RT_EXTERN( struct shell *Add_inner_shell , ( struct nmgregion *r, int entityno, int shell_orient ) );
RT_EXTERN( struct shell *Get_outer_shell , ( struct nmgregion *r, int entityno, int shell_orient ) );
RT_EXTERN( struct shell *nmg_msv , (struct nmgregion *r) );
RT_EXTERN( struct face_g_snurb *Get_nurb_surf, (int entity_no, struct model *m ) );
RT_EXTERN( struct vertex **Get_vertex , (struct iges_edge_use *edge ) );
RT_EXTERN( union tree *do_nmg_region_end , (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( void count_refs , ( struct db_i *dbip , struct directory *dp ) );
RT_EXTERN( void csg_comb_func , ( struct db_i *dbip , struct directory *dp ) );
RT_EXTERN( void csg_leaf_func , ( struct db_i *dbip , struct directory *dp ) );
RT_EXTERN( void nmg_region_edge_list , ( struct nmg_ptbl *tab , struct nmgregion *r ) );
RT_EXTERN( void set_iges_tolerances , ( struct rt_tol *set_tol , struct rt_tess_tol *set_ttol ) );
RT_EXTERN( void w_start_global , (FILE *fp_dir , FILE *fp_param , char *db_name , char *prog_name , char *output_file , char *id , char *version ));
RT_EXTERN( void w_terminate , (FILE *fp) );
RT_EXTERN( void write_edge_list , (struct nmgregion *r , int vert_de , struct nmg_ptbl *etab , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( void write_vertex_list , ( struct nmgregion *r , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( struct iges_edge *Get_edge, (struct iges_edge_use *e_use ) );
RT_EXTERN( struct vertex *Get_edge_start_vertex, (struct iges_edge *e ) );
RT_EXTERN( struct vertex *Get_edge_end_vertex, (struct iges_edge *e ) );
