/*                         S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup bu_log */
/** @{ */
/** @file stat.c
 *
 *  Support routines for identifying properties of files and
 *  directories such as whether they exist or are the same as another
 *  given file.
 *
 *  @author
 *	Christopher Sean Morrison
 *
 * @par  Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
static const char RCS_stat[] = "@(#)$Header$";

#include "common.h"

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "machine.h"
#include "bu.h"


/**
 *			B U _ F I L E _ E X I S T S
 *
 *  @return	1	The given filename exists.
 *  @return	0	The given filename does not exist.
 */
int
bu_file_exists(const char *path)
{
    if (bu_debug) {
	printf("Does %s exist? ", path);
    }

    if(!path) {
	if (bu_debug) {
	    printf("NO\n");
	}
	/* FAIL */
	return 0;
    }

#if defined(HAVE_ACCESS) && defined(F_OK)
#  define bu_file_exists_method 1
    /* access() is posix */
    if( access( path, F_OK )  == 0 ) {
	if (bu_debug) {
	    printf("YES\n");
	}
	/* OK */
	return 1;
    }
#endif

    /* does it exist as a filesystem entity? */
#if defined(HAVE_STAT)
#  define bu_file_exists_method 1
    {
	struct stat sbuf;
	/* stat() is posix */
	if( stat( path, &sbuf ) == 0 ) {
	    if (bu_debug) {
		printf("YES\n");
	    }
	    /* OK */
	    return 1;
	}
    }
#endif

#ifndef bu_file_exists_method
#  error "Do not know how to check whether a file exists on this system"
#endif

    if (bu_debug) {
	printf("NO\n");
    }
    /* FAIL */
    return 0;
}


/**
 * b u _ s a m e _ f i l e
 *
 * returns truthfully as to whether or not the two provided filenames
 * are the same file.  if either file does not exist, the result is
 * false.
 */
int
bu_same_file(const char *fn1, const char *fn2)
{
    if (!fn1 || !fn2) {
	return 0;
    }
    
    if (!bu_file_exists(fn1) || !bu_file_exists(fn2)) {
	return 0;
    }

#if defined(HAVE_STAT)
#  define bu_same_file_method 1
    {
	struct stat sb1, sb2;
	if ((stat(fn1, &sb1) == 0) &&
	    (stat(fn2, &sb2) == 0) &&
	    (sb1.st_dev == sb2.st_dev) &&
	    (sb1.st_ino == sb2.st_ino)) {
	    return 1;
	}
    }
#endif

#ifndef bu_same_file_method
#  error "Do not know how to test if two files are the same on this system"
#endif
    
    return 0;
}


/**
 * b u _ s a m e _ f d
 *
 * returns truthfully as to whether or not the two provided file
 * descriptors are the same file.  if either file does not exist, the
 * result is false.
 */
int
bu_same_fd(int fd1, int fd2)
{
    if (fd1<0 || fd2<0) {
	return 0;
    }

    /* use HAVE_STAT configure test for now until we find a need to
     * test for HAVE_FSTAT instead.
     */
#if defined(HAVE_STAT)
#  define bu_same_file_method 1
    {
	struct stat sb1, sb2;
	if ((fstat(fd1, &sb1) == 0) &&
	    (fstat(fd2, &sb2) == 0) &&
	    (sb1.st_dev == sb2.st_dev) &&
	    (sb1.st_ino == sb2.st_ino)) {
	    return 1;
	}
    }
#endif

#ifndef bu_same_file_method
#  error "Do not know how to test if two files are the same on this system"
#endif
    
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
