
extern fastf_t brlabs();

#define	NAMELEN	16	/* from "brlcad/db.h" */

#define	ARCSEGS	10	/* number of segments to use in representing a circle */

#define	TOL		0.0005
#define EQUAL( a , b )		((brlabs( (a)-(b) ) < TOL) ? 1 : 0 )
#define	SAMEPT( pt1 , pt2 )	(EQUAL( pt1[X] , pt2[X] ) && \
				 EQUAL( pt1[Y] , pt2[Y] ) && \
				 EQUAL( pt1[Z] , pt2[Z] ) )

#define	Union		1
#define	Intersect	2
#define	Subtract	3

/* Structure for storing info from the IGES file directory section along with
	transformation matrices */
struct iges_directory
{
	int type; /* IGES entity type */
	int form; /* IGES form number */
	int param; /* record number for parameter entry */
	int paramlines; /* number of lines for this entity in parameter section */
	int direct; /* directory entry sequence number */
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
	char name[NAMELEN];
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

struct iges_edge_list
{
	int edge_de;
	int no_of_edges;
	struct iges_edge *i_edge;
	struct iges_edge_list *next;
};
