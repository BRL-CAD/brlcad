/*			P I X C O L O R S
 *
 *	Count the number of different pixel values in a PIX format image.
 *	If the "-v" option is selected, list each unique pixel value 
 *	to the standard output.
 *
 *	Author(s)
 *	Lee A. Butler	butler@stsci.edu
 *
 *	Options
 *	v	list colors
 */
#include <stdio.h>

/* declarations to support use of getopt() system call */
char *options = "v";
char verbose = 0;
extern char *optarg;
extern int optind, opterr, getopt();
char *progname = "(noname)";

#define PIXELS 1024
unsigned char pixbuf[BUFSIZ*3];

/* This grotesque array provides 1 bit for every possible pixel value.
 * the "NOBASE" comment below allows compilation on the Gould 9000 series
 * of computers.
 */
/*NOBASE*/
unsigned char vals[2097152L];

#define TEST(p)    (vals[(p)/8] & 1 << (p)%8)

/*
 *    D O I T --- Main function of program
 */
void doit()
{
    unsigned long bytes, i, pixel, count;
    
    count = 0;
    while ((bytes=fread(pixbuf, 3, PIXELS, stdin)) > 0) {
	for (i = 0 ; i < bytes ; ++i) {
	    pixel = pixbuf[i*3] + (pixbuf[i*3+1] << 8) + (pixbuf[i*3+2] << 16);
	    if ( ! TEST(pixel) ) {
		vals[pixel/8] = vals[pixel/8] | 1 << pixel%8;
		++count;
	    }
	}
    }
    (void) printf("%u\n", count);
    if (verbose)
	for (i=0 ; i < sizeof(vals) ; ++i)
	    if (TEST(i))
		(void) printf("%3d %3d %3d\n", i & 0x0ff,
		    (i >> 8) & 0x0ff,
		    (i >> 16) & 0x0ff);
}

void usage()
{
    (void) fprintf(stderr, "Usage: %s [ -v ] < PIXfile\n", progname, options);
    exit(1);
}

/*
 *    M A I N
 *
 *    Perform miscelaneous tasks such as argument parsing and
 *    I/O setup and then call "doit" to perform the task at hand
 */
main(ac,av)
int ac;
char *av[];
{
    int  c;

    progname = *av;
    if (isatty(fileno(stdin))) usage();
    
    /* Get # of options & turn all the option flags off
     */

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line
     */
    while ((c=getopt(ac,av,options)) != EOF) {
	if ( c == 'v' ) verbose = ! verbose;
	else usage();
    }

    if (optind < ac) usage();

    doit();
}
