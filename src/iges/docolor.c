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
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

/*	This routine loops through all the directory entries and calls
	and sets colors according to the directory entry field #13 */

#include "./iges_struct.h"
#include "./iges_extern.h"

unsigned char colortab[9][4]={
	{ 0 , 217 , 217 , 217 },
	{ 1 ,   0 ,   0 ,   0 },
	{ 2 , 255 ,   0 ,   0 },
	{ 3 ,   0 , 255 ,   0 },
	{ 4 ,   0 ,   0 , 255 },
	{ 5 , 255 , 255 ,   0 },
	{ 6 , 255 ,   0 , 255 },
	{ 7 ,   0 , 255 , 255 },
	{ 8 , 255 , 255 , 255 }};

void
Docolor()
{

	int i,j;
	fastf_t a;

	for( i=0 ; i<totentities ; i++ )
	{
		/* only set colors for regions, groups, or solid instances */
		if( dir[i]->type == 180 || dir[i]->type == 184 || dir[i]->type == 430 )
		{
			if( dir[i]->colorp > 0 ) /* just use color table */
			{
				dir[i]->rgb[0] = colortab[dir[i]->colorp][1];
				dir[i]->rgb[1] = colortab[dir[i]->colorp][2];
				dir[i]->rgb[2] = colortab[dir[i]->colorp][3];
			}
			else if( dir[i]->colorp < 0 )
			{
				/* Use color definition entity */
				Readrec( dir[-dir[i]->colorp]->param );
				Readint( &j , "" );
				if( j != 314 )
				{
					bu_log( "Incorrect color parameters for entity %d\n" , i );
					bu_log( "\tcolor entity is #%d\n" , -dir[i]->colorp );
					continue;
				}
				Readflt( &a , "" );
				dir[i]->rgb[0] = 2.55 * a + 0.5;
				Readflt( &a , "" );
				dir[i]->rgb[1] = 2.55 * a + 0.5;
				Readflt( &a , "" );
				dir[i]->rgb[2] = 2.55 * a + 0.5;
			}
		}
	}
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
