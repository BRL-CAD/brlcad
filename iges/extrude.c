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
			int npts;
			struct model *m;
			struct nmgregion *r;
			struct shell *s;
			struct faceuse *fu;
			struct loopuse *lu;
			struct edgeuse *eu;
			struct ptlist *pt_ptr;

			npts = Getcurve( curve , &curv_pts );
			if( npts < 3 )
				return( 0 );


			m = nmg_mm();
			r = nmg_mrsv( m );
			s = BU_LIST_FIRST( shell, &r->s_hd );

			fu = nmg_cface( s, (struct vertex **)NULL, npts-1 );
			pt_ptr = curv_pts;
			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct vertex *v;

				v = eu->vu_p->v_p;
				nmg_vertex_gv( v, pt_ptr->pt );
				pt_ptr = pt_ptr->next;
			}

			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "Extrude: Failed to calculate face geometry\n" );
				nmg_km( m );
				rt_free( (char *)curv_pts, "curve_pts" );
				return( 0 );
			}

			if( nmg_extrude_face( fu, evect , &tol ) )
			{
				rt_log( "Extrude: extrusion failed\n" );
				nmg_km( m );
				rt_free( (char *)curv_pts, "curve_pts" );
				return( 0 );
			}

			write_shell_as_polysolid( fdout, dir[entityno]->name, s );
			nmg_km( m );
			rt_free( (char *)curv_pts, "curve_pts" );

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
