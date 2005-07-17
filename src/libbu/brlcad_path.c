/*                   B R L C A D _ P A T H . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brlcad_path.c
 *
 *  A support routine to provide the executable code with the path
 *  to where the BRL-CAD programs and libraries are installed.
 *
 *  Author -
 *	Christopher Sean Morrison
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 */
static const char RCSbrlcad_path[] = "@(#)$Header$ (BRL)";

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "machine.h"
#include "bu.h"

#ifndef MAXPATHLEN
#  define MAXPATHLEN 1024
#endif


/*
 *			B U _ F I L E _ E X I S T S
 *
 *  Returns boolean -
 *	1	The given filename exists.
 *	0	The given filename does not exist.
 */
int
bu_file_exists(const char *path)
{
	struct	stat	sbuf;

	if( path == NULL )  return 0;			/* FAIL */

	/* defined in unistd.h */
#if defined(F_OK)
	if( access( path, F_OK )  == 0 )  return 1;	/* OK */
#endif

	/* does it exist as a filesystem entity? */
	if( stat( path, &sbuf ) == 0 )  return 1;	/* OK */

	return 0;					/* FAIL */
}


/* print out an error/warning message if we cannot find the specified
 * paths.
 */
static void
bu_path_results(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD is installed while searching:\n\
%s\n\
This version of BRL-CAD (%s) was compiled to be installed at:\n\
	%s\n\n", paths, BRLCAD_VERSION, BRLCAD_ROOT);

#ifndef _WIN32
    bu_log("\
You may specify where to locate BRL-CAD by setting the BRLCAD_ROOT\n\
environment variable.\n\
\n\
For csh/tcsh users:\n\
	setenv BRLCAD_ROOT /path/to/brlcad\n\
For sh/bash users:\n\
	BRLCAD_ROOT=/path/to/brlcad\n\
	export BRLCAD_ROOT\n\n");
#endif

    return;
}


/* put a left-hand and right-hand path together and test whether they
 * exist or not.  returns boolean on whether a match was found.
 */
int
bu_find_path(char result[MAXPATHLEN], const char *lhs, const char *rhs, struct bu_vls *searched, const char *where)
{
    int llen,rlen;

    /* swap right with left if there is no left so logic is simplified
     * later on.
     */
    if (lhs == NULL && rhs != NULL) {
	lhs = rhs;
	rhs = NULL;
    }

    if (lhs == NULL) {
	bu_vls_strcat(searched, where);
	return 0;
    }

    rlen = llen = 0;
    if (rhs) {
	rlen = strlen(rhs);
    }
    
    /* be safe */
    if (rlen + 2 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%d characters > %d)\n", rlen+2, MAXPATHLEN);
	bu_vls_strcat(searched, where);
	return 0;
    }
    if (!searched || !where) {
	bu_bomb("someone is using the presumably static bu_find_path function wrong");
    }
        
    /* left-hand path should exist independent of right-hand path */
    if (!bu_file_exists(lhs)) {
	bu_vls_strcat(searched, where);
	return 0;
    }
    
    /* start fresh */
    memset(result, 0, MAXPATHLEN);
    strncpy(result, lhs, MAXPATHLEN);
    
    /* nothing to add, so just return what we have */
    if (!rhs) {
	return 1;
    }
    
    /* be safe again */
    llen = strlen(result);
    if (llen + rlen + 3 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%d characters > %d)\n", llen+rlen+3, MAXPATHLEN);
	bu_vls_strcat(searched, where);
	return 0;
    }

    if ( (*(result+llen-1) != BU_DIR_SEPARATOR) && (rhs[0] != BU_DIR_SEPARATOR) ) {
	/* let the caller give "/usr/brlcad" and "bin" and get "/usr/brlcad/bin" */
	*(result+llen) = BU_DIR_SEPARATOR;
	llen++;
    } else if ( (*(result+llen-1) == BU_DIR_SEPARATOR) && (rhs[0] == BU_DIR_SEPARATOR) ) {
	/* let the caller give "/usr/brlcad/" and "/bin" and get "/usr/brlcad/bin"*/
	rhs++;
	rlen--;
    }
        
    /* found a match */
   strncpy(result+llen, rhs, MAXPATHLEN - llen);
    if (bu_file_exists(result)) {
	return 1;
    }
    
    /* close, but no match */
    bu_vls_strcat(searched, where);
    return 0;
}


/*	B U _ B R L C A D _ R O O T
 *
 * Locate where the BRL-CAD applications and libraries are installed.
 *
 * The BRL-CAD root is searched for in the following order of
 * precedence by testing for the rhs existence if provided or the
 * directory existence otherwise:
 *
 *   BRLCAD_ROOT environment variable if set
 *   BRLCAD_ROOT compile-time path
 *   [ Unimplemented: run-time identification ]
 *   /usr/brlcad static path
 *   current directory
 *
 * A STATIC buffer is returned.
 * It is the caller's responsibility to call bu_strdup() or make
 * other provisions to save the returned string, before calling again.
 */
char *
bu_brlcad_root(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    struct bu_vls searched;
    char where[MAXPATHLEN + 64] = {0};

    bu_vls_init(&searched);

    /* BRLCAD_ROOT environment variable if set */
    lhs = getenv("BRLCAD_ROOT");
    if (lhs) {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT environment variable [%s]\n", lhs);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    } else {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* BRLCAD_ROOT compile-time path */
    lhs = BRLCAD_ROOT;
    if (lhs) {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT compile-time path [%s]\n", lhs);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    } else {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT compile-time path [UNKNOWN]\n");
	bu_vls_strcat(&searched, where);
    }

    /* [ Unimplemented: run-time identification ] */
    /* ... */

    /* /usr/brlcad static path */
    if (strncmp("/usr/brlcad", BRLCAD_ROOT, 12) != 0) {
	if (bu_find_path(result, "/usr/brlcad", rhs, &searched, "\t/usr/brlcad default path\n")) {
	    return result;
	}
    }

    /* current directory */
    if (bu_find_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	return result;
    }

    if (!fail_quietly) {
	bu_path_results(bu_vls_addr(&searched));
	if (rhs) {
	    bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n", rhs);
	} else {
	    bu_log("Unable to find the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
	}
    }
    return NULL;
}


/*	B U _ B R L C A D _ D A T A
 *
 * Locate where the BRL-CAD data resources are installed.
 *
 * The BRL-CAD data resources are searched for in the following order
 * of precedence by testing for the existence of rhs if provided or
 * the directory existence otherwise:
 *
 *   BRLCAD_DATA environment variable if set
 *   BRLCAD_DATA compile-time path
 *   bu_brlcad_root/share/brlcad/VERSION path
 *   bu_brlcad_root/share/brlcad path
 *   bu_brlcad_root path
 *   current directory/src
 *   current directory
 *
 * A STATIC buffer is returned.
 * It is the caller's responsibility to call bu_strdup() or make
 * other provisions to save the returned string, before calling again.
 */
char *
bu_brlcad_data(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    struct bu_vls searched;
    char where[MAXPATHLEN + 64] = {0};
    char path[64] = {0};

    bu_vls_init(&searched);

    /* BRLCAD_DATA environment variable if set */
    lhs = getenv("BRLCAD_DATA");
    if (lhs) {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_DATA environment variable [%s]\n", lhs);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    } else {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_DATA environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* BRLCAD_DATA compile-time path */
    lhs = BRLCAD_DATA;
    if (lhs) {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_DATA compile-time path [%s]\n", lhs);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    } else {
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_DATA compile-time path [UNKNOWN]\n");
	bu_vls_strcat(&searched, where);
    }

    /* bu_brlcad_root/share/brlcad/VERSION path */
    snprintf(path, MAXPATHLEN, "share/brlcad/%s", BRLCAD_VERSION);
    lhs = bu_brlcad_root(path, fail_quietly);
    if (lhs) {	
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT common data path [%s]\n", path);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    }
	
    /* bu_brlcad_root/share/brlcad path */
    lhs = bu_brlcad_root("share/brlcad", fail_quietly);
    if (lhs) {	
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT common data path [%s]\n", path);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    }
	
    /* bu_brlcad_root/share path */
    lhs = bu_brlcad_root("share", fail_quietly);
    if (lhs) {	
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT common data path [%s]\n", path);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    }

    /* bu_brlcad_root path */
    lhs = bu_brlcad_root("", fail_quietly);
    if (lhs) {	
	snprintf(where, MAXPATHLEN + 64, "\tBRLCAD_ROOT common data path [%s]\n", path);
	if (bu_find_path(result, lhs, rhs, &searched, where)) {
	    return result;
	}
    }

    /* current directory/src (running from uninstalled source distribution) */
    if (bu_find_path(result, "./src", rhs, &searched, "\t'src' in current directory\n")) {
	return result;
    }

    /* current directory (running from uninstalled source distribution) */
    if (bu_find_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	return result;
    }

    if (!fail_quietly) {
	bu_path_results(bu_vls_addr(&searched));
	bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
    }
    return NULL;
}

/*
 *	B U _ B R L C A D _ P A T H
 *
 *  Locate where the BRL-CAD programs and libraries are located,
 *  contatenate on the rest of the string provided by the caller,
 *  and return a pointer to a STATIC buffer with the full path.
 *  It is the caller's responsibility to call bu_strdup() or make
 *  other provisions to save the returned string, before calling again.
 *  bu_bomb() if unable to find the base path.
 *
 */
char *
bu_brlcad_path(const char *rhs, int fail_quietly)
{
	bu_log("\
WARNING: bu_brlcad_path is deprecated and will likely disappear in\n\
a future release of BRL-CAD.  Programs and scripts should utilize\n\
bu_brlcad_root and bu_brlcad_data instead.\n\
\n\
Use bu_brlcad_root for the path of applications and libraries.\n\
Use bu_brlcad_data for the path to the data resources.\n\n");

	return bu_brlcad_root(rhs, fail_quietly);
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
