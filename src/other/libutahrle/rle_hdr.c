/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/*
 * rle_hdr.c - Functions to manipulate rle_hdr structures.
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Mon May 20 1991
 * Copyright (c) 1991, University of Michigan
 */
static char rcsid[] = "$Header$";

#include "rle.h"

/*****************************************************************
 * TAG( rle_names )
 *
 * Load program and file names into header.
 * Inputs:
 * 	the_hdr:	Header to modify.
 * 	pgmname:	The program name.
 * 	fname:		The file name.
 * 	img_num:	Number of the image within the file.
 * Outputs:
 * 	the_hdr:	Modified header.
 * Algorithm:
 * 	If values previously filled in (by testing is_init field),
 * 	free them.  Make copies of file name and program name,
 * 	modifying file name for standard i/o.  Set is_init field.
 */
void
rle_names( the_hdr, pgmname, fname, img_num )
rle_hdr *the_hdr;
CONST_DECL char *pgmname;
CONST_DECL char *fname;
int img_num;
{
#if 0
    /* Can't do this because people do hdr1 = hdr2, which copies
       the pointers. */

    /* If filled in, free previous values. */
    if ( the_hdr->is_init == RLE_INIT_MAGIC &&
	 the_hdr->cmd != NULL && the_hdr->file_name != NULL )
    {
	if ( pgmname != the_hdr->cmd )
	    free( the_hdr->cmd );
	if ( fname != the_hdr->file_name )
	    free( the_hdr->file_name );
    }
#endif

    /* Mark as filled in. */
    the_hdr->is_init = RLE_INIT_MAGIC;

    /* Default file name for stdin/stdout. */
    if ( fname == NULL || strcmp( fname, "-" ) == 0 || *fname == '\0' )
	fname = "Standard I/O";
    if ( pgmname == NULL )
	pgmname = rle_dflt_hdr.cmd;

    /* Fill in with copies of the strings. */
    if ( the_hdr->cmd != pgmname )
    {
	char *tmp = (char *)malloc( strlen( pgmname ) + 1 );
	RLE_CHECK_ALLOC( pgmname, tmp, 0 );
	strcpy( tmp, pgmname );
	the_hdr->cmd = tmp;
    }

    if ( the_hdr->file_name != fname )
    {
	char *tmp = (char *)malloc( strlen( fname ) + 1 );
	RLE_CHECK_ALLOC( pgmname, tmp, 0 );
	strcpy( tmp, fname );
	the_hdr->file_name = tmp;
    }

    the_hdr->img_num = img_num;
}


/* Used by rle_hdr_cp and rle_hdr_init to avoid recursion loops. */
static int no_recurse = 0;

/*****************************************************************
 * TAG( rle_hdr_cp )
 *
 * Make a "safe" copy of a rle_hdr structure.
 * Inputs:
 * 	from_hdr:	Header to be copied.
 * Outputs:
 * 	to_hdr:		Copy of from_hdr, with all memory referred to
 * 			by pointers copied.  Also returned as function
 * 			value.  If NULL, a static header is used.
 * Assumptions:
 * 	It is safe to call rle_hdr_init on to_hdr.
 * Algorithm:
 * 	Initialize to_hdr, copy from_hdr to it, then copy the memory
 * 	referred to by all non-null pointers.
 */
rle_hdr *
rle_hdr_cp( from_hdr, to_hdr )
rle_hdr *from_hdr, *to_hdr;
{
    static rle_hdr dflt_hdr;
    CONST_DECL char *cmd, *file;
    int num;

    /* Save command, file name, and image number if already initialized. */
    if ( to_hdr &&  to_hdr->is_init == RLE_INIT_MAGIC )
    {
	cmd = to_hdr->cmd;
	file = to_hdr->file_name;
	num = to_hdr->img_num;
    }
    else
    {
	cmd = file = NULL;
	num = 0;
    }

    if ( !no_recurse )
    {
	no_recurse++;
	rle_hdr_init( to_hdr );
	no_recurse--;
    }

    if ( to_hdr == NULL )
	to_hdr = &dflt_hdr;

    *to_hdr = *from_hdr;

    if ( to_hdr->bg_color )
    {
	int size = to_hdr->ncolors * sizeof(int);
	to_hdr->bg_color = (int *)malloc( size );
	RLE_CHECK_ALLOC( to_hdr->cmd, to_hdr->bg_color, "background color" );
	bcopy( from_hdr->bg_color, to_hdr->bg_color, size );
    }

    if ( to_hdr->cmap )
    {
	int size = to_hdr->ncmap * (1 << to_hdr->cmaplen) * sizeof(rle_map);
	to_hdr->cmap = (rle_map *)malloc( size );
	RLE_CHECK_ALLOC( to_hdr->cmd, to_hdr->cmap, "color map" );
	bcopy( from_hdr->cmap, to_hdr->cmap, size );
    }

    /* Only copy array of pointers, as the original comment memory
     * never gets overwritten.
     */
    if ( to_hdr->comments )
    {
	int size = 0;
	CONST_DECL char **cp;
	for ( cp=to_hdr->comments; *cp; cp++ )
	    size++;		/* Count the comments. */
	/* Check if there are really any comments. */
	if ( size )
	{
	    size++;		/* Copy the NULL pointer, too. */
	    size *= sizeof(char *);
	    to_hdr->comments = (CONST_DECL char **)malloc( size );
	    RLE_CHECK_ALLOC( to_hdr->cmd, to_hdr->comments, "comments" );
	    bcopy( (const void *)from_hdr->comments, (void *)to_hdr->comments, size );
	}
	else
	    to_hdr->comments = NULL;	/* Blow off empty comment list. */
    }

    /* Restore the names to their original values. */
    to_hdr->cmd = cmd;
    to_hdr->file_name = file;

    /* Lines above mean nothing much happens if cmd and file are != NULL. */
    rle_names( to_hdr, to_hdr->cmd, to_hdr->file_name, num );

    return to_hdr;
}

/*****************************************************************
 * TAG( rle_hdr_clear )
 *
 * Clear out the allocated memory pieces of a header.
 *
 * This routine is intended to be used internally by the library, to
 * clear a header before putting new data into it.  It clears all the
 * fields that would be set by reading in a new image header.
 * Therefore, it does not clear the program and file names.
 *
 * Inputs:
 * 	the_hdr:	To be cleared.
 * Outputs:
 * 	the_hdr:	After clearing.
 * Assumptions:
 * 	If is_init field is RLE_INIT_MAGIC, the header has been
 * 	properly initialized.  This will fail every 2^(-32) times, on
 * 	average.
 * Algorithm:
 * 	Free memory and set to zero all pointers, except program and
 * 	file name.
 */
void
rle_hdr_clear( the_hdr )
rle_hdr *the_hdr;
{
    /* Try to free memory.  Assume if is_init is properly set that this
     * header has been previously initialized, therefore it is safe to
     * free memory.
     */
    if ( the_hdr && the_hdr->is_init == RLE_INIT_MAGIC )
    {
	if ( the_hdr->bg_color )
	    free( the_hdr->bg_color );
	the_hdr->bg_color = 0;
	if ( the_hdr->cmap )
	    free( the_hdr->cmap );
	the_hdr->cmap = 0;
	/* Unfortunately, we don't know how to free the comment memory. */
	if ( the_hdr->comments )
           free( (void *)the_hdr->comments );
	the_hdr->comments = 0;
    }
}



/*****************************************************************
 * TAG( rle_hdr_init )
 *
 * Initialize a rle_hdr structure.
 * Inputs:
 * 	the_hdr:	Header to be initialized.
 * Outputs:
 * 	the_hdr:	Initialized header.
 * Assumptions:
 * 	If the_hdr->is_init is RLE_INIT_MAGIC, the header has been
 * 	previously initialized.
 * 	If the_hdr is a copy of another rle_hdr structure, the copy
 * 	was made with rle_hdr_cp.
 * Algorithm:
 *	If the_hdr is rle_dflt_hdr, do nothing!
 *	If the_hdr is NULL, return a copy of rle_dflt_hdr.
 * 	If the_hdr->is_init is RLE_INIT_MAGIC, free all memory
 * 	pointed to by non-null pointers.
 * 	Copy rle_dflt_hdr into the_hdr.
 */
rle_hdr *
rle_hdr_init( the_hdr )
rle_hdr *the_hdr;
{
    rle_hdr *ret_hdr;

    if ( rle_dflt_hdr.rle_file == NULL)
    	rle_dflt_hdr.rle_file = stdout;
    if ( the_hdr == &rle_dflt_hdr )
	return the_hdr;

    rle_hdr_clear( the_hdr );

    /* Only call rle_hdr_cp if not called from there. */
    if ( !no_recurse )
    {
	no_recurse++;
	ret_hdr = rle_hdr_cp( &rle_dflt_hdr, the_hdr );
	no_recurse--;
    }
    else
	ret_hdr = the_hdr;

    return ret_hdr;
}
