/*
 *			B O D Y I O . C
 *
 * Functions -
 *	cmd_import_body - read an object's body from a file
 *	cmd_export_body - write an object's body to a file
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.

 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#ifndef WIN32
#include <pwd.h>
#endif
#include <signal.h>
#include <math.h>
#include <time.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

/* defined in librt/db5_bin.c */
extern void rt_binunif_dump();

/*
 *		C M D _ I M P O R T _ B O D Y ( )
 *
 *	Read an object's body from disk file
 *
 */
int
cmd_import_body(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register struct directory	*dp;
    struct stat			stat_buf;
    int				major_code, minor_code;
    int				majc, minc;
    char			*descrip;
    struct bu_vls		vls;
    struct rt_db_internal	intern;
    struct rt_binunif_internal	*bip;
    int				fd;
    int				gotten;

    CHECK_DBI_NULL;

    if(argc != 4){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help %s", argv[0]);
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    switch (sscanf(argv[3], "%d %d", &major_code, &minor_code)) {
	case 1:
	    /* XXX is it safe to have no minor code? */
	    minor_code = 0;
	case 2:
	    break;
	case 0:
	    if ( db5_type_codes_from_descrip(&majc, &minc,
						argv[3])
	     && db5_type_codes_from_tag(&majc, &minc,
						argv[3])) {
		bu_vls_init( &vls );
		bu_vls_printf( &vls,
				"Invalid data type: '%s'\n", argv[3] );
		Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
		bu_vls_free( &vls );
		mged_print_result( TCL_ERROR );
		return TCL_ERROR;
	    }
    	    major_code = majc;
    	    minor_code = minc;
	    break;
    }
    bu_vls_init( &vls );
    if (db5_type_descrip_from_codes( &descrip, major_code, minor_code )) {
	bu_vls_printf( &vls,
			"Invalid maj/min: %d %d\n",
			major_code, minor_code);
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }
    if (RT_G_DEBUG & DEBUG_VOL)
        bu_log( "Type is %d %d '%s'\n", major_code, minor_code, descrip);

    /*
     *	Check to see if we need to create a new object
     */
    if ((dp = db_lookup( dbip, argv[1], LOOKUP_QUIET)) == DIR_NULL) {
	bu_vls_init( &vls );
	bu_vls_printf( &vls, "object \"%s\" does not exist.\n", argv[1] );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;

#if 0
	/* Update the in-core directory */
	if( (dp = db_diradd( dbip, argv[1], -1, 0, 0, NULL ))
		== DIR_NULL ) {
	  Tcl_AppendResult(interp, "An error has occured while adding '",
			   argv[1], "' to the database.\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return TCL_ERROR;
	}

	BU_GETSTRUCT( comb, rt_comb_internal );
	comb->magic = RT_COMB_MAGIC;
	bu_vls_init( &comb->shader );
	bu_vls_init( &comb->material );
	comb->region_id = -1;
	comb->tree = TREE_NULL;
#endif

    } else {
	RT_INIT_DB_INTERNAL( &intern );
    }

    /*
     *	How much data do we have to suck in?
     */
    if( stat( argv[2], &stat_buf ) ) {
	bu_vls_init( &vls );
	bu_vls_printf( &vls, "Cannot get status of file %s\n", argv[2] );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }
    if (RT_G_DEBUG & DEBUG_VOL)
	    bu_log ("File '%s' is %d bytes long\n", argv[2], stat_buf.st_size);
    if( (fd = open( argv[2], O_RDONLY  )) == -1 ) {
	bu_vls_init( &vls );
	bu_vls_printf( &vls,
	    "Cannot open file %s for reading\n", argv[2] );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }

    if (db5_type_descrip_from_codes( &descrip, major_code, minor_code )) {
	bu_vls_printf( &vls,
	    "Invalid major_code/minor_code: %d\n", major_code);
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }
    switch (major_code) {
	case DB5_MAJORTYPE_BINARY_UNIF:
	    bip = bu_malloc(sizeof(struct rt_binunif_internal),
		    "rt_binunif_internal");
	    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
	    bip->type = minor_code;
	    bip->u.uint8 = (unsigned char *) bu_malloc(stat_buf.st_size, "binunif");
    if (RT_G_DEBUG & DEBUG_VOL)
	    bu_log("Created an rt_binunif_internal for type '%s' (minor=%d)\n", descrip, minor_code);

	    gotten = read( fd, (void *) (bip->u.uint8), stat_buf.st_size);
	    if (gotten == -1) {
		perror( "import_body" );
		return TCL_ERROR;
	    } else if (gotten < stat_buf.st_size) {
		bu_vls_init( &vls );
		bu_vls_printf( &vls,
		    "Incomplete read of object %s from file %s, got %d bytes\n",
		    argv[1], argv[2], gotten );
		Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
		bu_vls_free( &vls );
		mged_print_result( TCL_ERROR );
		return TCL_ERROR;
	    }
	    if (RT_G_DEBUG & DEBUG_VOL)
		    bu_log("gotten=%d,  minor_code is %d\n",
			   gotten, minor_code);
	    bip->count = gotten / db5_type_sizeof_n_binu( minor_code );
	    if (RT_G_DEBUG & DEBUG_VOL) {
		    bu_log("Got 'em!\nThink I own %d of 'em\n", bip->count);
		    fflush(stderr);
	    }
	    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    intern.idb_type = minor_code;
	    intern.idb_meth = &rt_functab[ID_BINUNIF];
	    intern.idb_ptr = (genptr_t)bip;
	    rt_binunif_dump(bip);
	    rt_db_put_internal5( dp, dbip, &intern, &rt_uniresource, DB5_MAJORTYPE_BINARY_UNIF );
	    rt_db_free_internal( &intern, &rt_uniresource );
	    break;
	default:
	    bu_vls_printf( &vls,
		"Haven't gotten around to supporting type: %s\n",
		descrip);
	    Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	    bu_vls_free( &vls );
	    mged_print_result( TCL_ERROR );
	    return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *		C M D _ E X P O R T _ B O D Y ( )
 *
 *	Write an object's body to disk file
 *
 */
int
cmd_export_body(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register struct directory	*dp;
    int				fd;
    void			*bufp;
    size_t			nbytes = 0;
    int				written;
    struct bu_external		ext;
    struct db5_raw_internal	raw;
    struct rt_db_internal	intern;
    struct rt_binunif_internal	*bip;
    struct bu_vls		vls;
#if 0
    int				status;
#endif
    char			*tmp;

    CHECK_DBI_NULL;

    if(argc != 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help %s", argv[0]);
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /*
     *	Find the guy we're told to write
     */
    if( (dp = db_lookup( dbip, argv[2], LOOKUP_NOISY)) == DIR_NULL ){
	bu_vls_init( &vls );
	bu_vls_printf( &vls,
	    "Cannot find object %s for writing\n", argv[2] );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }
    RT_INIT_DB_INTERNAL(&intern);
    if ( rt_db_get_internal5( &intern, dp, dbip, NULL, &rt_uniresource) 
	!= ID_BINUNIF
     || db_get_external( &ext, dp, dbip ) < 0 ) {
	(void)signal( SIGINT, SIG_IGN );
	TCL_READ_ERR_return;
    }
    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL)
    {
	bu_free_external( &ext );
	(void)signal( SIGINT, SIG_IGN );
	TCL_READ_ERR_return;
    }

    /*
     *	Do the writing
     */
#ifndef WIN32
    if( (fd = creat( argv[1], S_IRWXU | S_IRGRP | S_IROTH  )) == -1 ) {
#else
		if( (fd = creat( argv[1], _S_IREAD | _S_IWRITE  )) == -1 ) { 
#endif
	bu_free_external( &ext );
	bu_vls_init( &vls );
	bu_vls_printf( &vls,
	    "Cannot open file %s for writing\n", argv[1] );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }
    if (db5_type_descrip_from_codes(&tmp, raw.major_type, raw.minor_type))
	tmp = 0;

    if (RT_G_DEBUG & DEBUG_VOL)
	    bu_log("cmd_export_body() sees type (%d, %d)='%s'\n",
		   raw.major_type, raw.minor_type, tmp);
    switch (raw.major_type) {
	case DB5_MAJORTYPE_BINARY_UNIF:
#if 0
	    if ( rt_binunif_import5( &intern, &ext, 0, dbip,
		    &rt_uniresource, raw.minor_type  ) ) {
		(void)signal( SIGINT, SIG_IGN );
		TCL_READ_ERR_return;
	    }
#endif
	    bip = (struct rt_binunif_internal *) intern.idb_ptr;
	    RT_CK_BINUNIF(bip);
	    rt_binunif_dump(bip);
	    bufp = (void *) bip->u.uint8;
	    bu_log("cmd_export_body() thinks bip->count=%d\n", bip->count);
	    switch (bip -> type) {
		case DB5_MINORTYPE_BINU_FLOAT:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... float");
		    nbytes = (size_t) (bip->count * sizeof(float));
		    break;
		case DB5_MINORTYPE_BINU_DOUBLE:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... double");
		    nbytes = (size_t) (bip->count * sizeof(double));
		    break;
		case DB5_MINORTYPE_BINU_8BITINT:
		case DB5_MINORTYPE_BINU_8BITINT_U:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... 8bitint");
		    nbytes = (size_t) (bip->count);
		    break;
		case DB5_MINORTYPE_BINU_16BITINT:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... 16bitint");
		    nbytes = (size_t) (bip->count * 2);
		    bu_log("data[0] = %u\n", bip->u.uint16[0]);
		    break;
		case DB5_MINORTYPE_BINU_16BITINT_U:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... 16bituint");
		    nbytes = (size_t) (bip->count * 2);
		    bu_log("data[0] = %u\n", bip->u.uint16[0]);
		    break;
		case DB5_MINORTYPE_BINU_32BITINT:
		case DB5_MINORTYPE_BINU_32BITINT_U:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... 32bitint");
		    nbytes = (size_t) (bip->count * 4);
		    break;
		case DB5_MINORTYPE_BINU_64BITINT:
		case DB5_MINORTYPE_BINU_64BITINT_U:
			if (RT_G_DEBUG & DEBUG_VOL)
				bu_log("bip->type switch... 64bitint");
		    nbytes = (size_t) (bip->count * 8);
		    break;
		default:
		    /* XXX	This shouln't happen!!    */
		    bu_log("bip->type switch... default");
		    break;
	    }
	    break;
	default:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("I'm in the default\n");
		bufp = (void *) ext.ext_buf;
		nbytes = (size_t) ext.ext_nbytes;
		break;
    }
    if (RT_G_DEBUG & DEBUG_VOL)
	    bu_log("going to write %ld bytes\n", nbytes);

    if ( (written = write(fd, bufp, nbytes) ) != nbytes ) {
    	perror(argv[1]);
	    bu_log("%s:%d\n", __FILE__, __LINE__);
	bu_free_external( &ext );
	bu_vls_init( &vls );
	bu_vls_printf( &vls,
	    "Incomplete write of object %s to file %s, got %ld, s/b=%ld\n",
	    argv[2], argv[1], written, nbytes );
	Tcl_SetResult(interp, bu_vls_addr( &vls ), TCL_VOLATILE );
	bu_vls_free( &vls );
	mged_print_result( TCL_ERROR );
	return TCL_ERROR;
    }

    bu_free_external( &ext );
    return TCL_OK;
}
