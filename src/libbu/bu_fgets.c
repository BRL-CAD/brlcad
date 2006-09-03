/*                   B U _ F G E T S
 * BRL-CAD
 *
 * Copyright (c) 1995-2006 United States Government as represented by
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

/** \addtogroup bu_log */
/*@{*/
/** @file bu_fgets.c
 *@brief
 * fgets replacement function that also handles CR as an EOL marker
 *
 *  @Author John Anderson
 *
 *  @par Source
 *	The U. S. Army Research Laboratory			@n
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char libbu_fgets_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#if HAVE_STRING_H
#  include <string.h>
#endif

#include "machine.h"
#include "bu.h"


/**
 *			 B U _ F G E T S
 *
 *	    Reads in at most one less than size characters from stream and
 *      stores them into the buffer pointed to by s. Reading stops after an
 *      EOF, CR, LF, or a CR/LF combination. If a LF or CR is read,
 *	it is stored into the buffer. If a CR/LF is read, just a CR is
 *	stored into the buffer. A '\\0' is stored after the last
 *	character in the buffer. Returns s on success, and NULL on error or
 *	when end of file occurs while no characters have been read.
 */

char *
bu_fgets(char *s, int size, FILE *stream)
{
  int totBytesRead = 0;
  int isEOF = 0;

  /* if we are not asked to read anything, just return */
  if(size < 1) {
      return s;
  }

  /* if the buffer size is one, we have no space (we add a null)
   * so just return
   */
  if(size == 1) {
    *s = '\0';
    return s;
  }

  /* check for EOF or error */
  if(feof(stream) || ferror(stream)) {
      *s = '\0';
      return (char *)NULL;
  }

  /* actually do some reading */
  while(totBytesRead < size - 1) {
    int c;

    c = fgetc(stream);
    if(c == EOF) {
      isEOF = 1;
      break;
    }

    s[totBytesRead++] = c;

    /* check for newline */
    if(c == '\n') {
      break;
    }

    /* chech for CR */
    if(c == '\r') {

	/* check for CR/LF combination */
	c = fgetc(stream);
	if(c != '\n') {
	    /* not a CR/LF, so unget the last char */
	    ungetc(c, stream);
	}

	break;
    }
  }

  /* add our null */
  s[totBytesRead] = '\0';

  if(isEOF && totBytesRead == 0)
    return (char *)NULL;
  else
    return s;
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
