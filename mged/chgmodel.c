/*
 *			C H G M O D E L
 *
 * Functions -
 *
 * The U. S. Army Ballistic Research Laboratory
 */

#include	<math.h>
#include	<signal.h>
#include	<stdio.h>
#include "ged_types.h"
#include "3d.h"
#include "sedit.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "dm.h"
#include "vmath.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();

int		newedge;		/* new edge for arb editing */

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

static void	aexists();

/* Rename an object */
/* Format: n oldname newname	*/
void
f_name()
{
	register struct directory *dp;
	union record record;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	dp->d_namep = strdup( cmd_args[2] );
	db_getrec( dp, &record, 0 );

	NAMEMOVE( cmd_args[2], record.c.c_name );
	db_putrec( dp, &record, 0 );
	(void)printf("done\n");
}

/* Copy a solid */
/* Format: c oldname newname	*/
void
f_copy()
{
	register struct directory *proto;
	register struct directory *dp;
	union record record;
	int i, ngran;

	if( (proto = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}

	db_getrec( proto, &record, 0 );

	if( record.u_id != ID_SOLID && record.u_id != ID_ARS_A ) {
		(void)printf("%s: not a solid\n", proto->d_namep );
		return;
	}

	/*
	 * Update the in-core directory
	 */
	if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, 0 )) == DIR_NULL )
		return;
	db_alloc( dp, proto->d_len );

	/*
	 * Update the disk record
	 */
	if(record.u_id == ID_ARS_A)  {
		NAMEMOVE( cmd_args[2], record.a.a_name );
		ngran = record.a.a_totlen;
		db_putrec( dp, &record, 0 );

		/* Process the rest of the ARS (b records)  */
		for( i = 0; i < ngran; i++ )  {
			db_getrec( proto, &record, i+1 );
			if( i == 0 )  {
				record.b.b_values[0] = -toViewcenter[MDX];
				record.b.b_values[1] = -toViewcenter[MDY];
				record.b.b_values[2] = -toViewcenter[MDZ];
			}
			db_putrec( dp, &record, i+1 );
		}
	}  else  {
		NAMEMOVE( cmd_args[2], record.s.s_name );
		record.s.s_values[0] = -toViewcenter[MDX];
		record.s.s_values[1] = -toViewcenter[MDY];
		record.s.s_values[2] = -toViewcenter[MDZ];
		db_putrec( dp, &record, 0 );
	}
	(void)printf("done\n");
}

/* Create an instance of something */
/* Format: i object combname instname [op]	*/
void
f_instance()
{
	register struct directory *dp;
	char oper;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	oper = UNION;
	if( numargs == 5 )
		oper = cmd_args[4][0];
	if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
		(void)printf("bad operation: %c\n", oper );
		return;
	}
	if( combadd( dp, cmd_args[2], cmd_args[3], '\0', oper, 0, 0 ) ==
	    DIR_NULL )
		return;
	(void)printf("done\n");
}

/* add solids to a region or create the region */
/* and then add solids */
/* Format: r regionname opr1 sol1 opr2 sol2 ... oprn soln */
void
f_region()
{
	register struct directory *dp;
	union record record;
	int i;
	int ident, air;
	char oper;

	ident = air = 0;
	/* Check for even number of arguments */
	if( numargs & 01 )  {
		printf("error in number of args!\n");
		return;
	}
	/* Get operation and solid name for each solid */
	for( i = 2; i < numargs; i += 2 )  {
		if( cmd_args[i][1] != '\0' )  {
			(void)printf("bad operation: %s skip member: %s\n",
				cmd_args[i], cmd_args[i+1] );
			continue;
		}
		oper = cmd_args[i][0];
		if( (dp = lookup( cmd_args[i + 1], NOISY )) == DIR_NULL )  {
			(void)printf("skipping %s\n", cmd_args[i + 1] );
			continue;
		}

		if(oper != UNION && oper != SUBTRACT &&	oper != INTERSECT) {
			(void)printf("bad operation: %c skip member: %s\n",
				oper, dp->d_namep );
			continue;
		}

		db_getrec( dp, &record, 0 );
		if( record.u_id == ID_COMB ) {
			if( record.c.c_flags == 'R' ) {
				(void)printf(
				     "Note: %s is a region\n",
				     dp->d_namep );
			}
		}

		if( combadd( dp, cmd_args[1], (char *)NULL, 'r', oper, ident,
							air ) == DIR_NULL )  {
			(void)printf("error in combadd\n");
			return;
		}
	}
	(void)printf("done\n");
}

/* Add/modify item and air codes of a region */
/* Format: I region item <air>	*/
void
f_itemair()
{
	register struct directory *dp;
	int ident, air;
	union record record;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	air = ident = 0;
	ident = atoi( cmd_args[2] );

	/* If <air> is not included, it is assumed to be zero */
	if( numargs == 4 )  {
		air = atoi( cmd_args[3] );
	}
	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( record.c.c_flags != 'R' ) {
		(void)printf("%s: not a region\n", dp->d_namep );
		return;
	}
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	db_putrec( dp, &record, 0 );
	(void)printf("done\n");
}

/* Add/modify material code and los percent of a region */
/* Format: M region mat los	*/
void
f_modify()
{
	register struct directory *dp;
	register int mat, los;
	union record record;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	mat = los = 0;
	mat = atoi( cmd_args[2] );
	los = atoi( cmd_args[3] );
	/* Should check that los is in valid range */
	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB )  {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( record.c.c_flags != 'R' )  {
		(void)printf("%s: not a region\n", dp->d_namep );
		return;
	}
	record.c.c_material = mat;
	record.c.c_los = los;
	db_putrec( dp, &record, 0 );
	(void)printf("done\n");
}

/* Remove an object or several from the description */
/* Format: k object1 object2 .... objectn	*/
void
f_kill()
{
	register struct directory *dp;
	register int i;

	for( i = 1; i < numargs; i++ )  {
		if( (dp = lookup( cmd_args[i], NOISY )) != DIR_NULL )  {
			eraseobj( dp );
			db_delete( dp );
			dir_delete( dp );
		}
	}
	dmaflag = 1;
	(void)printf("done\n");
}

/* Grouping command */
/* Format: g groupname object1 object2 .... objectn	*/
void
f_group()
{
	register struct directory *dp;
	register int i;

	/* get objects to add to group */
	for( i = 2; i < numargs; i++ )  {
		if( (dp = lookup( cmd_args[i], NOISY)) != DIR_NULL )  {
			if( combadd( dp, cmd_args[1], (char *)NULL, 'g',
				UNION, 0, 0) == DIR_NULL )
				return;
		}
		else
			(void)printf("skip member %s\n", cmd_args[i]);
	}
	(void)printf("done\n");
}

/* Mirror image */
/* Format: m oldsolid newsolid axis	*/
void
f_mirror()
{
	register struct directory *dp;
	union record record;
	struct directory *proto;
	int i, j, k, ngran;

	if( (proto = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}
	k = -1;
	if( strcmp( cmd_args[3], "x" ) == 0 )
		k = 0;
	if( strcmp( cmd_args[3], "y" ) == 0 )
		k = 1;
	if( strcmp( cmd_args[3], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		(void)printf("axis must be x, y or z\n");
		return;
	}

	db_getrec( proto, &record, 0 );
	if( record.u_id != ID_SOLID && record.u_id != ID_ARS_A )  {
		(void)printf("%s: not a solid\n", dp->d_namep );
		return;
	}
	if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, 0 )) == DIR_NULL )
		return;
	db_alloc( dp, proto->d_len );

	/* create mirror image */
	if( record.u_id == ID_ARS_A )  {
		NAMEMOVE( cmd_args[2], record.a.a_name );
		ngran = record.a.a_totlen;
		db_putrec( dp, &record, 0 );
		for( i = 0; i < ngran; i++ )  {
			db_getrec( proto, &record, i+1 );
			for( j = k; j < 24; j += 3 )
				record.b.b_values[j] *= -1.0;
			db_putrec( dp, &record, i+1 );
		}

	} else  {
		for( i = k; i < 24; i += 3 )
			record.s.s_values[i] *= -1.0;
		NAMEMOVE( cmd_args[2], record.s.s_name );
		db_putrec( dp, &record, 0 );
	}
	if( no_memory )  {
		(void)printf(
		"Mirror image (%s) created but NO memory left to draw it\n",
			cmd_args[2] );
		return;
	}
	drawHobj( dp, ROOT, 0, identity );
	dmaflag = 1;
	(void)printf("done\n");
}

/* Face command - project an arb face */
/* Format: f face distance	*/
void
f_face()
{
	static int i, j, face, pt[4], prod;
	static float dist;
	static vect_t work;

	if( state != ST_S_EDIT )  {
		state_err( "Face Project" );
		return;
	}
	if( es_gentype != GENARB8 )  {
		(void)printf("ERROR: solid type must be generalized arb8\n");
		return;
	}
	face = atoi( cmd_args[1] );
	if( face < 1000 || face > 9999 ) {
		(void)printf("ERROR: face must be 4 points\n");
		return;
	}
	/* get distance to project face */
	dist = atof( cmd_args[2] );
	newedge = 1;

	/* convert to point notation */
	VMOVE( &work[0], &es_rec.s.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VADD2(&es_rec.s.s_values[i], &es_rec.s.s_values[i], work);
	}

	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);

	/* user can input face in any order - will use product of
	 * face points to distinguish faces:
	 *    product       face
	 *       24         1234
	 *     1680         5678
	 *      252         2367
	 *      160         1548
	 *      672         4378
	 *       60         1256
	 */
	prod = 1;
	for( i = 0; i <= 3; i++ )  {
		prod *= pt[i];
		--pt[i];
		if( pt[i] > 7 )  {
			(void)printf("bad face: %d\n",face);
			return;
		}
	}
	/* find plane containing this face */
	if( (j = plane( pt[0], pt[1], pt[2], pt[3], &es_rec.s )) >= 0 )  {
		(void)printf("face: %d is not a plane\n",face);
		return;
	}
	/* get normal vector of length == dist */
	for( i = 0; i < 3; i++ )
		es_plant[i] *= dist;

	/* protrude the selected face */
	switch( prod )  {

	case 24:   /* protrude face 1234 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &es_rec.s.s_values[j*3],
				&es_rec.s.s_values[i*3],
				&es_plant[0]);
		}
		break;

	case 1680:   /* protrude face 5678 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &es_rec.s.s_values[i*3],
				&es_rec.s.s_values[j*3],
				&es_plant[0] );
		}
		break;

	case 60:   /* protrude face 1256 */
		VADD2( &es_rec.s.s_values[9],
			&es_rec.s.s_values[0],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[6],
			&es_rec.s.s_values[3],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[21],
			&es_rec.s.s_values[12],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[18],
			&es_rec.s.s_values[15],
			&es_plant[0] );
		break;

	case 672:   /* protrude face 4378 */
		VADD2( &es_rec.s.s_values[0],
			&es_rec.s.s_values[9],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[3],
			&es_rec.s.s_values[6],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[15],
			&es_rec.s.s_values[18],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[12],
			&es_rec.s.s_values[21],
			&es_plant[0] );
		break;

	case 252:   /* protrude face 2367 */
		VADD2( &es_rec.s.s_values[0],
			&es_rec.s.s_values[3],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[9],
			&es_rec.s.s_values[6],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[12],
			&es_rec.s.s_values[15],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[21],
			&es_rec.s.s_values[18],
			&es_plant[0] );
		break;

	case 160:   /* protrude face 1548 */
		VADD2( &es_rec.s.s_values[3],
			&es_rec.s.s_values[0],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[15],
			&es_rec.s.s_values[12],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[6],
			&es_rec.s.s_values[9],
			&es_plant[0] );
		VADD2( &es_rec.s.s_values[18],
			&es_rec.s.s_values[21],
			&es_plant[0] );
		break;

	default:
		(void)printf("bad face: %d\n", face );
		return;
	}

	/* draw the new solid */
	illump = redraw( illump, &es_rec );

	/* Update display information */
	pr_solid( &es_rec.s );
	dmaflag = 1;
}

/* Delete members of a combination */
/* Format: D comb memb1 memb2 .... membn	*/
void
f_delmem()
{
	register struct directory *dp;
	register int i, rec;
	union record record;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	/* Examine all the Member records, one at a time */
	for( rec = 1; rec < dp->d_len; rec++ )  {
		db_getrec( dp, &record, rec );
top:
		/* Compare this member to each command arg */
		for( i = 2; i < numargs; i++ )  {
			if( strcmp( cmd_args[i], record.M.m_instname ) != 0 &&
			    strcmp( cmd_args[i], record.M.m_brname ) != 0 )
				continue;
			printf("deleting member %s\n", cmd_args[i] );
			db_getrec( dp, &record, dp->d_len-1 );	/* last one */
			db_putrec( dp, &record, rec );		/* xch */
			db_trunc( dp, 1 );
			goto top;
		}
	}
	(void)printf("done\n");
}

/* define an arb8 using rot fb angles to define a face */
/* Format: a name rot fb	*/
void
f_arbdef()
{
	register struct directory *dp;
	union record record;
	int i, j;
	float rota, fb;
	vect_t	norm;

	if( lookup( cmd_args[1] , QUIET ) != DIR_NULL )  {
		aexists( cmd_args[1] );
		return;
	}

	/* get rotation angle */
	rota = atof( cmd_args[2] ) * degtorad;

	/* get fallback angle */
	fb = atof( cmd_args[3] ) * degtorad;

	/* copy arb8 to the new name */
	if( (dp = lookup( "arb8", NOISY )) == DIR_NULL )
		return;
	db_getrec( dp, &record, 0 );
	if( (dp = dir_add( cmd_args[1], -1, DIR_SOLID, 1 )) == DIR_NULL )
		return;
	db_alloc( dp, 1 );
	NAMEMOVE( cmd_args[1], record.s.s_name );

	/* put vertex of new solid at center of screen */
	record.s.s_values[0] = -toViewcenter[MDX];
	record.s.s_values[1] = -toViewcenter[MDY];
	record.s.s_values[2] = -toViewcenter[MDZ];

	/* calculate normal vector (length = .5) defined by rot,fb */
	norm[0] = cos(fb) * cos(rota) * -0.5;
	norm[1] = cos(fb) * sin(rota) * -0.5;
	norm[2] = sin(fb) * -0.5;

	for( i = 3; i < 24; i++ )
		record.s.s_values[i] = 0.0;

	/* find two perpendicular vectors which are perpendicular to norm */
	j = 0;
	for( i = 0; i < 3; i++ )  {
		if( fabs(norm[i]) < fabs(norm[j]) )
			j = i;
	}
	record.s.s_values[j+3] = 1.0;
	VCROSS( &record.s.s_values[9], &record.s.s_values[3], norm );
	VCROSS( &record.s.s_values[3], &record.s.s_values[9], norm );

	/* create new rpp 5x5x.5 */
	/* the 5x5 faces are in rot,fb plane */
	VUNITIZE( &record.s.s_values[3] );
	VUNITIZE( &record.s.s_values[9] );
	VADD2( &record.s.s_values[6],
		&record.s.s_values[3],
		&record.s.s_values[9] );
	VMOVE( &record.s.s_values[12], norm );
	for( i = 3; i < 12; i += 3 )  {
		j = i + 12;
		VADD2( &record.s.s_values[j], &record.s.s_values[i], norm );
	}

	/* update objfd and draw new arb8 */
	db_putrec( dp, &record, 0 );
	if( no_memory )  {
		(void)printf(
			"ARB8 (%s) created but no memory left to draw it\n",
			cmd_args[1] );
		return;
	}
	drawHobj( dp, ROOT, 0, identity );
	dmaflag = 1;
	(void)printf("done\n");
}

/* Modify Combination record information */
/* Format: edcomb combname flag item air mat los	*/
void
f_edcomb()
{
	register struct directory *dp;
	union record record;
	int ident, air, mat, los;

	if( (dp = lookup( cmd_args[1], NOISY )) == DIR_NULL )
		return;

	ident = air = mat = los = 0;
	ident = atoi( cmd_args[3] );
	air = atoi( cmd_args[4] );
	mat = atoi( cmd_args[5] );
	los = atoi( cmd_args[6] );

	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}

	if( cmd_args[2][0] == 'R' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags =' ';
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	record.c.c_material = mat;
	record.c.c_los = los;
	db_putrec( dp, &record, 0 );
	(void)printf("done\n");
}

/* tell him it already exists */
static void
aexists( name )
char	*name;
{
	(void)printf( "%s:  already exists\n", name );
}
