/*	P I X S H R I N K . C
 *
 *	scale down a picture by a uniform factor.
 *
 *	Options
 *	h	help
 */
#include <stdio.h>
#ifdef BSD
#include <strings.h>
#endif
#ifdef SYSV
#include <string.h>
#endif

/* declarations to support use of getopt() system call */
char *options = "hs:w:n:f:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
char *filename = "(stdin)";

int width = 512;
int height = 512;
int scanlen;
int factor = 2;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage()
{
	(void) fprintf(stderr,
"Usage: %s [-h] [-w width] [-n scanlines] [-s squaresize]\n\
[-f shrink_factor] [pixfile] > pixfile\n", progname);
	exit(1);
}


/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
void parse_args(ac, av)
int ac;
char *av[];
{
	char *strrchr();
	int  c;

	
	if (!(progname = strrchr(*av, '/')))
		progname = *av;
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'f'	: if ((c = atoi(optarg)) > 1)
					factor = c;
				  break;
		case 'h'	: width = height = 1024; break;
		case 'n'	: if ((c=atoi(optarg)) > 0)
					height = c;
				  break;
		case 'w'	: if ((c=atoi(optarg)) > 0)
					width = c;
				  break;
		case 's'	: if ((c=atoi(optarg)) > 0)
					height = width = c;
				  break;
		case '?'	: 
		default		: usage(); break;
		}

	if (optind >= ac) {
		if (isatty(fileno(stdout)) )
			usage();
	}
	if (optind < ac) {
		if (freopen(av[optind], "r", stdin) == (FILE *)NULL) {
			perror(av[optind]);
			exit(-1);
		} else
			filename = av[optind];
	}
	if (optind+1 < ac)
		(void)fprintf(stderr, "%s: Excess arguments ignored\n", progname);

}



/*	R E A D _ I M A G E
 *
 *	read image into memory
 */
unsigned char *read_image(Width, Height, buffer)
int Width, Height;
unsigned char *buffer;
{
	int total_bytes, in_bytes, count;
	unsigned char *malloc();

	scanlen = Width * 3;
	if (!buffer && (buffer=malloc(scanlen * Height)) == (unsigned char *)NULL) {
		(void)fprintf(stderr, "%s: cannot allocate input buffer\n", 
			progname);
		exit(-1);
	}

	total_bytes = Width * Height * 3;
	in_bytes = 0;
	while (in_bytes < total_bytes && 
	    (count=read(0, (char *)&buffer[in_bytes], (unsigned)(total_bytes-in_bytes))) >= 0)
		in_bytes += count;

	if (count < 0) {
		perror(filename);
		exit(-1);
	}

	return(buffer);
}

/*	W R I T E _ I M A G E
 *
 *
 */
void write_image(Width, Height, buffer)
int Width, Height;
unsigned char *buffer;
{
	int count, out_bytes, total_bytes;
	
	total_bytes = Width * Height * 3;
	out_bytes = 0;

	while (out_bytes < total_bytes &&
	    (count=write(1, (char *)&buffer[out_bytes], total_bytes-out_bytes)) >= 0)
		out_bytes += count;

	if (count < 0) {
		perror("stdout");
		exit(-1);
	}
}

/*	S H R I N K _ I M A G E
 *
 *	
 */
shrink_image(Width, Height, buffer, Factor)
unsigned char *buffer;
int Factor, Width, Height;
{
	unsigned char *pixelp, *finalpixel;
	unsigned int p[3];
	int facsq, x, y, px, py;

	facsq = Factor * Factor;
	finalpixel = buffer;

	for (y=0 ; y < Height ; y += factor)
		for (x=0 ; x < Width ; x += factor) {

			/* average factor by factor pixels */

			for (py = 0 ; py < 3 ; ++py)
				p[py] = 0;

			for (py = 0 ; py < factor ; py++) {

				/* get first pixel in scanline */
				pixelp = &buffer[y*scanlen+x*3];

				/* add pixels from scanline to average */
				for (px = 0 ; px < factor ; px++) {
					p[0] += *pixelp++;
					p[1] += *pixelp++;
					p[2] += *pixelp++;
				}
			}
			
			/* store resultant pixel back in buffer */
			for (py = 0 ; py < 3 ; ++py)
				*finalpixel++ = p[py] / facsq;
		}
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	unsigned char *buffer = (char *)NULL;

	(void)parse_args(ac, av);
	if (isatty(fileno(stdin))) usage();

	/* process stdin */
	buffer = read_image(width, height, buffer);

	shrink_image(width, height, buffer, factor);

	write_image(width/factor, height/factor, buffer);
	return(0);
}


