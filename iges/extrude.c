/*
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#include "./iges_struct.h"
#include "./iges_extern.h"

extrude( entityno )
int entityno;
{ 

	fastf_t		length;			/* extrusion length */
	vect_t		edir;			/* a unit vector (direction of extrusion */
	vect_t		evect;			/* Scaled vector for extrusion */
	int		sol_num;		/* IGES solid type number */
	int		curve;			/* pointer to directory entry for base curve */
	struct ptlist	*curv_pts;		/* List of points along curve */
	point_t		crvmin,crvmax;		/* Bounding box for curve */
	int		i;

	/* Default values */
	VSET( edir , 0.0 , 0.0 , 1.0 );


	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );

	/* Read pointer to directory entry for curve to be extruded */

	Readint( &curve , "" );

	/* Convert this to a "dir" index */

	curve = (curve-1)/2;

	Readcnv( &length , "" );
	Readflt( &edir[X] , "" );
	Readflt( &edir[Y] , "" );
	Readflt( &edir[Z] , "" );

	if( length <= 0.0 )
	{
		rt_log( "Illegal parameters for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	/*
	 * Unitize direction vector
	 */

	VUNITIZE(edir);

	/* Scale vector */

	VSCALE(evect , edir, length);

	/* Switch based on type of curve to be extruded */

	switch( dir[curve]->type )
	{
		case 100:	/* circular arc */
			return( Extrudcirc( entityno , curve , evect ) );
		case 104:	/* conic arc */
			return( Extrudcon( entityno , curve , evect ) );
		case 102:	/* composite curve */
		case 106:	/* copius data */
		case 112:	/* parametric spline */
		case 126:	/* B-spline */
		{
			int npts,done,nverts;
			/* normal_order indicates how to calculate
			   outward normals:
				1 => (pt->next - pt) X extrude_vector
				2 => opposite direction		*/
			int normal_order;
			struct ptlist *ptr,*ptr2;
			vect_t v1,v2,v3;
			point_t verts[4],tmpv[4];

			npts = Getcurve( curve , &curv_pts );
			if( npts == 0 )
				return( 0 );

			/* Find bounding box for curve */
			crvmin[X] = INFINITY;
			crvmin[Y] = INFINITY;
			crvmin[Z] = INFINITY;
			for( i=0 ; i<3 ; i++ )
				crvmax[i] = (-crvmin[i]);
			ptr = curv_pts;
			while( ptr != NULL )
			{
				VMINMAX( crvmin , crvmax , ptr->pt );
				ptr = ptr->next;
			}

			ptr = curv_pts;
			while( ptr->pt[X] != crvmin[X] )
				ptr = ptr->next;
			if( ptr->next != NULL )
			{
				VSUB2( v1 , ptr->pt , ptr->next->pt );
			}
			else
			{
				VSUB2( v1 , ptr->pt , curv_pts->pt );
			}
			VCROSS( v2 , v1 , edir );
			if( v2[X] < 0.0 )
				normal_order = 1;
			else
				normal_order = 2;

			/* Make polysolid header */
			mk_polysolid( fdout , dir[entityno]->name );

			/* loop through curve constructing sides
				each piece will be a 4 sided rectangle */
			ptr = curv_pts;
			while( ptr->next != NULL )
			{
				if( normal_order == 2 )
				{
					VMOVE( verts[0] , ptr->pt );
					VMOVE( verts[1] , ptr->next->pt );
					VADD2( verts[2] , verts[1] , evect );
					VADD2( verts[3] , verts[0] , evect );
				}
				else
				{
					VMOVE( verts[0] , ptr->next->pt );
					VMOVE( verts[1] , ptr->pt );
					VADD2( verts[2] , verts[1] , evect );
					VADD2( verts[3] , verts[0] , evect );
				}
				mk_fpoly( fdout , 4 , verts );
				ptr = ptr->next;
			}

			/* make top and bottom polygons */

			ptr = curv_pts;
			ptr2 = ptr;
			while( ptr2->next != NULL )
				ptr2 = ptr2->next;
			if( SAMEPT( ptr2->pt , ptr->pt ) )
				ptr2 = ptr2->prev;
			done = 0;
			while( !done )
			{
				nverts = 4;
				/* Make Bottom polygon */
				VMOVE( verts[0] , ptr->pt );
				VMOVE( verts[1] , ptr2->pt );
				if( SAMEPT( ptr->next->pt , ptr2->prev->pt ) )
				{
					nverts = 3;
					VMOVE( verts[2] , ptr->next->pt );
					done = 1;
				}
				else
				{
					VMOVE( verts[2] , ptr2->prev->pt );
					VMOVE( verts[3] , ptr->next->pt );
				}

				VSUB2( v1 , verts[1] , verts[0] );
				VSUB2( v2 , verts[2] , verts[1] );
				VCROSS( v3 , v1 , v2 );
				if( VDOT( v3 , edir ) > 0.0 )
				{
					for( i=0 ; i<nverts ; i++ )
					{
						VMOVE( tmpv[i] , verts[nverts-1-i] );
					}
					for( i=0 ; i<nverts ; i++ )
					{
						VMOVE( verts[i] , tmpv[i] );
					}
				}
				mk_fpoly( fdout , nverts , verts );

				/* Make corresponding top polygon */
				for( i=0 ; i<nverts ; i++ )
				{
					VADD2( verts[i] , verts[i] , evect );
				}
				for( i=0 ; i<nverts ; i++ )
				{
					VMOVE( tmpv[i] , verts[nverts-1-i] );
				}
				for( i=0 ; i<nverts ; i++ )
				{
					VMOVE( verts[i] , tmpv[i] );
				}
				mk_fpoly( fdout , nverts , verts );
				ptr = ptr->next;
				ptr2 = ptr2->prev;
				if( SAMEPT( ptr->pt , ptr2->pt ) )
					done = 1;
				if( SAMEPT( ptr->next->pt , ptr2->pt ) )
					done = 1;
			}
			return( 1 );
		}
		default:
			i = (-1);
			while( dir[curve]->type != typecount[++i].type && i < ntypes );
			rt_log( "Extrusions of %s are not allowed\n" , typecount[i].name );
			break;
	}
	return( 0 );
		

}
