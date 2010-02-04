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
 * rle_addhist.c - Add to the HISTORY comment in header
 *
 * Author:	Andrew Marriott.
 * 		School of Computer Science
 * 		Curtin University of Technology
 * Date:	Mon Sept 10 1988
 * Copyright (c) 1988, Curtin University of Technology
 */

#include "rle.h"
#include <stdio.h>

#ifdef	USE_TIME_H
#include <time.h>
#else
#include <sys/types.h>
#include <sys/time.h>
#endif

/*****************************************************************
 * TAG( rle_addhist )
 *
 * Put a history comment into the header struct.
 * Inputs:
 * 	argv:		Command line history to add to comments.
 *	in_hdr:		Incoming header struct to use.
 * Outputs:
 *	out_hdr:	Outgoing header struct to add to.
 * Assumptions:
 * 	If no incoming struct then value is NULL.
 * Algorithm:
 * 	Calculate length of all comment strings, malloc and then set via
 *	rle_putcom.
 */

void rle_addhist(argv,in_hdr,out_hdr)
register char	*argv[];
rle_hdr *in_hdr,*out_hdr;
{
	register int	i;
	register size_t	length;
	time_t	temp;
	/* padding must give number of characters in histoire 	*/
	/*     plus one for "="					*/
	static CONST_DECL char	*histoire="HISTORY",
				*padding="\t";
	char	*timedate,*old= NULL;
	static char	*newc;

	if(getenv("NO_ADD_RLE_HISTORY"))return;

	length=0;
	for(i=0;argv[i];i++)
		length+= strlen(argv[i]) +1;					/* length of each arg plus space. */

	(void)time (&temp);
	timedate=ctime(&temp);
	length+= strlen(timedate);						/* length of date and time in ASCII. */

	length+= strlen(padding) + 3 + strlen(histoire) + 1;			/* length of padding, "on "  and length of history name plus "="*/
	if(in_hdr)								/* if we are interested in the old comments... */
		old=rle_getcom(histoire,in_hdr);				/* get old comment. */

	if((old) && (*old)) length+= strlen(old);				/* add length if there. */

	length++;								/*Cater for the null. */

	if((newc=(char *)malloc((unsigned int) length)) == NULL)return;

	(void)strcpy(newc,histoire);(void)strcat(newc,"=");
	if((old) && (*old)) (void)strcat(newc,old); /* add old string if there. */
	for(i=0;argv[i];i++)
	{
		(void)strcat(newc,argv[i]);(void)strcat(newc," ");
	}
	(void)strcat(newc,"on ");(void)strcat(newc,timedate);			/* \n supplied by time. */
	(void)strcat(newc,padding);							/* to line up multiple histories.*/

	(void)rle_putcom(newc,out_hdr);

}
