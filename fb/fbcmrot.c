/*
 *			F B C M R O T . C
 *
 * Function -
 *	Dynamicly rotate the color map
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>	
#include "fb.h"

ColorMap old_map;
ColorMap cm;

#ifdef SYSV
#define bzero(p,cnt)	memset(p,'\0',cnt);
#endif

int size = 512;
int wtime = 0;		/* micro-seconds between updates */

FBIO *fbp;

main(argc, argv )
char **argv;
{
	register int i;

	if( argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h' )  {
		argc--;
		argv++;
		size = 1024;
	}
	if( argc > 1 )  {
		wtime = atoi( argv[1] ) * 1000;	/* ms as arg */
		printf("%d us delay\n", wtime );
	}

	if( (fbp = fb_open( NULL, size, size)) == FBIO_NULL )  {
		fprintf(stderr, "fbcmrot:  fb_open failed\n");
		return	1;
	}
	fb_rmap( fbp, &old_map );
	fb_rmap( fbp, &cm );

	while(1)  {
		register int t;
		/* Build color map for current value */
		t = cm.cm_red[0];
		for( i=0; i<255; i++ )
			cm.cm_red[i] = cm.cm_red[i+1];
		cm.cm_red[255] = t;

		t = cm.cm_green[0];
		for( i=0; i<255; i++ )
			cm.cm_green[i] = cm.cm_green[i+1];
		cm.cm_green[255] = t;

		t = cm.cm_blue[0];
		for( i=0; i<255; i++ )
			cm.cm_blue[i] = cm.cm_blue[i+1];
		cm.cm_blue[255] = t;

		fb_wmap( fbp, &cm );

		if(wtime) delay( 0, wtime );
	}
}

#ifdef BSD
#include <sys/time.h>
delay( s, us )
{
	struct timeval tv;

	tv.tv_sec = s;
	tv.tv_usec = us;
	select( 2, (char *)0, (char *)0, (char *)0, &tv );
}
#else
delay( s, us )
{
	sleep( s + (us/1000000) );
}
#endif
