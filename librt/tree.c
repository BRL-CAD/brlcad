/*
 *			T R E E
 *
 * Ray Tracing program, GED tree tracer.
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "db.h"
#include "dir.h"
#include "debug.h"

extern struct soltab *HeadSolid;
extern struct functab functab[];
extern long nsolids;		/* total # of solids participating */
extern long nregions;		/* total # of regions participating */

struct region *HeadRegion;

void pr_region();
void pr_tree();

static char idmap[] = {
	ID_NULL,	/* undefined, 0 */
	ID_NULL,	/*  RPP	1 axis-aligned rectangular parallelopiped */
	ID_NULL,	/* BOX	2 arbitrary rectangular parallelopiped */
	ID_NULL,	/* RAW	3 right-angle wedge */
	ID_NULL,	/* ARB4	4 tetrahedron */
	ID_NULL,	/* ARB5	5 pyramid */
	ID_NULL,	/* ARB6	6 extruded triangle */
	ID_NULL,	/* ARB7	7 weird 7-vertex shape */
	ID_NULL,	/* ARB8	8 hexahedron */
	ID_NULL,	/* ELL	9 ellipsoid */
	ID_NULL,	/* ELL1	10 another ellipsoid ? */
	ID_NULL,	/* SPH	11 sphere */
	ID_NULL,	/* RCC	12 right circular cylinder */
	ID_NULL,	/* REC	13 right elliptic sylinder */
	ID_NULL,	/* TRC	14 truncated regular cone */
	ID_NULL,	/* TEC	15 truncated elliptic cone */
	ID_TOR,		/* TOR	16 toroid */
	ID_NULL,	/* TGC	17 truncated general cone */
	ID_TGC,		/* GENTGC 18 supergeneralized TGC; internal form */
	ID_ELL,		/* GENELL 19: V,A,B,C */
	ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
	ID_ARS,		/* ARS 21: arbitrary triangular-surfaced polyhedron */
	ID_NULL,	/* ARSCONT 22: extension record type for ARS solid */
	ID_NULL		/* n+1 */
};

union tree *drawHobj();
static char *path_str();

get_tree(node)
char *node;
{
	register union tree *curtree;
	register struct directory *dp;
	mat_t	mat;

	mat_idn( mat );

	dp = dir_lookup( node, LOOKUP_NOISY );
	if( dp == DIR_NULL )
		return;		/* ERROR */

	curtree = drawHobj( dp, REGION_NULL, 0, mat );
	if( curtree != TREE_NULL )  {
		/*  Subtree has not been contained by a region.
		 *  This should only happen when a top-level solid
		 *  is encountered.  Build a special region for it.
		 */
		register struct region *regionp;	/* XXX */
		GETSTRUCT( regionp, region );
		regionp->reg_active = REGION_NULL;
		printf("Warning:  Top level solid, region %s created\n",
			path_str(0) );
		if( curtree->tr_op != OP_SOLID )
			bomb("root subtree not Solid");
		curtree->tr_stp->st_regionp = regionp;
		regionp->reg_treetop = curtree;
		regionp->reg_name = strdup(path_str(0));
		regionp->reg_forw = HeadRegion;
		HeadRegion = regionp;
		nregions++;
	}

	if( debug & DEBUG_REGIONS )  {
		register struct region *rp = HeadRegion;	/* XXX */

		printf("printing regions\n");
		while( rp != REGION_NULL )  {
			pr_region( rp );
			rp = rp->reg_forw;
		}
	}
}

/*static */
struct soltab *
add_solid( rec, name, mat, regp )
union record *rec;
char	*name;
matp_t	mat;
struct region *regp;
{
	register struct soltab *stp;
	static vect_t v[8];
	FAST fastf_t f;

	/* Eliminate any [15] scaling */
	f = mat[15];
	if( NEAR_ZERO(f) )  {
		printf("solid %s:  ", name );
		mat_print("defective matrix", mat);
	} else if( f != 1.0 )  {
		f = 1.0 / f;
		mat[0] *= f;
		mat[5] *= f;
		mat[10]*= f;
		mat[15] = 1;
	}
	fastf_float( v, rec->s.s_values, 8 );

	GETSTRUCT(stp, soltab);
	stp->st_id = idmap[rec->s.s_type];	/* PUN for a.a_type, too */
	stp->st_name = name;
	stp->st_specific = (int *)0;
	stp->st_regionp = regp;

	if( functab[stp->st_id].ft_prep( v, stp, mat, &(rec->s) ) )  {
		/* Error, solid no good */
		free(stp);
		return( SOLTAB_NULL );		/* continue */
	}

	/* For now, just link them all onto the same list */
	stp->st_forw = HeadSolid;
	HeadSolid = stp;

	if(debug&DEBUG_SOLIDS)  {
		printf("-------------- %s -------------\n", stp->st_name);
		VPRINT("Bound Sph CENTER", stp->st_center);
		printf("Bound Sph Rad**2 = %f\n", stp->st_radsq);
		if( regp != REGION_NULL )
			printf("Member of region %s\n", regp->reg_name );
		functab[stp->st_id].ft_print( stp );
	}
	nsolids++;
	return( stp );
}

#define	MAXLEVELS	8
struct directory	*path[MAXLEVELS];	/* Record of current path */

static mat_t xmat;				/* temporary fastf_t matrix */

/*
 *			D R A W H O B J
 *
 * This routine is used to get an object drawn.
 * The actual processing of solids is performed by add_solid(),
 * but all transformations and region building is done here.
 */
/*static*/
union tree *
drawHobj( dp, argregion, pathpos, old_xlate )
struct directory *dp;
struct region *argregion;
matp_t old_xlate;
{
	auto struct directory *nextdp;	/* temporary */
	auto mat_t new_xlate;		/* Accumulated xlation matrix */
	auto union record rec;		/* local copy of this record */
	auto long savepos;
	auto int nparts;		/* Number of sub-parts to this comb */
	auto int i;
	auto union tree *curtree;	/* ptr to current tree top */
	auto union tree *subtree;	/* ptr to subtree passed up */
	auto struct region *regionp;

	if( pathpos >= MAXLEVELS )  {
		(void)printf("%s: nesting exceeds %d levels\n",
			path_str(MAXLEVELS), MAXLEVELS );
		return(TREE_NULL);
	}
	path[pathpos] = dp;

	/* Routine is recursive:  save file pointer for restoration. */
	savepos = lseek( ged_fd, 0L, 1 );

	/*
	 * Load the record into local record buffer
	 */
	(void)lseek( ged_fd, dp->d_addr, 0 );
	(void)read( ged_fd, (char *)&rec, sizeof rec );

	if( rec.u_id == SOLID || rec.u_id == ARS_A )  {
		register struct soltab *stp;		/* XXX */

		/* Draw a solid */
		stp = add_solid( &rec.s, strdup(path_str(pathpos)), old_xlate, argregion );
		(void)lseek( ged_fd, savepos, 0);	/* restore pos */
		if( stp == SOLTAB_NULL )
			return( TREE_NULL );

		/**GETSTRUCT( curtree, union tree ); **/
		curtree = (union tree *)malloc(sizeof(union tree));
		if( curtree == TREE_NULL )
			bomb("drawHobj: curtree malloc failed\n");
		bzero( (char *)curtree, sizeof(union tree) );
		curtree->tr_op = OP_SOLID;
		curtree->tr_stp = stp;
		return( curtree );
	}

	if( rec.u_id != COMB )  {
		(void)printf("drawobj:  defective input '%c'\n", rec.u_id );
		return(TREE_NULL);			/* ERROR */
	}

	/* recurse down through a combination (directory) node */
	regionp = argregion;
	if( rec.c.c_flags == 'R' )  {
		if( argregion != REGION_NULL )  {
			printf("Warning:  region %s within region %s (ignored)\n",
				path_str(pathpos), argregion->reg_name );
		} else {
			/* Start a new region here */
			GETSTRUCT( regionp, region );
			regionp->reg_forw = regionp->reg_active = REGION_NULL;
		}
	}
	curtree = TREE_NULL;
	nparts = rec.c.c_length;		/* save in an auto var */
	for( i=0; i<nparts; i++ )  {
		(void)read(ged_fd, (char *)&rec, sizeof rec );
		nextdp = dir_lookup( rec.M.m_instname, LOOKUP_NOISY );
		if( nextdp == DIR_NULL )
			continue;
		if( rec.M.m_brname[0] != '\0' )  {
			register struct directory *tdp;	/* XXX */
			/*
			 * Create an alias.  First step towards full
			 * branch naming.  User is responsible for his
			 * branch names being unique.
			 */
			tdp = dir_lookup(rec.M.m_brname, LOOKUP_QUIET);
			if( tdp != DIR_NULL )
				nextdp = tdp; /* use existing alias */
			else
				nextdp=dir_add(rec.M.m_brname,nextdp->d_addr);
		}
		{
			register int i;
			for( i=0; i<4*4; i++ )
				xmat[i] = rec.M.m_mat[i];/* cvt to fastf_t */
		}
		mat_mul(new_xlate, old_xlate, xmat);

		/* Recursive call */
		subtree = drawHobj( nextdp, regionp, pathpos+1, new_xlate );
		if( subtree == TREE_NULL )
			continue;	/* no valid subtree, keep on going */
		if( regionp == REGION_NULL )  {
			/*
			 * Found subtree that is not contained in a region;
			 * invent a region to hold it.  Reach down into
			 * subtree and "fix" first solid's region pointer.
			 */
			GETSTRUCT( regionp, region );
			regionp->reg_forw = regionp->reg_active = REGION_NULL;
			printf("Warning:  Forced to create region %s\n",
				path_str(pathpos) );
			if(subtree->tr_op != OP_SOLID )
				bomb("subtree not Solid");
			subtree->tr_stp->st_regionp = regionp;
		}

		if( curtree == TREE_NULL )  {
			curtree = subtree;
		} else {
			register union tree *xtp;	/* XXX */
			/** GETSTRUCT( xtp, union tree ); **/
			xtp=(union tree *)malloc(sizeof(union tree));
			if( xtp == TREE_NULL )
				bomb("drawHobj: xtp malloc failed\n");
			bzero( (char *)xtp, sizeof(union tree) );
			xtp->tr_left = curtree;
			xtp->tr_right = subtree;
			switch( rec.M.m_relation )  {
			default:
				printf("(%s) bad m_relation '%c'\n",
					path_str(pathpos), rec.M.m_relation );
				/* FALL THROUGH */
			case UNION:
				xtp->tr_op = OP_UNION; break;
			case SUBTRACT:
				xtp->tr_op = OP_SUBTRACT; break;
			case INTERSECT:
				xtp->tr_op = OP_INTERSECT; break;
			}
			curtree = xtp;
		}
	}
	if( curtree != TREE_NULL )  {
		if( regionp == REGION_NULL )  {
			printf("drawHobj: (%s) null regionp, non-null curtree\n", path_str(pathpos) );
		} else if( argregion == REGION_NULL )  {
			/* Region began at this level */
			regionp->reg_treetop = curtree;
			regionp->reg_name = strdup(path_str(pathpos));
			regionp->reg_forw = HeadRegion;
			HeadRegion = regionp;
			nregions++;
			if( debug & DEBUG_REGIONS )
				printf("Add Region %s\n", regionp->reg_name);
			curtree = TREE_NULL;
		}
	} else {
		/* Null result tree, release region struct */
		if( argregion == REGION_NULL && regionp != REGION_NULL )
			free( regionp );
	}

	/* Clean up and return */
	(void)lseek( ged_fd, savepos, 0 );
	return( curtree );
}

static char *
path_str( pos )
int pos;
{
	static char line[256];
	register char *cp = &line[0];
	register int i;

	line[0] = '/';
	line[1] = '\0';
	for( i=0; i<=pos; i++ )  {
		(void)sprintf( cp, "/%s%c", path[i]->d_namep, '\0' );
		cp += strlen(cp);
	}
	return( &line[0] );
}

void
pr_region( rp )
register struct region *rp;
{
	printf("REGION %s:\n", rp->reg_name );
	printf("id=%d, air=%d, material=%d, los=%d\n",
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_material, rp->reg_los );
	pr_tree( rp->reg_treetop, 0 );
	printf("\n");
}

void
pr_tree( tp, lvl )
register union tree *tp;
int lvl;			/* recursion level */
{
	register int i;

	printf("%.8x ", tp);
	for( i=3*lvl; i>0; i-- )
		putchar(' ');	/* indent */

	if( tp == TREE_NULL )  {
		printf("Null???\n");
		return;
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		printf("SOLID %s (bin %d)\n",
			tp->tr_stp->st_name,
			tp->tr_stp->st_bin );
		return;

	default:
		printf("Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		printf("UNION\n");
		break;
	case OP_INTERSECT:
		printf("INTERSECT\n");
		break;
	case OP_SUBTRACT:
		printf("MINUS\n");
		break;
	}
	/* BINARY TYPE */
	pr_tree( tp->tr_left, lvl+1 );
	pr_tree( tp->tr_right, lvl+1 );
}

/* Convert TO 4xfastf_t FROM 3xfloats (for database)  */
fastf_float( ff, fp, n )
register fastf_t *ff;
register float *fp;
register int n;
{
	while( n-- )  {
		VMOVE( ff, fp );
		fp += 3;
		ff += ELEMENTS_PER_VECT;
	}
}
