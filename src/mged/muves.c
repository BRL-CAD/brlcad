/*                         M U V E S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file muves.c
 *	Routines to support viewing of BRL-CAD models by using MUVES
 *	component/system names
 */
#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./sedit.h"
#include "../librt/debug.h"	/* XXX */

/* Maximum line length allowed for MUVES input files */
#define MUVES_LINE_LEN		256

/* Size of block for allocating memory for edit_com command */
#define	E_ARGV_BLOCK_LEN	100

/* object types for MUVES */
#define MUVES_TYPE_UNKNOWN	0
#define	MUVES_COMPONENT		1
#define	MUVES_SYSTEM		2

struct region_array
{
	struct directory *dp;
	int region_id;
};

struct cad_comp_list
{
	struct bu_list l;
	struct directory *dp;
};

struct muves_comp
{
	struct bu_list l;
	char *muves_name;
	struct cad_comp_list comp_head;
};

struct member_list
{
	struct bu_list l;
	int object_type;
	union muves_member
	{
		struct muves_comp *comp;
		struct muves_sys *sys;
	} mem;
};

struct muves_sys
{
	struct bu_list l;
	char *muves_name;
	struct member_list member_head;
};

static char *regionmap_delims=" \t";
static char *sysdef_delims=" \t\n@?!~&-^><|*+";

static struct muves_sys muves_sys_head={ {BU_LIST_HEAD_MAGIC, &muves_sys_head.l, &muves_sys_head.l}, (char *)NULL, { {BU_LIST_HEAD_MAGIC, &muves_sys_head.member_head.l, &muves_sys_head.member_head.l} }};
static struct muves_comp muves_comp_head={ {BU_LIST_HEAD_MAGIC, &muves_comp_head.l, &muves_comp_head.l}, (char *)NULL, {{BU_LIST_HEAD_MAGIC, &muves_comp_head.comp_head.l, &muves_comp_head.comp_head.l}, (struct directory *)NULL}};


void
Free_muves_sys(struct bu_list *hp)
{
	struct muves_sys *sys;

	while( BU_LIST_NON_EMPTY( hp ) )
	{
		sys = BU_LIST_FIRST( muves_sys, hp );
		bu_free( sys->muves_name, "muves_sys.muves_name" );
		BU_LIST_DEQUEUE( &sys->l );
		bu_free( (char *)sys, "muves_sys" );
	}
}

void
Free_cad_list(struct bu_list *hp)
{

	while( BU_LIST_NON_EMPTY( hp ) )
	{
		struct cad_comp_list *cad;

		cad = BU_LIST_FIRST( cad_comp_list, hp );
		BU_LIST_DEQUEUE( &cad->l );
		bu_free( (char *)cad, "cad" );
	}
}

void
Free_muves_comp(struct bu_list *hp)
{
	struct muves_comp *comp;

	while( BU_LIST_NON_EMPTY( hp ) )
	{
		comp = BU_LIST_FIRST( muves_comp, hp );
		bu_free( comp->muves_name, "muves_comp.muves_name" );
		Free_cad_list( &comp->comp_head.l );
		BU_LIST_DEQUEUE( &comp->l );
		bu_free( (char *)comp, "muves_comp" );
	}
}

/*
 *	F _ R E A D _ M U V E S
 *
 *  routine to read MUVES input files and create structures to hold the data
 */
int
f_read_muves(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	FILE *muves_in;
	char line[MUVES_LINE_LEN];
	struct muves_comp *new_comp;
	struct muves_sys *new_sys;
	struct directory *dp;
	struct region_array *regions;
	struct rt_comb_internal *comb;
	struct rt_db_internal intern;
	int line_no=0;
	int i;
	long reg_count=0;

	CHECK_DBI_NULL;

	if( argc < 2 || argc > 3 )
	{
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help %s", argv[0]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if( (muves_in=fopen( argv[1], "r" )) == NULL )
	{
		Tcl_AppendResult(interp, "Cannot open input file: ", argv[1], " aborting\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( BU_LIST_NON_EMPTY( &muves_sys_head.l ) )
		Free_muves_sys( &muves_sys_head.l );

	if( BU_LIST_NON_EMPTY( &muves_comp_head.l ) )
		Free_muves_comp( &muves_comp_head.l );

	/* count the number of regions in the model */
	FOR_ALL_DIRECTORY_START(dp, dbip) {
		if( !(dp->d_flags & DIR_REGION) )
			continue;
		reg_count++;
	} FOR_ALL_DIRECTORY_END;

	/* allocate an array to contain info for every region */
	regions = (struct region_array *)bu_calloc( reg_count, sizeof( struct region_array ), "regions" );

	/* fill in the regions array */
	reg_count =  0;

	FOR_ALL_DIRECTORY_START(dp, dbip) {
		if( !(dp->d_flags & DIR_REGION) )
			continue;

		if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
		{
			(void)signal( SIGINT, SIG_IGN );
			TCL_READ_ERR_return;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;

		regions[reg_count].dp = dp;
		regions[reg_count].region_id = comb->region_id;

		rt_db_free_internal( &intern, &rt_uniresource );
		reg_count++;
	} FOR_ALL_DIRECTORY_END;

	/* read lines of region map file */
	new_comp = (struct muves_comp *)NULL;
	while( bu_fgets( line, MUVES_LINE_LEN, muves_in ) != NULL )
	{
		char *ptr;

		line_no++;

		ptr = strtok( line, regionmap_delims );

		/* check for comment */
		if( *ptr == '#' )
			continue;

		if( *ptr == '"' )
		{
			/* continuation of previous component */
			if( !new_comp )
			{
				char str[32];

				sprintf( str, "%d", line_no );
				Tcl_AppendResult(interp, "Found a continuation indicator on line #",
					str, " in file ", argv[1],
					"when no component is active\n", (char *)NULL );
				fclose( muves_in );
				return TCL_ERROR;
			}
		}
		else
		{
			char *c;

			/* check for component name */

			c = ptr;
			while( *c != '\0' )
			{
				if( isalpha( *c ) )
				{
					int length;

					/* found a new component name, save the old one */
					if( new_comp && BU_LIST_IS_EMPTY( &new_comp->l ) )
					{
						BU_LIST_INSERT( &muves_comp_head.l, &new_comp->l )
					}

					/* check if this component name already exists */
					for( BU_LIST_FOR( new_comp, muves_comp, &muves_comp_head.l ) )
					{
						if( !strcmp( ptr, new_comp->muves_name ) )
							break;
					}

					/* if name doesn't exist, create a new list */
					if( BU_LIST_IS_HEAD( &new_comp->l, &muves_comp_head.l ) )
					{
						new_comp = (struct muves_comp *)bu_malloc( sizeof( struct muves_comp ), "new_comp" );
						BU_LIST_INIT( &new_comp->l );
						length = strlen( ptr );
						new_comp->muves_name = (char *)bu_malloc( length + 1, "muves_comp.name" );
						strcpy( new_comp->muves_name, ptr );
						BU_LIST_INIT( &new_comp->comp_head.l );
					}

					break;
				}
				c++;
			}
		}

		/* get next token */
		ptr = strtok( (char *)NULL, regionmap_delims );
		while( ptr )
		{
			char *ptr2;
			int id1, id2;

			if( *ptr == '#' )
				break;

			if( (ptr2 = strchr( ptr, ':' ) ) )
			{
				/* this is a range of idents */

				id1 = atoi( ptr );
				id2 = atoi( ptr2+1 );
			}
			else
			{
				/* this is a single ident */

				id1 = atoi( ptr );
				id2 = id1;
			}

			/* search through all regions for these idents */
			for( i = 0; i < reg_count; i++ )
			{
				struct cad_comp_list *comp;

				if( regions[i].region_id < id1 || regions[i].region_id > id2 )
					continue;

				/* this region is part of the current MUVES component */
				comp = (struct cad_comp_list *)bu_malloc( sizeof( struct cad_comp_list ), "comp" );
				comp->dp = regions[i].dp;
				BU_LIST_INSERT( &new_comp->comp_head.l, &comp->l );
			}

			ptr = strtok( (char *)NULL, regionmap_delims );
		}
	}

	bu_free( (char *)regions, "regions" );

	if( new_comp )
	{
		BU_LIST_INSERT( &muves_comp_head.l, &new_comp->l )
	}

	fclose( muves_in );

	if( argc < 3 )
		return TCL_OK;

	/* open sysdef file */
	if( (muves_in=fopen( argv[2], "r" )) == NULL )
	{
		Tcl_AppendResult(interp, "Cannot open input file: ", argv[2], " aborting\n", (char *)NULL );
		return TCL_ERROR;
	}

	new_sys = (struct muves_sys *)NULL;

	/* read sysdef file */
	while( bu_fgets( line, MUVES_LINE_LEN, muves_in ) != NULL )
	{
		char *ptr;
		char *c;
		int is_constant=1;
		int is_def = 0;
		int i;
		int in_subscript;
		char *equal_sign=(char *)NULL;

		i = 0;
		while( isspace( line[i] ) && line[i] != '\0' )
			i++;
		if( line[i] == '#' )	/* comment */
			continue;

		equal_sign = strchr( line, '=' );
		if( equal_sign )
		{
			struct muves_sys *sys;
			int j;

			*equal_sign = '\0';
			if( strchr( line, '(' ) )	/* function definition */
			{
				is_def = 0;
				new_sys = (struct muves_sys *)NULL;
				continue;
			}

			/* this is a system definition */
			is_def = 1;

			/* get rid of anything in square brackets (and the brackets) */
			ptr = strchr( line, '[' );
			if( ptr )
				*ptr = '\0';

			/* mark end of system name */
			j = i;
			while( line[j] != '\0' && !isspace( line[j] )  )
				j++;
			line[j] = '\0';

			/* look for system name in existing list */
			new_sys = (struct muves_sys *)NULL;
			for( BU_LIST_FOR( sys, muves_sys, &muves_sys_head.l ) )
			{
				if( !strcmp( &line[i], sys->muves_name ) )
				{
					/* found system already existing */
					new_sys = sys;
					break;
				}
			}

			if( !new_sys )
			{
				int length;

				/* need to create a new system */
				new_sys = (struct muves_sys *)bu_malloc( sizeof( struct muves_sys ), "new_sys" );
				length = strlen( &line[i] );
				new_sys->muves_name = (char *)bu_malloc( length + 1, "new_sys->muves_name" );
				strcpy( new_sys->muves_name, &line[i] );
				BU_LIST_INIT( &new_sys->member_head.l );
				BU_LIST_APPEND( &muves_sys_head.l, &new_sys->l );
			}

			/* look at rhs of '=' */
			ptr = (++equal_sign);
		}
		else
			ptr = line;

		if( !is_def )
			continue;

		/* eliminate square brackets and contents */
		in_subscript = 0;
		c = ptr;
		while( *c != '\0' )
		{
			if( *c == '[' )
				in_subscript++;
			else if( *c == ']' )
			{
				*c = ' ';
				in_subscript--;
			}

			if( in_subscript )
				*c = ' ';

			c++;
		}

		/* process remaining tokens on RHS */
		ptr = strtok( ptr, sysdef_delims );

		while( ptr )
		{
			struct member_list *member;
			struct muves_comp *comp;
			struct muves_sys *sys;
			int already_member;

			if( *ptr == '#' )	/* comment */
				break;

			c = ptr;
			is_constant = 1;
			while( *c != '\0' )
			{
				if( isalpha( *c ) )
				{
					is_constant = 0;
					break;
				}
				c++;
			}

			if( is_constant )	/* ignore numerical constants */
			{
				ptr = strtok( (char *)NULL, sysdef_delims );
				continue;
			}

			/* found a system or component name */

			/* check if this is already a member of the current system */
			already_member = 0;
			for( BU_LIST_FOR( member, member_list, &new_sys->member_head.l ) )
			{
				switch( member->object_type )
				{
					case MUVES_COMPONENT:   /* component */
						if( !strcmp( ptr, member->mem.comp->muves_name ) )
							already_member = 1;
						break;
					case MUVES_SYSTEM:      /* system */
						if( !strcmp( ptr, member->mem.sys->muves_name ) )
							already_member = 1;
						break;
					default:
						Tcl_AppendResult(interp, "\t",
							"ERROR: Unrecognized type of system member\n", (char *)NULL );
						break;
				}
				if( already_member )
					break;
			}
			if( already_member )
			{
				ptr = strtok( (char *)NULL, sysdef_delims );
				continue;
			}

			member = (struct member_list *)bu_malloc( sizeof( struct member_list ), "member" );
			member->object_type = MUVES_TYPE_UNKNOWN;
			for( BU_LIST_FOR( sys, muves_sys, &muves_sys_head.l ) )
			{
				if( !strcmp( ptr, sys->muves_name ) )
				{
					/* found a matching system name */
					member->object_type = MUVES_SYSTEM;
					member->mem.sys = sys;
					break;
				}
			}

			if( member->object_type == MUVES_TYPE_UNKNOWN )
			{
				/* look for a matching component */
				for( BU_LIST_FOR( comp, muves_comp, &muves_comp_head.l ) )
				{
					if( !strcmp( ptr, comp->muves_name ) )
					{
						/* found a matching component */
						member->object_type = MUVES_COMPONENT;
						member->mem.comp = comp;
						break;
					}
				}
			}

			if( member->object_type == MUVES_TYPE_UNKNOWN )
			{
				/* didn't find system or component */
				Tcl_AppendResult(interp, "WARNING: Could not find member ", ptr, " while building system ", new_sys->muves_name, "\n", (char *)NULL );
				bu_free( member, "member" );
			}
			else
				BU_LIST_APPEND( &new_sys->member_head.l, &member->l )

			ptr = strtok( (char *)NULL, sysdef_delims );
		}

	}
	return TCL_OK;
}

int
Display_muves_comp(struct muves_comp *comp, int *e_argc, char ***e_argv, int *e_argv_len)
{
	struct cad_comp_list *cad;

	for( BU_LIST_FOR( cad, cad_comp_list, &comp->comp_head.l ) )
	{
		(*e_argc)++;
		if( *e_argc >= *e_argv_len )
		{
			(*e_argv_len) += E_ARGV_BLOCK_LEN;
			(*e_argv) = (char **)bu_realloc( *e_argv, sizeof( char *) * (*e_argv_len), "e_argv");
		}

		(*e_argv)[(*e_argc)-1] = cad->dp->d_namep;
	}
	return 42;
}

int
Display_muves_sys(struct muves_sys *sys, int *e_argc, char ***e_argv, int *e_argv_len)
{
	struct member_list *member;

	for( BU_LIST_FOR( member, member_list, &sys->member_head.l ) )
	{
		switch( member->object_type )
		{
			case MUVES_COMPONENT:		/* component */
				Display_muves_comp( member->mem.comp, e_argc, e_argv, e_argv_len );
				break;
			case MUVES_SYSTEM:		/* system */
				Display_muves_sys( member->mem.sys, e_argc, e_argv, e_argv_len );
				break;
			default:	/* error */
				Tcl_AppendResult(interp, "unrecognized member type in system ",
					sys->muves_name, " \n", (char *)NULL );
				return TCL_ERROR;
		}
	}

	return TCL_OK;
}

/*
 *	F _ E _ M U V E S
 *
 * routine to display regions using MUVES component names
 */

int
f_e_muves(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct muves_comp *comp;
	struct muves_sys *sys;
	int i;
	char **e_argv;
	int e_argc;
	int e_argv_len=0;
	char *e_comm="e";

	e_argv = (char **)bu_malloc( E_ARGV_BLOCK_LEN * sizeof( char *), "e_argv" );
	e_argv_len = E_ARGV_BLOCK_LEN;
	e_argv[0] = e_comm;
	e_argc = 1;

	/* loop through args (each should be a MUVES component or system */
	for( i=1 ; i<argc ; i++ )
	{
		/* look in list of MUVES components */
		for( BU_LIST_FOR( comp, muves_comp, &muves_comp_head.l ) )
		{
			if( !strcmp( argv[i], comp->muves_name ) )
				Display_muves_comp( comp,
					&e_argc, &e_argv, &e_argv_len  );
		}

		/* look in list of MUVES systems */
		for( BU_LIST_FOR( sys, muves_sys, &muves_sys_head.l ) )
		{
			if( !strcmp( argv[i], sys->muves_name ) )
				Display_muves_sys( sys,
					&e_argc, &e_argv, &e_argv_len  );
		}
	}

	if( e_argc > 1 )
		return( edit_com( e_argc, e_argv, 1 , 0 ) );

	bu_free( (char *)e_argv, "e_argv" );

	return TCL_OK;
}

/*	F _ L _ M U V E S
 *
 *  routine to list the muves comoponents
 */
int
f_l_muves(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int i;

	if( BU_LIST_IS_EMPTY( &muves_comp_head.l ) && BU_LIST_IS_EMPTY( &muves_sys_head.l ) )
	{
		Tcl_AppendResult(interp, "No MUVES components known, use 'read_muves' command\n", (char *)NULL );
		return TCL_ERROR;
	}

	for( i=1 ; i<argc ; i++ )
	{
		struct muves_comp *comp;
		struct muves_sys *sys;

		for( BU_LIST_FOR( comp, muves_comp, &muves_comp_head.l ) )
		{
			if( !strcmp( argv[i], comp->muves_name ) )
			{
				struct cad_comp_list *cad;
				int member_count;
				char count_str[32];

				member_count = 0;
				for( BU_LIST_FOR( cad, cad_comp_list, &comp->comp_head.l ) )
					member_count++;

				sprintf( count_str, "%d members:\n", member_count );
				Tcl_AppendResult(interp, comp->muves_name, " (component) ", count_str, (char *)NULL );

				for( BU_LIST_FOR( cad, cad_comp_list, &comp->comp_head.l ) )
					Tcl_AppendResult(interp, "\t", cad->dp->d_namep, "\n", (char *)NULL );
			}
		}

		for( BU_LIST_FOR( sys, muves_sys, &muves_sys_head.l ) )
		{
			if( !strcmp( argv[i], sys->muves_name ) )
			{
				struct member_list *member;
				int member_count;
				char count_str[32];

				member_count = 0;
				for( BU_LIST_FOR( member, member_list, &sys->member_head.l ) )
					member_count++;

				sprintf( count_str, "%d members:\n", member_count );

				Tcl_AppendResult(interp, sys->muves_name, " (system) ", count_str, (char *)NULL );

				for( BU_LIST_FOR( member, member_list, &sys->member_head.l ) )
				{
					switch( member->object_type )
					{
						case MUVES_COMPONENT:	/* component */
							Tcl_AppendResult(interp, "\t",
								member->mem.comp->muves_name, " (component)\n", (char *)NULL );
							break;
						case MUVES_SYSTEM:	/* system */
							Tcl_AppendResult(interp, "\t",
								member->mem.sys->muves_name, " (system)\n", (char *)NULL );
							break;
						default:
							Tcl_AppendResult(interp, "\t",
								"ERROR: Unrecognized type of system member\n", (char *)NULL );
							break;
					}
				}
			}
		}
	}

	return TCL_OK;
}

/*	F _ T _ M U V E S
 *
 *  routine to list the muves comoponents
 */
int
f_t_muves(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct muves_comp *comp;
	struct muves_sys *sys;

	for( BU_LIST_FOR( comp, muves_comp, &muves_comp_head.l ) )
		Tcl_AppendResult(interp, "\t", comp->muves_name, " (component)\n", (char *)NULL );

	for( BU_LIST_FOR( sys, muves_sys, &muves_sys_head.l ) )
		Tcl_AppendResult(interp, "\t", sys->muves_name, " (system)\n", (char *)NULL );

	return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
