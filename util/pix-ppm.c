
Received: from vgr.brl.mil by WOLF.BRL.MIL id aa26015; 19 Jan 91 3:38 EST
Received: from iuvax.cs.indiana.edu by VGR.BRL.MIL id aa28346;
          19 Jan 91 3:19 EST
Received: by iuvax.cs.indiana.edu 
Date: Sat, 19 Jan 91 03:20:32 -0500
From: Steve Hayman <sahayman@iuvax.cs.indiana.edu>
To: mike@BRL.MIL
Subject: Re:  War Images
Message-ID:  <9101190319.aa28346@VGR.BRL.MIL>

Here it is, it's pretty basic and only handles the images at
the specific resolution you posted.  Just a 10-minute hack,
but I suppose it would be easy to add options to select
different widths/heights.

#include <stdio.h>

/*
 * convert BRL .pix files to ppm
 * zcat file.Z | pixtoppm
 * sahayman 1991 01 18
 *
 * brl says this:
 *
 * These compressed images are in BRL-CAD .pix file format.
 * They are 24-bits/pixel, 720 by 486 pixels.
 *
 * Pixels run left-to-right, from the bottom of the screen to the top.
 *
 *	Mike @ BRL.MIL
 */

#define WIDTH 720
#define HEIGHT 486
#define BYTESPERPIXEL 3
#define ROWSIZE (WIDTH * BYTESPERPIXEL)

#define SIZE (WIDTH * HEIGHT * BYTESPERPIXEL)


char *map;
char *malloc();

main()
{

    int i;
    char *row;
    /*
     * magic number
     */

    printf("P6\n");

    /*
     * width height
     */
    printf("%d %d\n", WIDTH, HEIGHT);

    /*
     * maximum color component value
     */

    printf("255\n");
    fflush(stdout);

    /*
     * now gobble up the bytes
     */

    map = malloc( SIZE );
    if ( map == NULL ) {
	perror("malloc");
	exit(1);
    }


    if ( fread(map, 1, SIZE, stdin) == 0 ) {
	fprintf(stderr, "Short read\n");
	exit(1);
    }

    /*
     * now write them out in the right order, 'cause the
     * input is upside down.
     */
    
    for ( i = 0; i < HEIGHT; i++ ) {
	row = map + (HEIGHT - i) * ROWSIZE;
	fwrite(row, 1, ROWSIZE, stdout);
    }

    exit(0);
}

