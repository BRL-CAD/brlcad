/*
 *			P P - I K . C
 *
 *	plot color shaded pictures from GIFT on Ikonas.
 *
 *	Original Version:  Gary Kuehl,  April 1983
 *	Ported to VAX:  Mike Muuss, January 1984
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>;

#define ABS(i)	((i<0)?-(i):(i))

#ifdef vax
#define IKBUFSIZE (64*1024)	/* Size of Ikonas DMA buffer */
#else
#define IKBUFSIZE 16384		/* Size of Ikonas DMA buffer */
#endif
char ikbuf[IKBUFSIZE];
static char *Ikp = &ikbuf[0];

#define WPIXEL(x)	{register int *ip=x; \
	*Ikp++ = *ip++;/*R*/	*Ikp++ = *ip++;/*G*/	*Ikp++ = *ip;/*B*/ \
	Ikp++; \
	if( Ikp >= &ikbuf[IKBUFSIZE] )  { \
		write( ikfd, ikbuf, Ikp-ikbuf); \
		Ikp = &ikbuf[0]; \
	} \
}

char g();
long numb();

char linebuf[128];		/* For reading text lines into */

extern ikfd;			/* Ikonas file descriptor (iklib.o) */
extern ikhires;			/* Ikonas mode;  non-zero = HIRES */

int last_unpacked;				/* Global magic */
FILE *input;			/* Input file handle */

#define NCOLORS	((sizeof(ctab))/(sizeof(struct colors)))
struct colors {
	char *color;
	int  pixel[3];
} ctab[] = {
	"black",	0,0,0,
	"white",	255,255,255,
	"red",		255,0,0,
	"green",	0,255,0,
	"blue",		0,0,255,
	"cyan",		0,255,200,
	"magenta",	255,0,255,
	"yellow",	255,200,0,
	"orange",	255,100,0,
	"lime",		200,255,0,
	"olive",	220,190,0,
	"lt blue",	0,255,255,
	"violet",	200,0,255,
	"rose",		255,0,175,
	"gray",		120,120,120,
	"silver",	237,237,237,
	"brown",	200,130,0,
	"pink",		255,200,200,
	"flesh",	255,200,160,
	"rust",		200,100,0
};
struct items {
	long	low;
	long	high;
	int	color;
} itemtab[50] = {
	0,	99,	18,
	100,	199,	10,
	200,	299,	4,
	300,	399,	6,
	400,	499,	8,
	500,	599,	16,
	600,	699,	15,
	700,	799,	2,
	800,	899,	13,
	900,	999,	12,
	1000,	99999,	3,
};
int nitems = 11;		/* Number of items in table */

#define REFLECTANCE .003936

main(argc,argv)
int argc;
char **argv;
{
	register int i;
	static int horiz_pos,vert_pos;
	static int newsurface;
	static int inten_high;		/* high bits of intentisy */
	static float rm[3];
	static int icl[3];
	static int ibc;			/* background color index */
	static int *backgroundp;	/* pointer to background pixel */
	static int maxh,maxv,mnh,mnv;
	static int horiz_max,vert_max;
	static long li,lj;
	register char c;

	/* check invocation */
	if(argc!=2){
		printf("Usage: pp-ik filename\n");
		exit(10);
	}

	/* get plot file */
	if( (input = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		exit(10);
	}

	ikhires = 0;		/* for now */
	ikopen();
	load_map(1,0,0,0);	/* std map */

	/* print data on first two lines of plot file */
	fgets(linebuf, sizeof(linebuf), input);
line1:
	if( (linebuf[0] == ' ' || (linebuf[0]>='A' && linebuf[0]<='Z')) )
		printf("\007WARNING:  This appears to be a .PC file.  If this does not work, use pc-ik\n\n");
	for( i=0; linebuf[i]!='\n'; )
		putchar(linebuf[i++]);
	putchar('\n');

	for(i=0;i<20;i++){
		c=getc(input);
		putchar(c);
	}
	putchar('\n');

	fscanf( input, "%d", &maxh );
	fscanf( input, "%d", &maxv );
	printf("Number of Horz cells %4d, ",maxh);
	printf("Number of Vert cells %4d\n",maxv);

	printf("Code Color\n");
	for(i=0;i<NCOLORS;i++){
		printf("%3d - %-8s", i+1, ctab[i].color);
		if((i%5)==4) putchar('\n');
	}
	printf("Background color? ");
	scanf("%d",&ibc);
	if( ibc <= 0 )  ibc = 1;
	if( ibc > NCOLORS )  ibc = NCOLORS;

	printf("Code  Item range     Color\n");
	for(i=0;i<nitems;i++)
		printf("%4d %6ld %6ld  %s \n",
			i+1,
			itemtab[i].low,
			itemtab[i].high,
			ctab[itemtab[i].color].color );
	while(1)  {
		auto int incode, incolor;

		printf("Code (end<=0)? ");
		scanf("%d",&incode);
		if(incode<=0)
			break;
		if(incode>nitems)
			incode = ++nitems;
		printf("Lower limit? ");
		scanf("%ld",&itemtab[incode-1].low);
		printf("Upper limit? ");
		scanf("%ld",&itemtab[incode-1].high);
		printf("Color code? ");
		scanf("%d", &incolor);
		itemtab[incode-1].color = incolor-1;
	}

	/* compute screen coordinates of min and max */
	/* cause image to be centered on screen */
	mnh=(512-maxh)/2;
	mnv=(512-maxv)/2;
	horiz_max=mnh+maxh;
	vert_max=mnv+maxv;

	/* Random initialization */
	newsurface = 0;
	inten_high = 0;
	horiz_pos = 0;
	lseek( ikfd, 0L, 0);		/* To start of frame */

	/* paint background on upper part of screen */
	backgroundp= ctab[ibc-1].pixel;
	for(vert_pos=512; vert_pos > vert_max; vert_pos--)
		for(i=0;i<512;i++)
			WPIXEL(backgroundp);

	/* paint background on left side of screen */
	for(horiz_pos=0;horiz_pos<mnh;horiz_pos++)
		WPIXEL(backgroundp); 

	while((c=g())!='/')  {
		last_unpacked=c-32;
noread:		
		if(last_unpacked>31){
			/* compute intensity */
			static float ftemp;
			static float spi=0.;		/* Saved "pi" */
			register float pi;

			pi=REFLECTANCE * ( (last_unpacked&0x1F) + inten_high );
			icl[0]=pi*rm[0];
			icl[1]=pi*rm[1];
			icl[2]=pi*rm[2];
			ftemp=ABS(spi-pi);
			spi=pi;
			if(newsurface==0||ftemp>.1)  {
				WPIXEL(icl);
			}  else {
				/* fill scan between surfaces of same intensity */
				static int pixel[3];

				if(pi<.15) pi+=.15;
				if(pi>=.15) pi-=.15;
				pixel[0]=pi*rm[0];
				pixel[1]=pi*rm[1];
				pixel[2]=pi*rm[2];
				WPIXEL(pixel);
			}
			newsurface=0;
			horiz_pos++;
		}

		/* high order intensity */
		else if(last_unpacked>15) inten_high=(last_unpacked-16)<<5;

		/* control character */
		else switch(last_unpacked){

		case 0:
			/* miss target */
			lj=numb();
			for(li=0; li<lj; li++,horiz_pos++)  {
				WPIXEL(backgroundp);
			}
			newsurface=0;
			goto noread;

		case 1:
			/* new surface */
			newsurface=1;
			break;

		case 3:
			/* new item */
			lj=numb();
			/* Locate item in itemtab */
			for(i=0;i<(nitems-1);i++)
				if( lj >= itemtab[i].low && lj <= itemtab[i].high )
					break;
			rm[0]=ctab[itemtab[i].color].pixel[0];
			rm[1]=ctab[itemtab[i].color].pixel[1];
			rm[2]=ctab[itemtab[i].color].pixel[2];
			goto noread;

		case 10:
			/* repeat intensity */
			lj=numb();
			for(li=0;li<lj;li++,horiz_pos++) WPIXEL(icl);
			if(last_unpacked!=10) goto noread;
			break;

		case 14:
			/* end of line -- fill edges with background */
			while((horiz_pos++)<512)
				WPIXEL(backgroundp); 
			vert_pos--;
			for(horiz_pos=0;horiz_pos<mnh;horiz_pos++)
				WPIXEL(backgroundp);
		}
	}

	/* end of view */
	while((horiz_pos++)<512)
		WPIXEL(backgroundp);
	vert_pos--;
	while((vert_pos--)>0)
		for(i=0;i<512;i++)
			WPIXEL(backgroundp);

	/* Gobble up file until we see an alphabetic in col 1 */
	while(1)  {
		if( fgets(linebuf, sizeof(linebuf), input) == NULL )
			exit(0);		/* EOF */
		c = linebuf[0];
		if( (c>='a' && c <='z') || (c>='A' && c<='Z') )
			break;
	}
	printf("\n\n----------------------------------\n");
	lseek( ikfd, 0L, 0);		/* Back to start of frame */
	goto line1;
}

/*
 *	get number from packed word
 */
long numb()
{
	register long n;
	register int shift;

	n=0;
	shift=0;
	while( (last_unpacked=g()-32) > 31 )  {
		n += ((long)last_unpacked&31) << shift;
		shift += 5;
	}
	return(n);
}


/* get char from plot file - check for 75 columns and discard rest */
char g()
{
	static int nc=0;
	register char c;

	if( feof(input) )  {
		printf("pp-ik: unexpected EOF on data file\n");
		exit(1);
	}
	if((c=getc(input))!='\n'){
		nc++;
		if( nc > 75 )  {
			while((c=getc(input))!='\n');
			nc=1;
			return(getc(input));
		}
		return(c);
	}
	/* Char was newline */
	if(nc==75){
		nc=1;
		return(getc(input));
	}
	/* Pad with spaces to end of "card" */
	nc++;
	ungetc( c, input );
	return(' ');
}
