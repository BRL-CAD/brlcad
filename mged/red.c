/*
 *			R E D . C
 *
 *	These routines allow editing of a combination using the text editor
 *	of the users choice.
 *
 *  Author -
 *	John Anderson
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#	include <string.h>
#else
#	include <strings.h>
#endif
#include <errno.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "externs.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include <ctype.h>

static char	red_tmpfil[] = "/tmp/GED.aXXXXX";
static char	red_tmpcomb[] = "red_tmp.aXXXXX";
static char	*delims = " \t/";	/* allowable delimiters */

void restore_comb();
int editit();
int clear_comb(),build_comb(),save_comb();

static int ident;
static int air;

int
f_red(clientData, interp, argc , argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int node_count;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	CHECK_READ_ONLY;

	if(argc != 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help red");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	dp = db_lookup( dbip , argv[1] , LOOKUP_QUIET );

	if( dp != DIR_NULL )
	{
		if( !(dp->d_flags & DIR_COMB ) )
		{
		  Tcl_AppendResult(interp, argv[1],
				   " is not a combination, so cannot be edited this way\n", (char *)NULL);
		  return TCL_ERROR;
		}

		if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
			TCL_READ_ERR_return;

		comb = (struct rt_comb_internal *)intern.idb_ptr;

		/* Make a file for the text editor */
		(void)mktemp( red_tmpfil );

		/* Write the combination components to the file */
		if( writecomb( comb, dp->d_namep ) )
		{
		  Tcl_AppendResult(interp, "Unable to edit ", argv[1], "\n", (char *)NULL);
		  unlink( red_tmpfil );
		  return TCL_ERROR;
		}
	}
	else
	{
		comb = (struct rt_comb_internal *)NULL;
		/* Make a file for the text editor */
		(void)mktemp( red_tmpfil );

		/* Write the combination components to the file */
		if( writecomb( comb, argv[1] ) )
		{
		  Tcl_AppendResult(interp, "Unable to edit ", argv[1], "\n", (char *)NULL);
		  unlink( red_tmpfil );
		  return TCL_ERROR;
		}
	}

	/* Edit the file */
	if( editit( red_tmpfil ) )
	{
	  if( (node_count = checkcomb()) < 0 ){ /* Do some quick checking on the edited file */
	    Tcl_AppendResult(interp, "Error in edited region, no changes made\n", (char *)NULL);
	    if( comb )
		    rt_comb_ifree( &intern );
	    (void)unlink( red_tmpfil );
	    return TCL_ERROR;
	  }

	  if( comb )
	  {
	  if( save_comb( dp ) ){ /* Save combination to a temp name */
	    Tcl_AppendResult(interp, "No changes made\n", (char *)NULL);
	    rt_comb_ifree( &intern );
	    (void)unlink( red_tmpfil );
	    return TCL_OK;
	  }
	}

	  if( build_comb( comb, dp, node_count, argv[1] ) )
	  {
	    Tcl_AppendResult(interp, "Unable to construct new ", dp->d_namep,
			     (char *)NULL);
	    if( comb )
	    {
		    restore_comb( dp );
	    	    Tcl_AppendResult(interp, "\toriginal restored\n", (char *)NULL );
		    rt_comb_ifree( &intern );
	    }
	    (void)unlink( red_tmpfil );
	    return TCL_ERROR;
	  }
	  else if( comb )
	  {
		/* eliminate the temporary combination */
	    char *av[3];

	    av[0] = "kill";
	    av[1] = red_tmpcomb;
	    av[2] = NULL;
	    (void)f_kill(clientData, interp, 2, av);
	  }
	}

	(void)unlink( red_tmpfil );
	return TCL_OK;
}

HIDDEN char *
find_keyword( i, line, word )
int i;
char *line;
char *word;
{
	char *ptr1;
	char *ptr2;
	int j;

	/* find the keyword */
	ptr1 = strstr( &line[i], word );
	if( !ptr1 )
		return( (char *)NULL );

	/* find the '=' */
	ptr2 = strchr( ptr1, '=' );
	if( !ptr2 )
		return( (char *)NULL );

	/* skip any white space before the value */
	while( isspace( *(++ptr2) ) );

	/* eliminate trailing white space */
	j = strlen( line );
	while( isspace( line[--j] ) );
	line[j+1] = '\0';

	/* return pointer to the value */
	return( ptr2 );
}

HIDDEN void
print_matrix( fp, matrix )
FILE *fp;
matp_t matrix;
{
	int k;
	char buf[64];
	fastf_t tmp;

	if( !matrix )
		return;

	if( bn_mat_is_identity( matrix ) )
		return;

	for( k=0 ; k<16 ; k++ )
	{
		sprintf( buf, "%g", matrix[k] );
		tmp = atof( buf );
		if( tmp == matrix[k] )
			fprintf( fp, " %g", matrix[k] );
		else
			fprintf( fp, " %.12e", matrix[k] );
	}
}

HIDDEN void
vls_print_matrix(vls, matrix)
struct bu_vls *vls;
matp_t matrix;
{
  int k;
  char buf[64];
  fastf_t tmp;

  if(!matrix)
    return;

  if(bn_mat_is_identity(matrix))
    return;

  for(k=0; k<16; k++){
    sprintf(buf, "%g", matrix[k]);
    tmp = atof(buf);
    if(tmp == matrix[k])
      bu_vls_printf(vls, " %g", matrix[k]);
    else
      bu_vls_printf(vls, " %.12e", matrix[k]);
  }
}

HIDDEN int
put_rgb_into_comb(comb, str)
struct rt_comb_internal *comb;
char *str;
{
  int r, g, b;

  if(sscanf(str, "%d%d%d", &r, &g, &b) != 3)
    return TCL_ERROR;

  /* clamp the RGB values to [0,255] */
  if(r < 0)
    r = 0;
  else if(r > 255)
    r = 255;

  if(g < 0)
    g = 0;
  else if(g > 255)
    g = 255;

  if(b < 0)
    b = 0;
  else if(b > 255)
    b = 255;

  comb->rgb[0] = (unsigned char)r;
  comb->rgb[1] = (unsigned char)g;
  comb->rgb[2] = (unsigned char)b;
  comb->rgb_valid = 1;

  return TCL_OK;
}

struct line_list{
  struct bu_list l;
  char *line;
};

struct line_list HeadLines;

HIDDEN int
put_tree_into_comb(comb, dp, old_name, new_name, str)
struct rt_comb_internal *comb;
struct directory *dp;
char *old_name;
char *new_name;
char *str;
{
  int i;
  char *line;
  char *ptr;
  char relation;
  char name[NAMESIZE+1];
  char newline[] = "\n";
  struct directory *dp1;
  struct rt_tree_array *rt_tree_array;
  struct line_list *llp;
  int node_count = 0;
  int tree_index = 0;
  union tree *tp;
  union tree *final_tree;
  struct rt_db_internal intern;
  matp_t matrix;

  if(str == (char *)NULL)
    return TCL_ERROR;

  BU_LIST_INIT(&HeadLines.l);

  /* break str into lines */
  line = strtok(str, newline);
  while(line != (char *)NULL){
    BU_GETSTRUCT(llp, line_list);
    BU_LIST_INSERT(&HeadLines.l, &llp->l);
    llp->line = line;
    line = strtok((char *)NULL, newline);
    ++node_count;
  }

  /* build tree list */
  if( node_count )
    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list" );
  else
    rt_tree_array = (struct rt_tree_array *)NULL;

  for(BU_LIST_FOR(llp, line_list, &HeadLines.l)){
    ptr = strtok(llp->line, delims);

    /* First non-white is the relation operator */
    relation = (*ptr);
    if( relation == '\0' ){
      Tcl_AppendResult(interp, "no relational operator\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* Next must be the member name */
    ptr = strtok( (char *)NULL, delims );
    if(ptr == (char *)NULL){
      Tcl_AppendResult(interp, "no name specified\n", (char *)NULL);
      return TCL_ERROR;
    }
    strncpy( name , ptr, NAMESIZE );
    name[NAMESIZE] = '\0';

    /* Eliminate trailing white space from name */
    i = NAMESIZE;
    while( isspace( name[--i] ) )
      name[i] = '\0';

    /* Check for existence of member */
    if( (dp1=db_lookup( dbip , name , LOOKUP_QUIET )) == DIR_NULL )
      Tcl_AppendResult(interp, "\tWARNING: '", name,
		       "' does not exist\n", (char *)NULL);

    /* get matrix */
    ptr = strtok( (char *)NULL, delims );
    if(ptr == (char *)NULL)
      matrix = (matp_t)NULL;
    else{
      int k;

      matrix = (matp_t)bu_calloc( 16, sizeof( fastf_t ), "red: matrix" );
      matrix[0] = atof( ptr );
      for( k=1 ; k<16 ; k++ ){
	ptr = strtok( (char *)NULL, delims );
	if( !ptr ){
	  Tcl_AppendResult(interp, "incomplete matrix for member ",
			   name, " No changes made\n", (char *)NULL );
	  bu_free( (char *)matrix, "red: matrix" );
	  if( rt_tree_array )
	    bu_free( (char *)rt_tree_array, "red: tree list" );
	  return TCL_ERROR;
	}
	matrix[k] = atof( ptr );
      }
      if( bn_mat_is_identity( matrix ) ){
	bu_free( (char *)matrix, "red: matrix" );
	matrix = (matp_t)NULL;
      }
    }

    /* Add it to the combination */
    switch( relation ){
    case '+':
      rt_tree_array[tree_index].tl_op = OP_INTERSECT;
      break;
    case '-':
      rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
      break;
    default:
      Tcl_AppendResult(interp, "unrecognized relation (assume UNION)\n",
		       (char *)NULL );
    case 'u':
      rt_tree_array[tree_index].tl_op = OP_UNION;
      break;
    }

    BU_GETUNION( tp, tree );
    rt_tree_array[tree_index].tl_tree = tp;
    tp->tr_l.magic = RT_TREE_MAGIC;
    tp->tr_l.tl_op = OP_DB_LEAF;
    tp->tr_l.tl_name = bu_strdup( name );
    tp->tr_l.tl_mat = matrix;
    tree_index++;
  }

  if( tree_index )
    final_tree = (union tree *)db_mkgift_tree( rt_tree_array, node_count, (struct db_tree_state *)NULL );
  else
    final_tree = (union tree *)NULL;

  RT_INIT_DB_INTERNAL( &intern );
  intern.idb_type = ID_COMBINATION;
  intern.idb_ptr = (genptr_t)comb;
  comb->tree = final_tree;

  if( strncmp( new_name, old_name, NAMESIZE ) ){
    int flags;

    if( comb->region_flag )
      flags = DIR_COMB | DIR_REGION;
    else
      flags = DIR_COMB;

    if( dp != DIR_NULL ){
      if(  db_delete( dbip, dp ) || db_dirdelete( dbip, dp ) ){
	Tcl_AppendResult(interp, "ERROR: Unable to delete directory entry for ",
			 old_name, "\n", (char *)NULL );
	rt_comb_ifree( &intern );
	return( 1 );
      }
    }

    if( (dp=db_diradd( dbip, new_name, -1, node_count+1, flags)) == DIR_NULL ){
      Tcl_AppendResult(interp, "Cannot add ", new_name,
		       " to directory, no changes made\n", (char *)NULL);
      rt_comb_ifree( &intern );
      return( 1 );
    }

    if( db_alloc( dbip, dp, node_count+1) ){
      Tcl_AppendResult(interp, "Cannot allocate file space for ", new_name,
		       "\n", (char *)NULL );
      rt_comb_ifree( &intern );
      return( 1 );
    }
  }else if( dp == DIR_NULL ){
    int flags;

    if( comb->region_flag )
      flags = DIR_COMB | DIR_REGION;
    else
      flags = DIR_COMB;

    if( (dp=db_diradd( dbip, new_name, -1, node_count+1, flags)) == DIR_NULL ){
      Tcl_AppendResult(interp, "Cannot add ", new_name,
		       " to directory, no changes made\n", (char *)NULL);
      rt_comb_ifree( &intern );
      return TCL_ERROR;
    }

    if( db_alloc( dbip, dp, node_count+1) ){
      Tcl_AppendResult(interp, "Cannot allocate file space for ", new_name,
		       "\n", (char *)NULL );
      rt_comb_ifree( &intern );
      return TCL_ERROR;
    }
  }

  if( rt_db_put_internal( dp, dbip, &intern ) < 0 ){
    Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int
cmd_get_comb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct directory *dp;
  struct rt_db_internal	intern;
  struct rt_comb_internal *comb;
  struct rt_tree_array	*rt_tree_array;
  int offset,i;
  int node_count;
  int actual_count;

  if(dbip == DBI_NULL)
    return TCL_OK;

  CHECK_READ_ONLY;

  if(argc != 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help get_comb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  dp = db_lookup( dbip , argv[1] , LOOKUP_QUIET );

  if(dp != DIR_NULL){
    struct bu_vls vls;

    if( !(dp->d_flags & DIR_COMB) ){
      Tcl_AppendResult(interp, argv[1],
		       " is not a combination, so cannot be edited this way\n", (char *)NULL);
      return TCL_ERROR;
    }

    if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
      TCL_READ_ERR_return;

    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 ){
      db_non_union_push( comb->tree );
      if( db_ck_v4gift_tree( comb->tree ) < 0 ){
	Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL );
	return TCL_ERROR;
      }
    }

    node_count = db_tree_nleaves( comb->tree );
    if( node_count > 0 ){
      rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
							 sizeof( struct rt_tree_array ), "tree list" );
      actual_count = (struct rt_tree_array *)db_flatten_tree( rt_tree_array, comb->tree, OP_UNION ) - rt_tree_array;
      if( actual_count > node_count )  bu_bomb("write_comb() array overflow!");
      if( actual_count < node_count )  bu_log("WARNING write_comb() array underflow! %d < %d", actual_count, node_count);
    } else {
      rt_tree_array = (struct rt_tree_array *)NULL;
      actual_count = 0;
    }

    bu_vls_init(&vls);
    Tcl_AppendElement(interp, dp->d_namep);                 /* NAME=name */
    if( comb->region_flag ){
      Tcl_AppendElement(interp, "Yes");              /* REGION=Yes */
      bu_vls_printf(&vls, "%d", comb->region_id );
      Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* REGION_ID=comb->region_id */
      bu_vls_trunc(&vls, 0);
      bu_vls_printf(&vls, "%d", comb->aircode);
      Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* AIRCODE=comb->aircode */
      bu_vls_trunc(&vls, 0);
      bu_vls_printf(&vls, "%d", comb->GIFTmater);
      Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* GIFT_MATERIAL=comb->GIFTmater */
      bu_vls_trunc(&vls, 0);
      bu_vls_printf(&vls, "%d", comb->los );
      Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* LOS=comb->los */
    }else{
      Tcl_AppendElement(interp, "No");   /* REGION=No        */
      Tcl_AppendElement(interp, "");     /* REGION_ID=""     */
      Tcl_AppendElement(interp, "");     /* AIRCODE=""       */
      Tcl_AppendElement(interp, "");     /* GIFT_MATERIAL="" */
      Tcl_AppendElement(interp, "");     /* LOS=""           */
    }

    if(comb->rgb_valid){
      bu_vls_trunc(&vls, 0);
      bu_vls_printf(&vls, "%d %d %d", V3ARGS(comb->rgb));
      Tcl_AppendElement(interp, bu_vls_addr(&vls));  /* COLOR=comb->rgb */
    }else
      Tcl_AppendElement(interp, "");                 /* COLOR=""        */

    Tcl_AppendElement(interp, bu_vls_addr(&comb->shader)); /* SHADER=comb->shader */

    if( comb->inherit )
      Tcl_AppendElement(interp, "Yes");  /* INHERIT=Yes */
    else
      Tcl_AppendElement(interp, "No");   /* INHERIT=No  */


    bu_vls_trunc(&vls, 0);
    for( i=0 ; i<actual_count ; i++ ){
      char op;

      switch( rt_tree_array[i].tl_op ){
      case OP_UNION:
	op = 'u';
	break;
      case OP_INTERSECT:
	op = '+';
	break;
      case OP_SUBTRACT:
	op = '-';
	break;
      default:
	Tcl_AppendResult(interp, "Illegal op code in tree\n",
			 (char *)NULL );
	bu_vls_free(&vls);

	return TCL_ERROR;
      }

      bu_vls_printf(&vls, " %c %s" , op , rt_tree_array[i].tl_tree->tr_l.tl_name);
      vls_print_matrix(&vls, rt_tree_array[i].tl_tree->tr_l.tl_mat);
      bu_vls_printf(&vls, "\n");
    }

    Tcl_AppendElement(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
  }else {
    Tcl_AppendElement(interp, argv[1]); /* NAME=argv[1]     */
    Tcl_AppendElement(interp, "No");    /* REGION=No        */
    Tcl_AppendElement(interp, "");      /* REGION_ID=""     */
    Tcl_AppendElement(interp, "");      /* AIRCODE=""       */
    Tcl_AppendElement(interp, "");      /* GIFT_MATERIAL="" */
    Tcl_AppendElement(interp, "");      /* LOS=""           */
    Tcl_AppendElement(interp, "");      /* COLOR=""         */
    Tcl_AppendElement(interp, "");      /* SHADER=""        */
    Tcl_AppendElement(interp, "No");    /* INHERIT=No       */
    Tcl_AppendElement(interp, "");      /* COMBINATION:""   */

    return TCL_OK;
  }
}

int
cmd_put_comb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct directory *dp;
  struct rt_db_internal	intern;
  struct rt_comb_internal *comb;
  char new_name[NAMESIZE+1];

  if(dbip == DBI_NULL)
    return TCL_OK;

  CHECK_READ_ONLY;

  if(argc != 11){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help put_comb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  dp = db_lookup( dbip , argv[1] , LOOKUP_QUIET );
  if(dp != DIR_NULL){
    if( !(dp->d_flags & DIR_COMB) ){
      Tcl_AppendResult(interp, argv[1],
		       " is not a combination, so cannot be edited this way\n", (char *)NULL);
      return TCL_ERROR;
    }
    
    if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
      TCL_READ_ERR_return;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    save_comb(dp); /* Save combination to a temp name */
  }else{
    comb = (struct rt_comb_internal *)NULL;
  }

  /* empty the existing combination */
  if( comb && comb->tree ){
    db_free_tree( comb->tree );
    comb->tree = NULL;
  }else{
    /* make an empty combination structure */
    BU_GETSTRUCT( comb, rt_comb_internal );
    comb->magic = RT_COMB_MAGIC;
    comb->tree = TREE_NULL;
    bu_vls_init( &comb->shader );
    bu_vls_init( &comb->material );
  }

  if(dp == DIR_NULL)
    NAMEMOVE(argv[1], new_name);
  else
    NAMEMOVE(dp->d_namep, new_name);

  if(*argv[2] == 'y' || *argv[2] == 'Y')
    comb->region_flag = 'Y';
  else
    comb->region_flag = '\0';

  comb->region_id = atoi(argv[3]);
  comb->aircode = atoi(argv[4]);
  comb->GIFTmater = atoi(argv[5]);
  comb->los = atoi(argv[6]);
  put_rgb_into_comb(comb, argv[7]);
  bu_vls_strcpy(&comb->shader, argv[8]);

  if(*argv[9] == 'y' || *argv[9] == 'Y')
    comb->inherit = 1;
  else
    comb->inherit = 0;

  if(put_tree_into_comb(comb, dp, argv[1], new_name, argv[10]) == TCL_ERROR){
    if(comb){
      restore_comb(dp);
      Tcl_AppendResult(interp, "\toriginal restored\n", (char *)NULL);
    }
    (void)unlink(red_tmpfil);
    return TCL_ERROR;
  }else if(comb){
    /* eliminate the temporary combination */
    char *av[3];

    av[0] = "kill";
    av[1] = red_tmpcomb;
    av[2] = NULL;
    (void)f_kill(clientData, interp, 2, av);
  }

  (void)unlink(red_tmpfil);
  return TCL_OK;
}

int
writecomb( comb, name )
struct rt_comb_internal	*comb;
char *name;
{
/*	Writes the file for later editing */
	struct rt_tree_array	*rt_tree_array;
	FILE			*fp;
	int			offset,i;
	int			node_count;
	int			actual_count;

	if( comb )
		RT_CK_COMB( comb );

	/* open the file */
	if( (fp=fopen( red_tmpfil , "w" )) == NULL )
	{
	  Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
	  perror( "MGED" );
	  return(1);
	}

	if( !comb )
	{
		fprintf( fp, "NAME=%s\n", name );
		fprintf( fp, "REGION=No\n" );
		fprintf( fp, "REGION_ID=\n" );
		fprintf( fp, "AIRCODE=\n" );
		fprintf( fp, "GIFT_MATERIAL=\n" );
		fprintf( fp, "LOS=\n" );
		fprintf( fp, "COLOR=\n" );
		fprintf( fp, "SHADER=\n" );
		fprintf( fp, "INHERIT=No\n" );
		fprintf( fp, "COMBINATION:\n" );
		fclose( fp );
		return( 0 );
	}

	if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
	{
		db_non_union_push( comb->tree );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
		{
			Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL );
			return( 1 );
		}
	}
	node_count = db_tree_nleaves( comb->tree );
	if( node_count > 0 )
	{
		rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
			sizeof( struct rt_tree_array ), "tree list" );
		actual_count = (struct rt_tree_array *)db_flatten_tree( rt_tree_array, comb->tree, OP_UNION ) - rt_tree_array;
		if( actual_count > node_count )  bu_bomb("write_comb() array overflow!");
		if( actual_count < node_count )  bu_log("WARNING write_comb() array underflow! %d < %d", actual_count, node_count);
	}
	else
	{
		rt_tree_array = (struct rt_tree_array *)NULL;
		actual_count = 0;
	}

	fprintf( fp, "NAME=%s\n", name );
	if( comb->region_flag )
	{
		fprintf( fp, "REGION=Yes\n" );
		fprintf( fp, "REGION_ID=%d\n", comb->region_id );
		fprintf( fp, "AIRCODE=%d\n", comb->aircode );
		fprintf( fp, "GIFT_MATERIAL=%d\n", comb->GIFTmater );
		fprintf( fp, "LOS=%d\n", comb->los );
	}
	else
	{
		fprintf( fp, "REGION=No\n" );
		fprintf( fp, "REGION_ID=\n" );
		fprintf( fp, "AIRCODE=\n" );
		fprintf( fp, "GIFT_MATERIAL=\n" );
		fprintf( fp, "LOS=\n" );
	}

	if( comb->rgb_valid )
		fprintf( fp, "COLOR= %d %d %d\n", V3ARGS( comb->rgb ) );
	else
		fprintf( fp, "COLOR=\n" );

	fprintf( fp, "SHADER=%s\n", bu_vls_addr( &comb->shader ) );
#if 0
	fprintf( fp, "MATERIAL=%s\n", bu_vls_addr( &comb->material ) );
#endif
	if( comb->inherit )
		fprintf( fp, "INHERIT=Yes\n" );
	else
		fprintf( fp, "INHERIT=No\n" );

	fprintf( fp, "COMBINATION:\n" );

	for( i=0 ; i<actual_count ; i++ )
	{
		char op;

		switch( rt_tree_array[i].tl_op )
		{
			case OP_UNION:
				op = 'u';
				break;
			case OP_INTERSECT:
				op = '+';
				break;
			case OP_SUBTRACT:
				op = '-';
				break;
			default:
				Tcl_AppendResult(interp, "Illegal op code in tree\n",
					(char *)NULL );
				fclose( fp );
				return( 1 );
		}
		if( fprintf( fp , " %c %s" , op , rt_tree_array[i].tl_tree->tr_l.tl_name ) <= 0 )
		{
			Tcl_AppendResult(interp, "Cannot write to temp file (", red_tmpfil,
				"). Aborting edit\n", (char *)NULL );
			fclose( fp );
			return( 1 );
		}
		print_matrix( fp, rt_tree_array[i].tl_tree->tr_l.tl_mat );
		fprintf( fp, "\n" );
	}
	fclose( fp );
	return( 0 );
}

int
checkcomb()
{
/*	Do some minor checking of the edited file */

	FILE *fp;
	int node_count=0;
	int nonsubs=0;
	int i,j,done,ch;
	char relation;
	char name[NAMESIZE+1];
	char line[MAXLINE];
	char *ptr;
	int region=(-1);
	int id,air,gift,los;
	unsigned char rgb[3];
	int rgb_valid;

	if( (fp=fopen( red_tmpfil , "r" )) == NULL )
	{
	  Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
	  perror( "MGED" );
	  return(-1);
	}

	/* Read a line at a time */
	done = 0;
	while( !done )
	{
		/* Read a line */
		i = (-1);

		while( (ch=getc( fp )) != EOF && ch != '\n' && i<MAXLINE )
			line[++i] = ch;

		if( ch == EOF )	/* We must be done */
		{
			done = 1;
			break;
		}
		if( i == MAXLINE )
		{
		  Tcl_AppendResult(interp, "Line too long in edited file\n", (char *)NULL);
		  return( 1 );
		}

		line[++i] = '\0';

		/* skip leading white space */
		i = (-1);
		while( isspace( line[++i] ));

		if( (ptr=find_keyword(i, line, "NAME" ) ) )
		{
			strncpy( name, ptr, NAMESIZE );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "REGION" ) ) )
		{
			if( *ptr == 'y' || *ptr == 'Y' )
				region = 1;
			else
				region = 0;
			continue;
		}
		else if( (ptr=find_keyword( i, line, "REGION_ID" ) ) )
		{
			id = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "AIRCODE" ) ) )
		{
			air = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "GIFT_MATERIAL" ) ) )
		{
			gift = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "LOS" ) ) )
		{
			los = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "COLOR" ) ) )
		{
			char *ptr2;

			ptr2 = strtok( ptr, delims );
			if( !ptr2 )
			{
				rgb_valid = 0;
				continue;
			}
			rgb[0] = atoi( ptr2 ) & 0377;
			ptr2 = strtok( (char *)NULL, delims );
			if( !ptr2 )
			{
				rgb_valid = 0;
				continue;
			}
			rgb[1] = atoi( ptr2 ) & 0377;
			ptr2 = strtok( (char *)NULL, delims );
			if( !ptr2 )
			{
				rgb_valid = 0;
				continue;
			}
			rgb[2] = atoi( ptr2 ) & 0377;
			continue;
		}
		else if( (ptr=find_keyword( i, line, "SHADER" ) ) )
			continue;
#if 0
		else if( (ptr=find_keyword( i, line, "MATERIAL" ) ) )
			continue;
#endif
		else if( (ptr=find_keyword( i, line, "INHERIT" ) ) )
			continue;
		else if( !strncmp( &line[i], "COMBINATION:", 12 ) )
		{
			if( region < 0 )
			{
				Tcl_AppendResult(interp, "Region flag not correctly set\n",
					"\tMust be 'Yes' or 'No'\n", "\tNo Changes made\n",
					(char *)NULL );
				fclose( fp );
				return( 1 );
			}
			else if( region )
			{
				if( id < 0 )
				{
					Tcl_AppendResult(interp, "invalid region ID\n",
						"\tNo Changes made\n",
						(char *)NULL );
					fclose( fp );
					return( 1 );
				}
				if( air < 0 )
				{
					Tcl_AppendResult(interp, "invalid Air code\n",
						"\tNo Changes made\n",
						(char *)NULL );
					fclose( fp );
					return( 1 );
				}
				if( air == 0 && id == 0 )
					Tcl_AppendResult(interp, "Warning: both ID and Air codes are 0!!!\n", (char *)NULL );
				if( air && id )
					Tcl_AppendResult(interp, "Warning: both ID and Air codes are non-zero!!!\n", (char *)NULL );
			}
			continue;
		}

		ptr = strtok( line , delims );
		/* First non-white is the relation operator */
		relation = (*ptr);
		if( relation == '\0' )
		{
			done = 1;
			break;
		}

		/* Next must be the member name */
		ptr = strtok( (char *)NULL, delims );
		strncpy( name , ptr , NAMESIZE );
		name[NAMESIZE] = '\0';

		/* Eliminate trailing white space from name */
		j = NAMESIZE;
		while( isspace( name[--j] ) )
			name[j] = '\0';

		if( relation != '+' && relation != 'u' & relation != '-' )
		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, " %c is not a legal operator\n" , relation );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  fclose( fp );
		  return( -1 );
		}
		if( relation != '-' )
			nonsubs++;

		if( name[0] == '\0' )
		{
		  Tcl_AppendResult(interp, " operand name missing\n", (char *)NULL);
		  fclose( fp );
		  return( -1 );
		}

		node_count++;
	}

	fclose( fp );

	if( nonsubs == 0 && node_count )
	{
	  Tcl_AppendResult(interp, "Cannot create a combination with all subtraction operators\n",
			   (char *)NULL);
	  return( -1 );
	}
	return( node_count );
}

int build_comb( comb, dp, node_count, old_name )
struct rt_comb_internal *comb;
struct directory *dp;
int node_count;
char *old_name;
{
/*	Build the new combination by adding to the recently emptied combination
	This keeps combo info associated with this combo intact */

	FILE *fp;
	char relation;
	char name[NAMESIZE+1];
	char new_name[NAMESIZE+1];
	char line[MAXLINE];
	char *ptr;
	int ch;
	int i;
	int done=0;
	int region;
	struct directory *dp1;
	struct rt_tree_array *rt_tree_array;
	int tree_index=0;
	union tree *tp;
	union tree *final_tree;
	struct rt_db_internal intern;
	matp_t matrix;

	if(dbip == DBI_NULL)
	  return 0;

	if( comb )
	{
		RT_CK_COMB( comb );
		RT_CK_DIR( dp );
	}

	if( (fp=fopen( red_tmpfil , "r" )) == NULL )
	{
	  Tcl_AppendResult(interp, " Cannot open edited file: ",
			   red_tmpfil, "\n", (char *)NULL);
	  return( 1 );
	}

	/* empty the existing combination */
	if( comb && comb->tree )
	{
		db_free_tree( comb->tree );
		comb->tree = NULL;
	}
	else
	{
		/* make an empty combination structure */
		BU_GETSTRUCT( comb, rt_comb_internal );
		comb->magic = RT_COMB_MAGIC;
		comb->tree = TREE_NULL;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );
	}

	/* build tree list */
	if( node_count )
		rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count , sizeof( struct rt_tree_array ) , "tree list" );
	else
		rt_tree_array = (struct rt_tree_array *)NULL;

	if( dp == DIR_NULL )
		NAMEMOVE( old_name, new_name );
	else
		NAMEMOVE( dp->d_namep, new_name );

	/* Read edited file */
	while( !done )
	{
		/* Read a line */
		i = (-1);

		while( (ch=getc( fp )) != EOF && ch != '\n' && i<MAXLINE )
			line[++i] = ch;

		if( ch == EOF )	/* We must be done */
		{
			done = 1;
			break;
		}

		line[++i] = '\0';

		/* skip leading white space */
		i = (-1);
		while( isspace( line[++i] ));

		if( line[i] == '\0' )
			continue;	/* blank line */

		if( (ptr=find_keyword(i, line, "NAME" ) ) )
		{
			NAMEMOVE( ptr, new_name );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "REGION_ID" ) ) )
		{
			comb->region_id = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "REGION" ) ) )
		{
			if( *ptr == 'y' || *ptr == 'Y' )
				comb->region_flag = 'Y';
			else
				comb->region_flag = '\0';
			continue;
		}
		else if( (ptr=find_keyword( i, line, "AIRCODE" ) ) )
		{
			comb->aircode = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "GIFT_MATERIAL" ) ) )
		{
			comb->GIFTmater = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "LOS" ) ) )
		{
			comb->los = atoi( ptr );
			continue;
		}
		else if( (ptr=find_keyword( i, line, "COLOR" ) ) )
		{
			char *ptr2;
			int value;

			ptr2 = strtok( ptr, delims );
			if( !ptr2 )
			{
				comb->rgb_valid = 0;
				continue;
			}
			value = atoi( ptr2 );
			if( value < 0 )
			{
				Tcl_AppendResult(interp, "Red value less than 0, assuming 0\n", (char *)NULL );
				value = 0;
			}
			if( value > 255 )
			{
				Tcl_AppendResult(interp, "Red value greater than 255, assuming 255\n", (char *)NULL );
				value = 255;
			}
			comb->rgb[0] = value;
			ptr2 = strtok( (char *)NULL, delims );
			if( !ptr2 )
			{
				Tcl_AppendResult(interp, "Invalid RGB value\n", (char *)NULL );
				comb->rgb_valid = 0;
				continue;
			}
			value = atoi( ptr2 );
			if( value < 0 )
			{
				Tcl_AppendResult(interp, "Green value less than 0, assuming 0\n", (char *)NULL );
				value = 0;
			}
			if( value > 255 )
			{
				Tcl_AppendResult(interp, "Green value greater than 255, assuming 255\n", (char *)NULL );
				value = 255;
			}
			comb->rgb[1] = value;
			ptr2 = strtok( (char *)NULL, delims );
			if( !ptr2 )
			{
				Tcl_AppendResult(interp, "Invalid RGB value\n", (char *)NULL );
				comb->rgb_valid = 0;
				continue;
			}
			value = atoi( ptr2 );
			if( value < 0 )
			{
				Tcl_AppendResult(interp, "Blue value less than 0, assuming 0\n", (char *)NULL );
				value = 0;
			}
			if( value > 255 )
			{
				Tcl_AppendResult(interp, "Blue value greater than 255, assuming 255\n", (char *)NULL );
				value = 255;
			}
			comb->rgb[2] = value;
			comb->rgb_valid = 1;
			continue;
		}
		else if( (ptr=find_keyword( i, line, "SHADER" ) ) )
		{
			bu_vls_strcpy( &comb->shader,  ptr );
			continue;
		}
#if 0
		else if( (ptr=find_keyword( i, line, "MATERIAL" ) ) )
		{
			bu_vls_strcpy( &comb->material,  ptr );
			continue;
		}
#endif
		else if( (ptr=find_keyword( i, line, "INHERIT" ) ) )
		{
			if( *ptr == 'y' || *ptr == 'Y' )
				comb->inherit = 1;
			else
				comb->inherit = 0;
			continue;
		}
		else if( !strncmp( &line[i], "COMBINATION:", 12 ) )
			continue;

		ptr = strtok( line, delims );

		/* First non-white is the relation operator */
		relation = (*ptr);
		if( relation == '\0' )
		{
			done = 1;
			break;
		}

		/* Next must be the member name */
		ptr = strtok( (char *)NULL, delims );
		strncpy( name , ptr, NAMESIZE );
		name[NAMESIZE] = '\0';

		/* Eliminate trailing white space from name */
		i = NAMESIZE;
		while( isspace( name[--i] ) )
			name[i] = '\0';

		/* Check for existence of member */
		if( (dp1=db_lookup( dbip , name , LOOKUP_QUIET )) == DIR_NULL )
		  Tcl_AppendResult(interp, "\tWARNING: '", name, "' does not exist\n", (char *)NULL);

		/* get matrix */
		ptr = strtok( (char *)NULL, delims );
		if( !ptr )
			matrix = (matp_t)NULL;
		else
		{
			int k;

			matrix = (matp_t)bu_calloc( 16, sizeof( fastf_t ), "red: matrix" );
			matrix[0] = atof( ptr );
			for( k=1 ; k<16 ; k++ )
			{
				ptr = strtok( (char *)NULL, delims );
				if( !ptr )
				{
					Tcl_AppendResult(interp, "incomplete matrix for member ",
						name, " No changes made\n", (char *)NULL );
					bu_free( (char *)matrix, "red: matrix" );
					if( rt_tree_array )
						bu_free( (char *)rt_tree_array, "red: tree list" );
					fclose( fp );
					return( 1 );
				}
				matrix[k] = atof( ptr );
			}
			if( bn_mat_is_identity( matrix ) )
			{
				bu_free( (char *)matrix, "red: matrix" );
				matrix = (matp_t)NULL;
			}
		}

		/* Add it to the combination */
		switch( relation )
		{
			case '+':
				rt_tree_array[tree_index].tl_op = OP_INTERSECT;
				break;
			case '-':
				rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
				break;
			default:
				Tcl_AppendResult(interp, "unrecognized relation (assume UNION)\n",
					(char *)NULL );
			case 'u':
				rt_tree_array[tree_index].tl_op = OP_UNION;
				break;
		}
		BU_GETUNION( tp, tree );
		rt_tree_array[tree_index].tl_tree = tp;
		tp->tr_l.magic = RT_TREE_MAGIC;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup( name );
		tp->tr_l.tl_mat = matrix;
		tree_index++;
	}

	fclose( fp );

	if( tree_index )
		final_tree = (union tree *)db_mkgift_tree( rt_tree_array, node_count, (struct db_tree_state *)NULL );
	else
		final_tree = (union tree *)NULL;

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_type = ID_COMBINATION;
	intern.idb_ptr = (genptr_t)comb;
	comb->tree = final_tree;

	if( strncmp( new_name, old_name, NAMESIZE ) )
	{
		int flags;

		if( comb->region_flag )
			flags = DIR_COMB | DIR_REGION;
		else
			flags = DIR_COMB;

		if( dp != DIR_NULL )
		{
			if(  db_delete( dbip, dp ) || db_dirdelete( dbip, dp ) )
			{
				Tcl_AppendResult(interp, "ERROR: Unable to delete directory entry for ",
					old_name, "\n", (char *)NULL );
				rt_comb_ifree( &intern );
				return( 1 );
			}
		}

		if( (dp=db_diradd( dbip, new_name, -1, node_count+1, flags)) == DIR_NULL )
		{
		  Tcl_AppendResult(interp, "Cannot add ", new_name,
				   " to directory, no changes made\n", (char *)NULL);
		  rt_comb_ifree( &intern );
		  return( 1 );
		}

		if( db_alloc( dbip, dp, node_count+1) )
		{
			Tcl_AppendResult(interp, "Cannot allocate file space for ", new_name,
				"\n", (char *)NULL );
			rt_comb_ifree( &intern );
			return( 1 );
		}
	}
	else if( dp == DIR_NULL )
	{
		int flags;

		if( comb->region_flag )
			flags = DIR_COMB | DIR_REGION;
		else
			flags = DIR_COMB;

		if( (dp=db_diradd( dbip, new_name, -1, node_count+1, flags)) == DIR_NULL )
		{
		  Tcl_AppendResult(interp, "Cannot add ", new_name,
				   " to directory, no changes made\n", (char *)NULL);
		  rt_comb_ifree( &intern );
		  return( 1 );
		}

		if( db_alloc( dbip, dp, node_count+1) )
		{
			Tcl_AppendResult(interp, "Cannot allocate file space for ", new_name,
				"\n", (char *)NULL );
			rt_comb_ifree( &intern );
			return( 1 );
		}
	}

	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )  {
		Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
		return 1;
	}
	return( 0 );
}

void
mktemp_comb( str )
char *str;
{
/* Make a temporary name for a combination
	a template name is expected as in "mk_temp()" with 
	5 trailing X's */

	int counter,done;
	char *ptr;


	if(dbip == DBI_NULL)
	  return;

	/* Set "ptr" to start of X's */

	ptr = str;
	while( *ptr != '\0' )
		ptr++;

	while( *(--ptr) == 'X' );
	ptr++;


	counter = 1;
	done = 0;
	while( !done && counter < 99999 )
	{
		sprintf( ptr , "%d" , counter );
		if( db_lookup( dbip , str , LOOKUP_QUIET ) == DIR_NULL )
			done = 1;
		else
			counter++;
	}
}

int save_comb( dpold )
struct directory *dpold;
{
/* Save a combination under a temporory name */

	register struct directory	*dp;
	struct rt_db_internal		intern;

	if(dbip == DBI_NULL)
	  return 0;

	/* Make a new name */
	mktemp_comb( red_tmpcomb );

	if( rt_db_get_internal( &intern, dpold, dbip, (mat_t *)NULL ) < 0 )
		TCL_READ_ERR_return;

	if( (dp=db_diradd( dbip, red_tmpcomb, -1, dpold->d_len, dpold->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, dpold->d_len ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			   ", no changes made\n", (char *)NULL);
	  return( 1 );
	}

	if( rt_db_put_internal(	dp, dbip, &intern ) < 0 )
	{
		Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			", no changes made\n", (char *)NULL);
		return( 1 );
	}
	
	return( 0 );
}

/* restore a combination that was saved in "red_tmpcomb" */
void
restore_comb( dp )
struct directory *dp;
{
  char *av[4];
  char name[NAMESIZE];

  /* Save name of original combo */
  strcpy( name , dp->d_namep );

  av[0] = "kill";
  av[1] = name;
  av[2] = NULL;
  av[3] = NULL;
  (void)f_kill((ClientData)NULL, interp, 2, av);

  av[0] = "mv";
  av[1] = red_tmpcomb;
  av[2] = name;

  (void)f_name((ClientData)NULL, interp, 3, av);
}
