/*		T E A . C
 *
 * Convert the Utah Teapot description from the IEEE CG&A database to the
 * BRL-CAD spline format. (Note that this has the closed bottom)
 *
 */

/* Header files which are used for this example */

#include "conf.h"

#include <stdio.h>		/* Direct the output to stdout */
#include "machine.h"		/* BRLCAD specific machine data types */
#include "db.h"			/* BRLCAD data base format */
#include "vmath.h"		/* BRLCAD Vector macros */
#include "nurb.h"		/* BRLCAD Spline data structures */
#include "raytrace.h"
#include "../librt/debug.h"	/* rt_g.debug flag settings */

#include "./tea.h"		/* IEEE Data Structures */
#include "./ducks.h"		/* Teapot Vertex data */
#include "./patches.h"		/* Teapot Patch data */

extern dt ducks[DUCK_COUNT];		/* Vertex data of teapot */
extern pt patches[PATCH_COUNT];		/* Patch data of teapot */

char *Usage = "This program ordinarily generates a database on stdout.\n\
	Your terminal probably wouldn't like it.";

void dump_patch();

main(argc, argv) 			/* really has no arguments */
int argc; char *argv[];
{
	char * id_name = "Spline Example";
	char * tea_name = "UtahTeapot";
	int i;

	rt_uniresource.re_magic = RESOURCE_MAGIC;

	if (isatty(fileno(stdout))) {
		(void)fprintf(stderr, "%s: %s\n", *argv, Usage);
		return(-1);
	}

	while ((i=getopt(argc, argv, "d")) != EOF) {
		switch (i) {
		case 'd' : rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL; break;
		default	:
			(void)fprintf(stderr,
				"Usage: %s [-d] > database.g\n", *argv);
			return(-1);
		}
	}

	/* Setup information 
   	 * Database header record
	 *	File name
	 * B-Spline Solid record
	 * 	Name, Number of Surfaces, resolution (not used)
	 *
	 */

	mk_id( stdout, id_name);
	mk_bsolid( stdout, tea_name, PATCH_COUNT, 1.0);

	/* Step through each patch and create a B_SPLINE surface
	 * representing the patch then dump them out.
	 */

	for( i = 0; i < PATCH_COUNT; i++)
	{
		dump_patch( patches[i] );
	}
	return(0);
}

/* IEEE patch number of the Bi-Cubic Bezier patch and convert it
 * to a B-Spline surface (Bezier surfaces are a subset of B-spline surfaces
 * and output it to a BRLCAD binary format.
 */
void
dump_patch( patch )
pt patch;
{
	struct face_g_snurb * b_patch;
	int i,j, pt_type;
	fastf_t * mesh_pointer;

	/* U and V parametric Direction Spline parameters
	 * Cubic = order 4, 
	 * knot size is Control point + order = 4
	 * control point size is 4
	 * point size is 3
	 */

	pt_type = RT_NURB_MAKE_PT_TYPE(3, 2,0); /* see nurb.h for details */

	b_patch = (struct face_g_snurb *) rt_nurb_new_snurb( 4, 4, 8, 8, 4, 4,
		pt_type, &rt_uniresource);
	
	/* Now fill in the pieces */

	/* Both u and v knot vectors are [ 0 0 0 0 1 1 1 1] 
	 * spl_kvknot( order, lower parametric value, upper parametric value,
	 * 		Number of interior knots )
 	 */
	

	rt_free((char *)b_patch->u.knots, "dumping u knots I'm about to realloc");
	rt_nurb_kvknot( &b_patch->u, 4, 0.0, 1.0, 0, &rt_uniresource);

	rt_free((char *)b_patch->v.knots, "dumping v_kv knots I'm about to realloc");
	rt_nurb_kvknot( &b_patch->v, 4, 0.0, 1.0, 0, &rt_uniresource);

	if (rt_g.debug) {
		rt_ck_malloc_ptr(b_patch, "b_patch");
		rt_ck_malloc_ptr(b_patch->u.knots,
			"b_patch->u.knots");
		rt_ck_malloc_ptr(b_patch->v.knots,
			"b_patch->v.knots");
	}

	/* Copy the control points */

	mesh_pointer = b_patch->ctl_points;

	for( i = 0; i< 4; i++)
	for( j = 0; j < 4; j++)
	{
		*mesh_pointer = ducks[patch[i][j]-1].x * 1000;
		*(mesh_pointer+1) = ducks[patch[i][j]-1].y * 1000;
		*(mesh_pointer+2) = ducks[patch[i][j]-1].z * 1000;
		mesh_pointer += 3;
	}

	/* Output the the b_spline through the libwdb interface */
	mk_bsurf( stdout, b_patch);
}
