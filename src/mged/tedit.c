/*                         T E D I T . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file tedit.c
 *
 * Functions -
 *	f_tedit		Run text editor on numerical parameters of solid
 *	writesolid	Write numerical parameters of solid into a file
 *	readsolid	Read numerical parameters of solid from file
 *	editit		Run $EDITOR on temp file
 *
 *  Author -
 *	Michael John Muuss
 *	(Inspired by 4.2 BSD program "vipw")
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./sedit.h"
#include "./mged_dm.h"

#define LINELEN		256	/* max length of input line */
#define	DEFEDITOR	"/bin/ed"
#define V3BASE2LOCAL( _pt )	(_pt)[X]*base2local , (_pt)[Y]*base2local , (_pt)[Z]*base2local
#define V4BASE2LOCAL( _pt )	(_pt)[X]*base2local , (_pt)[Y]*base2local , (_pt)[Z]*base2local , (_pt)[W]*base2local

extern struct rt_db_internal	es_int;
extern struct rt_db_internal	es_int_orig;

static char	tmpfil[17];
#ifndef _WIN32
static char	*tmpfil_init = "/tmp/GED.aXXXXXX";
#else
static char	*tmpfil_init = "c:\\GED.aXXXXXX";
#endif

int writesolid(void), readsolid(void);
int editit(const char *file);

int
f_tedit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register int i;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help ted");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Only do this if in solid edit state */
	if( not_state( ST_S_EDIT, "Primitive Text Edit" ) )
	  return TCL_ERROR;

	strcpy(tmpfil, tmpfil_init);
#ifdef _WIN32
	(void)mktemp(tmpfil);
	i=creat(tmpfil, 0600);
#else
	i = mkstemp(tmpfil);
#endif
	if( i < 0 )
	{
	  perror(tmpfil);
	  return TCL_ERROR;
	}
	(void)close(i);

	if( writesolid() )
	{
	  (void)unlink(tmpfil);
	  return TCL_ERROR;
	}

	if( editit( tmpfil ) )
	{
		if( readsolid() )
		{
		  (void)unlink(tmpfil);
		  return TCL_ERROR;
		}

		/* Update the display */
		replot_editing_solid();
		view_state->vs_flag = 1;
		Tcl_AppendResult(interp, "done\n", (char *)NULL);
	}
	(void)unlink(tmpfil);

	return TCL_OK;
}

/* Write numerical parameters of a solid into a file */
int
writesolid(void)
{
	register int i;
	FILE *fp;

	CHECK_DBI_NULL;

	fp = fopen(tmpfil, "w");

	/* Print solid parameters, 1 vector or point per line */
	switch( es_int.idb_type )
	{
		struct rt_tor_internal *tor;
		struct rt_tgc_internal *tgc;
		struct rt_ell_internal *ell;
		struct rt_arb_internal *arb;
		struct rt_half_internal *haf;
		struct rt_grip_internal *grip;
		struct rt_rpc_internal *rpc;
		struct rt_rhc_internal *rhc;
		struct rt_epa_internal *epa;
		struct rt_ehy_internal *ehy;
		struct rt_eto_internal *eto;
		struct rt_part_internal *part;
		struct rt_superell_internal *superell;

		default:
		  Tcl_AppendResult(interp, "Cannot text edit this solid type\n", (char *)NULL);
		  (void)fclose(fp);
		  return( 1 );
		case ID_TOR:
			tor = (struct rt_tor_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL( tor->v ) );
			(void)fprintf( fp , "Normal: %.9f %.9f %.9f\n", V3BASE2LOCAL( tor->h ) );
			(void)fprintf( fp , "radius_1: %.9f\n", tor->r_a*base2local );
			(void)fprintf( fp , "radius_2: %.9f\n", tor->r_h*base2local );
			break;
		case ID_TGC:
		case ID_REC:
			tgc = (struct rt_tgc_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->v ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->h ) );
			(void)fprintf( fp , "A: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->a ) );
			(void)fprintf( fp , "B: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->b ) );
			(void)fprintf( fp , "C: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->c ) );
			(void)fprintf( fp , "D: %.9f %.9f %.9f\n", V3BASE2LOCAL( tgc->d ) );
			break;
		case ID_ELL:
		case ID_SPH:
			ell = (struct rt_ell_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL( ell->v ) );
			(void)fprintf( fp , "A: %.9f %.9f %.9f\n", V3BASE2LOCAL( ell->a ) );
			(void)fprintf( fp , "B: %.9f %.9f %.9f\n", V3BASE2LOCAL( ell->b ) );
			(void)fprintf( fp , "C: %.9f %.9f %.9f\n", V3BASE2LOCAL( ell->c ) );
			break;
		case ID_ARB8:
			arb = (struct rt_arb_internal *)es_int.idb_ptr;
			for( i=0 ; i<8 ; i++ )
				(void)fprintf( fp , "pt[%d]: %.9f %.9f %.9f\n", i+1 , V3BASE2LOCAL( arb->pt[i] ) );
			break;
		case ID_HALF:
			haf = (struct rt_half_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Plane: %.9f %.9f %.9f %.9f\n" , V4BASE2LOCAL( haf->eqn ) );
			break;
		case ID_GRIP:
			grip = (struct rt_grip_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Center: %.9f %.9f %.9f\n" , V3BASE2LOCAL( grip->center ) );
			(void)fprintf( fp , "Normal: %.9f %.9f %.9f\n" , V3BASE2LOCAL( grip->normal ) );
			(void)fprintf( fp , "Magnitude: %.9f\n" , grip->mag*base2local );
			break;
		case ID_PARTICLE:
			part = (struct rt_part_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( part->part_V ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n" , V3BASE2LOCAL( part->part_H ) );
			(void)fprintf( fp , "v radius: %.9f\n", part->part_vrad * base2local );
			(void)fprintf( fp , "h radius: %.9f\n", part->part_hrad * base2local );
			break;
		case ID_RPC:
			rpc = (struct rt_rpc_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rpc->rpc_V ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rpc->rpc_H ) );
			(void)fprintf( fp , "Breadth: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rpc->rpc_B ) );
			(void)fprintf( fp , "Half-width: %.9f\n" , rpc->rpc_r * base2local );
			break;
		case ID_RHC:
			rhc = (struct rt_rhc_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rhc->rhc_V ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rhc->rhc_H ) );
			(void)fprintf( fp , "Breadth: %.9f %.9f %.9f\n" , V3BASE2LOCAL( rhc->rhc_B ) );
			(void)fprintf( fp , "Half-width: %.9f\n" , rhc->rhc_r * base2local );
			(void)fprintf( fp , "Dist_to_asymptotes: %.9f\n" , rhc->rhc_c * base2local );
			break;
		case ID_EPA:
			epa = (struct rt_epa_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( epa->epa_V ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n" , V3BASE2LOCAL( epa->epa_H ) );
			(void)fprintf( fp , "Semi-major axis: %.9f %.9f %.9f\n" , V3ARGS( epa->epa_Au ) );
			(void)fprintf( fp , "Semi-major length: %.9f\n" , epa->epa_r1 * base2local );
			(void)fprintf( fp , "Semi-minor length: %.9f\n" , epa->epa_r2 * base2local );
			break;
		case ID_EHY:
			ehy = (struct rt_ehy_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( ehy->ehy_V ) );
			(void)fprintf( fp , "Height: %.9f %.9f %.9f\n" , V3BASE2LOCAL( ehy->ehy_H ) );
			(void)fprintf( fp , "Semi-major axis: %.9f %.9f %.9f\n" , V3ARGS( ehy->ehy_Au ) );
			(void)fprintf( fp , "Semi-major length: %.9f\n" , ehy->ehy_r1 * base2local );
			(void)fprintf( fp , "Semi-minor length: %.9f\n" , ehy->ehy_r2 * base2local );
			(void)fprintf( fp , "Dist to asymptotes: %.9f\n" , ehy->ehy_c * base2local );
			break;
		case ID_ETO:
			eto = (struct rt_eto_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n" , V3BASE2LOCAL( eto->eto_V ) );
			(void)fprintf( fp , "Normal: %.9f %.9f %.9f\n" , V3BASE2LOCAL( eto->eto_N ) );
			(void)fprintf( fp , "Semi-major axis: %.9f %.9f %.9f\n" , V3BASE2LOCAL( eto->eto_C ) );
			(void)fprintf( fp , "Semi-minor length: %.9f\n" , eto->eto_rd * base2local );
			(void)fprintf( fp , "Radius of roation: %.9f\n" , eto->eto_r * base2local );
			break;
		case ID_SUPERELL:
			superell = (struct rt_superell_internal *)es_int.idb_ptr;
			(void)fprintf( fp , "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL( superell->v ) );
			(void)fprintf( fp , "A: %.9f %.9f %.9f\n", V3BASE2LOCAL( superell->a ) );
			(void)fprintf( fp , "B: %.9f %.9f %.9f\n", V3BASE2LOCAL( superell->b ) );
			(void)fprintf( fp , "C: %.9f %.9f %.9f\n", V3BASE2LOCAL( superell->c ) );
			(void)fprintf( fp , "<n,e>: <%.9f, %.9f>\n", superell->n, superell->e);
			break;
	}
	
	(void)fclose(fp);
	return( 0 );
}

static char *
Get_next_line(FILE *fp)
{
	static char line[LINELEN];
	int i;
	int len;

	if( fgets( line , sizeof( line ) , fp ) == NULL )
		return( (char *)NULL );

	len = strlen( line );

	i = 0;
	while( i<len && line[i++] != ':' );

	if( i == len || line[i] == '\0' )
		return( (char *)NULL );

	return( &line[i] );
}

/* Read numerical parameters of solid from file */
int
readsolid(void)
{
	register int i;
	FILE *fp;
	int ret_val=0;

	CHECK_DBI_NULL;

	fp = fopen(tmpfil, "r");
	if( fp == NULL )  {
		perror(tmpfil);
		return 1;	/* FAIL */
	}

	switch( es_int.idb_type )
	{
		struct rt_tor_internal *tor;
		struct rt_tgc_internal *tgc;
		struct rt_ell_internal *ell;
		struct rt_arb_internal *arb;
		struct rt_half_internal *haf;
		struct rt_grip_internal *grip;
		struct rt_rpc_internal *rpc;
		struct rt_rhc_internal *rhc;
		struct rt_epa_internal *epa;
		struct rt_ehy_internal *ehy;
		struct rt_eto_internal *eto;
		struct rt_part_internal *part;
		struct rt_superell_internal *superell;
		char *str;
		double a,b,c,d;

		default:
		  Tcl_AppendResult(interp, "Cannot text edit this solid type\n", (char *)NULL);
		  ret_val = 1;
		  break;
		case ID_TOR:
			tor = (struct rt_tor_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tor->v , a , b , c );
			VSCALE( tor->v , tor->v , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tor->h , a , b , c );
			VUNITIZE( tor->h );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			tor->r_a = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			tor->r_h = a * local2base;
			break;
		case ID_TGC:
		case ID_REC:
			tgc = (struct rt_tgc_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->v , a , b , c );
			VSCALE( tgc->v , tgc->v , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->h , a , b , c );
			VSCALE( tgc->h , tgc->h , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->a , a , b , c );
			VSCALE( tgc->a , tgc->a , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->b , a , b , c );
			VSCALE( tgc->b , tgc->b , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->c , a , b , c );
			VSCALE( tgc->c , tgc->c , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( tgc->d , a , b , c );
			VSCALE( tgc->d , tgc->d , local2base );

			break;
		case ID_ELL:
		case ID_SPH:
			ell = (struct rt_ell_internal *)es_int.idb_ptr;

			fprintf(stderr, "ID_SPH\n");

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ell->v , a , b , c );
			VSCALE( ell->v , ell->v , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ell->a , a , b , c );
			VSCALE( ell->a , ell->a , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ell->b , a , b , c );
			VSCALE( ell->b , ell->b , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ell->c , a , b , c );
			VSCALE( ell->c , ell->c , local2base );
			break;
		case ID_ARB8:
			arb = (struct rt_arb_internal *)es_int.idb_ptr;
			for( i=0 ; i<8 ; i++ )
			{
				if( (str=Get_next_line( fp )) == NULL )
				{
					ret_val = 1;
					break;
				}
				(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
				VSET( arb->pt[i] , a , b , c );
				VSCALE( arb->pt[i] , arb->pt[i] , local2base );
			}
			break;
		case ID_HALF:
			haf = (struct rt_half_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf %lf" , &a , &b , &c , &d );
			VSET( haf->eqn , a , b , c );
			haf->eqn[W] = d * local2base;
			break;
		case ID_GRIP:
			grip = (struct rt_grip_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( grip->center , a , b , c );
			VSCALE( grip->center , grip->center , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( grip->normal , a , b , c );
			break;
		case ID_PARTICLE:
			part = (struct rt_part_internal *)es_int.idb_ptr;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( part->part_V , a , b , c );
			VSCALE( part->part_V , part->part_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( part->part_H , a , b , c );
			VSCALE( part->part_H , part->part_H , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			part->part_vrad = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			part->part_hrad = a * local2base;

			break;
		case ID_RPC:
			rpc = (struct rt_rpc_internal *)es_int.idb_ptr;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rpc->rpc_V , a , b , c );
			VSCALE( rpc->rpc_V , rpc->rpc_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rpc->rpc_H , a , b , c );
			VSCALE( rpc->rpc_H , rpc->rpc_H , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rpc->rpc_B , a , b , c );
			VSCALE( rpc->rpc_B , rpc->rpc_B , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			rpc->rpc_r = a * local2base;
			break;
		case ID_RHC:
			rhc = (struct rt_rhc_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rhc->rhc_V , a , b , c );
			VSCALE( rhc->rhc_V , rhc->rhc_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rhc->rhc_H , a , b , c );
			VSCALE( rhc->rhc_H , rhc->rhc_H , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( rhc->rhc_B , a , b , c );
			VSCALE( rhc->rhc_B , rhc->rhc_B , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			rhc->rhc_r = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			rhc->rhc_c = a * local2base;
			break;
		case ID_EPA:
			epa = (struct rt_epa_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( epa->epa_V , a , b , c );
			VSCALE( epa->epa_V , epa->epa_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( epa->epa_H , a , b , c );
			VSCALE( epa->epa_H , epa->epa_H , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( epa->epa_Au , a , b , c );
			VUNITIZE( epa->epa_Au );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			epa->epa_r1 = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			epa->epa_r2 = a * local2base;
			break;
		case ID_EHY:
			ehy = (struct rt_ehy_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ehy->ehy_V , a , b , c );
			VSCALE( ehy->ehy_V , ehy->ehy_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ehy->ehy_H , a , b , c );
			VSCALE( ehy->ehy_H , ehy->ehy_H , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( ehy->ehy_Au , a , b , c );
			VUNITIZE( ehy->ehy_Au );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			ehy->ehy_r1 = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			ehy->ehy_r2 = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			ehy->ehy_c = a * local2base;
			break;
		case ID_ETO:
			eto = (struct rt_eto_internal *)es_int.idb_ptr;
			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( eto->eto_V , a , b , c );
			VSCALE( eto->eto_V , eto->eto_V , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( eto->eto_N , a , b , c );
			VUNITIZE( eto->eto_N );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( eto->eto_C , a , b , c );
			VSCALE( eto->eto_C , eto->eto_C , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			eto->eto_rd = a * local2base;

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf" , &a );
			eto->eto_r = a * local2base;
			break;
		case ID_SUPERELL:
			superell = (struct rt_superell_internal *)es_int.idb_ptr;

			fprintf(stderr, "ID_SUPERELL\n");

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( superell->v , a , b , c );
			VSCALE( superell->v , superell->v , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( superell->a , a , b , c );
			VSCALE( superell->a , superell->a , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( superell->b , a , b , c );
			VSCALE( superell->b , superell->b , local2base );

			if( (str=Get_next_line( fp )) == NULL )
			{
				ret_val = 1;
				break;
			}
			(void)sscanf( str , "%lf %lf %lf" , &a , &b , &c );
			VSET( superell->c , a , b , c );
			VSCALE( superell->c , superell->c , local2base );
			
			if ( (str=Get_next_line( fp )) == NULL ) {
			  ret_val = 1;
			  break;
			}
			(void) sscanf( str, "%lf %lf", &superell->n, &superell->e);
			break;	
	}

	(void)fclose(fp);
	return( ret_val );
}


#ifdef _WIN32

/* Run $EDITOR on temp file */
editit( file )
const char *file;
{
   STARTUPINFO si = {0};
   PROCESS_INFORMATION pi = {0};
   char line[1024];

   sprintf(line,"notepad %s",file);

   
      si.cb = sizeof(STARTUPINFO);
      si.lpReserved = NULL;
      si.lpReserved2 = NULL;
      si.cbReserved2 = 0;
      si.lpDesktop = NULL;
      si.dwFlags = 0;

      CreateProcess( NULL,
                     line,
                     NULL,
                     NULL,
                     TRUE,
                     NORMAL_PRIORITY_CLASS,
                     NULL,
                     NULL,
                     &si,
                     &pi );
      WaitForSingleObject( pi.hProcess, INFINITE );
   
	return 1;
}

#else
/* else win32 is not defined */

/* Run $EDITOR on temp file 
 * 
 * BUGS -- right now we only check at compile time whether or not to pop up an
 *         X window to display into (for editors that do not open their own
 *         window like vi or jove).  If we have X support, we automatically use
 *         xterm (regardless of whether the user is running mged in console
 *         mode!)
 */
int
editit(const char *file)
{
	register int pid, xpid;
	register char *ed;
	int stat;
	void (*s2)(), (*s3)();

	if ((ed = getenv("EDITOR")) == (char *)0)
		ed = DEFEDITOR;
	bu_log("Invoking %s %s\n", ed, file);

	s2 = signal( SIGINT, SIG_IGN );
	s3 = signal( SIGQUIT, SIG_IGN );
	if ((pid = fork()) < 0) {
		perror("fork");
		return (0);
	}
	if (pid == 0) {
		register int i;
		/* Don't call bu_log() here in the child! */

		/* XXX do not want to close all io if we are in console mode
		 * and the editor needs to use stdout...
		 */
#	if defined(DM_X) || defined(DM_OGL)
		/* close all stdout/stderr (XXX except do not close 0==stdin) */
		for( i=1; i < 20; i++ )
			(void)close(i);
#	else
		/* leave stdin/out/err alone */
		for( i=3; i < 20; i++ )
			(void)close(i);
#	endif

		(void)signal( SIGINT, SIG_DFL );
		(void)signal( SIGQUIT, SIG_DFL );

		/* if we have x support, we pop open the editor in an xterm.
		 * otherwise, we use whatever the user gave as EDITOR
		 */
#	if defined(DM_X) || defined(DM_OGL)
		(void)execlp("xterm", "xterm", "-e", ed, file, (char *)0);
#	else
		(void)execlp(ed, ed, file, 0);
#	endif
		perror(ed);
		exit(1);
	}


	while ((xpid = wait(&stat)) >= 0)
		if (xpid == pid)
			break;

	(void)signal(SIGINT, s2);
	(void)signal(SIGQUIT, s3);

	return (!stat);
}
#endif
/* end check if win32 defined */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
