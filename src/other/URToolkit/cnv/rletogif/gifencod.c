
/*****************************************************************************
 *
 * GIFENCODE.C    - GIF Image compression interface
 * 
 * GIFEncode( FName, GHeight, GWidth, GInterlace, Background,
 *	      BitsPerPixel, Red, Green, Blue, GetPixel )
 *
 * Modified for use with RLE interface, Spencer W. Thomas, June, 1990.
 * (replaced fopen with rle_open_f).
 *
 *****************************************************************************/

#include <stdio.h>
#include "rle.h"
#include "rletogif.h"

#define TRUE 1
#define FALSE 0

static int Width, Height;
static int curx, cury;
static long CountDown;
static int Pass = 0;
static int Interlace;



/*
 * Bump the 'curx' and 'cury' to point to the next pixel
 */
static void
BumpPixel()
{
	/*
	 * Bump the current X position
	 */
	curx++;

	/*
	 * If we are at the end of a scan line, set curx back to the beginning
	 * If we are interlaced, bump the cury to the appropriate spot,
	 * otherwise, just increment it.
	 */
	if( curx == Width ) {
		curx = 0;

	        if( !Interlace ) 
			cury++;
		else {
		     switch( Pass ) {
	     
	               case 0:
        	          cury += 8;
                	  if( cury >= Height ) {
		  		Pass++;
				cury = 4;
		  	  }
                          break;
		  
	               case 1:
        	          cury += 8;
                	  if( cury >= Height ) {
		  		Pass++;
				cury = 2;
		  	  }
			  break;

	               case 2:
	                  cury += 4;
	                  if( cury >= Height ) {
	                     Pass++;
	                     cury = 1;
	                  }
	                  break;
			  
	               case 3:
	                  cury += 2;
	                  break;
			}
		}
	}
}

/*
 * Write out a word to the GIF file
 */
static void
Putword( w, fp )
int w;
FILE *fp;
{
	fputc( w & 0xff, fp );
    fputc( (w >> 8) & 0xff, fp );
}


/*
 * Return the next pixel from the image
 */
int
GIFNextPixel( getpixel )
ifunptr getpixel;
{
	int r;

	if( CountDown == 0 )
		return EOF;

	CountDown--;

	r = ( * getpixel )( curx, cury );

	BumpPixel();

	return r;
}

/* public */

void
GIFEncode( FName, GWidth, GHeight, GInterlace, Background, 
	   BitsPerPixel, Red, Green, Blue, GetPixel )
	 
char *FName;
int GWidth, GHeight;
int GInterlace;
int Background;
int BitsPerPixel;
short int Red[], Green[], Blue[];
ifunptr GetPixel;
{
	FILE *fp;
	int B;
	int RWidth, RHeight;
	int LeftOfs, TopOfs;
	int Resolution;
	int ColorMapSize;
	int InitCodeSize;
	int i;

    /* doesn't support interlace yet */
    if (GInterlace) error("no support for interlace yet");
	Interlace = GInterlace;
	
	ColorMapSize = 1 << BitsPerPixel;
	
	RWidth = Width = GWidth;
	RHeight = Height = GHeight;
	LeftOfs = TopOfs = 0;
	
	Resolution = BitsPerPixel;

	/*
	 * Calculate number of bits we are expecting
	 */
	CountDown = (long)Width * (long)Height;

	/*
	 * Indicate which pass we are on (if interlace)
	 */
	Pass = 0;

	/*
	 * The initial code size
	 */
	if( BitsPerPixel <= 1 )
		InitCodeSize = 2;
	else
		InitCodeSize = BitsPerPixel;

	/*
	 * Set up the current x and y position
	 */
	curx = cury = 0;

	/*
	 * Open the GIF file for binary write
	 */
	fp = rle_open_f( MY_NAME, FName, "w" );

	/*
	 * Write the Magic header
	 */
	fwrite( "GIF87a", 1, 6, fp );

	/*
	 * Write out the screen width and height
	 */
	Putword( RWidth, fp );
	Putword( RHeight, fp );

	/*
	 * Indicate that there is a global colour map
	 */
	B = 0x80;	/* Yes, there is a color map */
	/*
	 * OR in the resolution
	 */
	B |= (Resolution - 1) << 5;

	/*
	 * OR in the Bits per Pixel
	 */
	B |= (BitsPerPixel - 1);

	/*
	 * Write it out
	 */
	fputc( B, fp );

	/*
	 * Write out the Background colour
	 */
	fputc( Background, fp );

	/*
	 * Byte of 0's (future expansion)
	 */
	fputc( 0, fp );

	/*
	 * Write out the Global Colour Map
	*/
     	for( i=0; i<ColorMapSize; i++ ) {
            fputc( Red[i]>>8, fp );
            fputc( Green[i]>>8, fp );
            fputc( Blue[i]>>8, fp );
        }
	/*
	 * Write an Image separator
	 */
	fputc( ',', fp );

	/*
	 * Write the Image header
	 */

	Putword( LeftOfs, fp );
	Putword( TopOfs, fp );
	Putword( Width, fp );
	Putword( Height, fp );

	/*
	 * Write out whether or not the image is interlaced
	 */

	if( Interlace )
		fputc( 0x40, fp );
	else
		fputc( 0x00, fp );

	/*
	 * Write out the initial code size
	 */
    fputc( InitCodeSize, fp );

	/*
	 * Go and actually compress the data
	 */
    compgif( InitCodeSize + 1, fp, GetPixel );
	/*
	 * Write out a Zero-length packet (to end the series)
	 */
	fputc( 0, fp );

	/*
	 * Write the GIF file terminator
	 */
	fputc( ';', fp );

	/*
	 * And close the file
	 */
	fclose( fp );
	
}


