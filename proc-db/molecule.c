/* 
 *			M O L E C U L E . C
 *
 * Create a molecule from G. Adams format
 * 
 * Author:	Paul R. Stay
 * 		Ballistic Research Labratory
 * 		Aberdeen Proving Ground, Md.
 * Date:	Mon Dec 29 1986
 */
static char rcs_ident[] = "$Header$";

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "db.h"
#include "vmath.h"
#include "wdb.h"


struct sphere  {
	struct sphere * next;		/* Next Sphere */
	int	s_id;			/* Sphere id */
	char	s_name[15];		/* Sphere name */
	point_t	s_center;		/* Sphere Center */
	fastf_t	s_rad;			/* Sphere radius */
	int	s_atom_type;		/* Atom Type */
};

struct sphere *s_list = (struct sphere *) 0;
struct sphere *s_head = (struct sphere *) 0;

struct atoms  {
	int a_id;
	char a_name[128];
	unsigned char red, green, blue;
};

struct atoms atom_list[50];

char * matname = "plastic";
char * matparm = "shine=100.0 diffuse=.8 specular=.2";

void	read_data(), process_sphere();

struct wmember head;

static CONST char usage[] = "Usage: molecule db_title < mol-cube.dat > mol.g\n";

main(argc, argv)
int argc;
char ** argv;
{

	if( argc != 2 )  {
		fputs( usage, stderr );
		exit(1);
	}

	RT_LIST_INIT( &head.l );
	mk_id( stdout, argv[1] );
	read_data();

	/* Build the overall combination */
	mk_lfcomb( stdout, "mol.g", &head, 0 );
}

/* File format from stdin
 *
 * For a ATOM DATA_TYPE ATOM_ID ATOM_NAME RED GREEN BLUE
 * For a Sphere DATA_TYPE SPH_ID CENTER ( X, Y, Z) RADIUS ATOM_TYPE
 * For a Bond   DATA_TYPE SPH_ID SPH_ID
 * DATA_TYPE = 0 - Atom1 - Sphere 2 - Bond
 * SPH_ID = integer
 * CENTER = three float values x, y, z
 * RADIUS = Float
 * ATOM_TYPE = integer
 * ATOM_NAME = Character pointer to name value.
 */
void
read_data( )
{

	int             data_type;
	int             sphere_id;
	point_t		center;
	float           x, y, z;
	float           sphere_radius;
	int             atom_type;
	int             b_1, b_2;
	int red, green, blue;
	int i = 0;
	

	while (scanf(" %d", &data_type) != 0) {

		switch (data_type) {
		case (0):
			scanf("%d", &i);
			scanf("%s", atom_list[i].a_name);
			scanf("%d", &red);
			scanf("%d", &green);
			scanf("%d", &blue);
			atom_list[i].red  = red;
			atom_list[i].green  = green;
			atom_list[i].blue  = blue;
			break;
		case (1):
		        scanf("%d", &sphere_id);
		        scanf("%f", &x );
		        scanf("%f", &y);
		        scanf("%f", &z);
		        scanf("%f", &sphere_radius);
		        scanf("%d", &atom_type);
			VSET( center, x, y, z );
			process_sphere(sphere_id, center, sphere_radius,
				atom_type);
			break;
		case (2):
			scanf("%d", &b_1);
			scanf("%d", &b_2);
			(void)make_bond( b_1, b_2);
			break;
		case (4):
			return;
		}
	}
}

void
process_sphere(id, center, rad, sph_type)
int	id;
point_t	center;
double	rad;
int	sph_type;
{
	struct sphere * new = (struct sphere *)
	    malloc( sizeof ( struct sphere) );
	char nm[128], nm1[128];
	mat_t m;
	unsigned char rgb[3];

	mat_idn( m);

	rgb[0] = atom_list[sph_type].red;
	rgb[1] = atom_list[sph_type].green;
	rgb[2] = atom_list[sph_type].blue;

	sprintf(nm, "SPH.%d", id );
	(void)mk_addmember( nm, &head, WMOP_UNION );
	sprintf(nm1, "sph.%d", id );
	mk_sph( stdout, nm1, center, rad );
	mk_comb( stdout, nm, 1, 1, matname, matparm, rgb, 0 );
	mk_memb( stdout, nm1, m, UNION);

	new->next = ( struct sphere *)0;
	new->s_id = id;
	NAMEMOVE(nm1, new->s_name);
	VMOVE( new->s_center, center );
	new->s_rad = rad;
	new->s_atom_type = sph_type;

	if ( s_head == (struct sphere *) 0 )
	{
		s_head = s_list = new;
	} else
	{
		s_list->next = new;
		s_list = new;
	}
}

make_bond( sp1, sp2 )
int sp1, sp2;
{
	struct sphere * s1, *s2, *s_ptr;
	point_t base;
	vect_t height;
	mat_t m;
	char nm[128], nm1[128];
	unsigned char rgb[3];

	mat_idn( m );

	s1 = s2 = (struct sphere *) 0;

	for( s_ptr = s_head; s_ptr != (struct sphere *)0; s_ptr = s_ptr->next )
	{
		if ( s_ptr->s_id == sp1 )
			s1 = s_ptr;
			
		if ( s_ptr->s_id == sp2 )
			s2 = s_ptr;
	}

	if( s1 == (struct sphere *) 0 || s2 == (struct sphere *)0 )
		return -1;		/* error */

	VMOVE( base, s1->s_center );
	VSUB2( height, s2->s_center, s1->s_center );

	sprintf( nm, "bond.%d.%d", sp1, sp2);
	sprintf( nm1, "BOND.%d.%d", sp1, sp2);
	(void)mk_addmember( nm1, &head, WMOP_UNION );

	rgb[0] = 191;
	rgb[1] = 142;
	rgb[2] = 57;

#if 1
	/* Use this for mol-cube.dat */
	mk_rcc( stdout, nm, base, height, s1->s_rad * 0.15 );
#else
	/* Use this for chemical molecules */
	mk_rcc( stdout, nm, base, height, s1->s_rad * 0.5 );
#endif

	mk_comb( stdout, nm1, 3, 1, matname, matparm, rgb, 0 );
	mk_memb( stdout, nm, m, UNION);
	mk_memb( stdout, s1->s_name, m, SUBTRACT);
	mk_memb( stdout, s2->s_name, m, SUBTRACT);
	return(0);		/* OK */
}
