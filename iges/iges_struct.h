
#include <brlcad/machine.h>
#include <brlcad/vmath.h>

extern fastf_t brlabs();

#define	NAMELEN	16	/* from "brlcad/db.h" */

#define	ARCSEGS	10	/* number of segments to use in representing a circle */

#define	INFINITY	1.0e20
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
struct directory
{
	int type; /* IGES entity type */
	int form; /* IGES form number */
	int param; /* record number for parameter entry */
	int paramlines; /* number of lines for this entity in parameter section */
	int direct; /* directory entry sequence number */
/* Note that the directory entry sequence number and the directory structure
   array index are not the same.  The index into the array can be calculated
   as (sequence number - 1)/2.	*/
	int trans; /* index into directory array for transformation matrix */
	int colorp; /* pointer to color definition entity (or color number) */
	unsigned char rgb[3]; /* Actual color */
	char *mater;	/* material parameter string */
	int referenced; /* number of times this entity is referenced by boolean trees,
		solids, or solid assemblies. For transformation entities,
		it indicates whether the matrix list has been evaluated */
	char name[NAMELEN+1]; /* entity name */
	mat_t *rot; /*  transformation matrix. */
};

/* Structure used in builing boolean trees in memory */
struct node
{
	int op; /* if positive, this is an operator (Union, Intersect, or Subtract)
		   if negative, this is a directory entry sequence number (operand) */
	struct node *left,*right,*parent;
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
