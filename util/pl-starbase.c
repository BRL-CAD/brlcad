/*
 *			P L - S T A R B A S E . C
 *
 * Description -
 *	Convert a unix-plot file to STARBASE output
 *
 * Author -  
 *	Mark Huston Bowden  
 *  
 *  Source -
 *	Research Institute, E-47 
 *	University of Alabama in Huntsville  
 *	Huntsville, AL  35899
 *	(205) 895-6467 UAH
 *	(205) 876-1089 Redstone Arsenal
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <starbase.c.h>

#define	geti(x)	{ (x) = getchar(); (x) |= (short)(getchar()<<8); }
#define getb(x)	((x) = getchar())
#define ASPECT  1.56163

int fildes,indx;

int
main(argc,argv)
int argc;
char **argv;
{
	char s[80];
	int c,i,x,y,x1,x2,y1,y2,r,g,b,or,og,ob;

	switch (argc) {
	case 1:
		fildes = gopen("/dev/tty",OUTDEV,"hp262x",INIT);
		break;
	case 3:
		fildes = gopen(argv[1],OUTDEV,argv[2],INIT);
		break;
	default:
		printf("Usage: %s [device driver]\n",argv[0]);
		exit(1);
	}
	if (fildes == -1) {
		printf("%s: unable to open STARBASE.\n",argv[0]);
		exit(2);
	}

	getb(c);
	do {
		switch (c) {
		case 'p':		/* point */
			geti(x);
			geti(y);
			move2d(fildes,(float)x,(float)y);
			draw2d(fildes,(float)x,(float)y);
			break;
		case 'l':		/* line */
			geti(x1);
			geti(y1);
			geti(x2);
			geti(y2);
			move2d(fildes,(float)x1,(float)y1);
			draw2d(fildes,(float)x2,(float)y2);
			break;
		case 'f':		/* line style */
			for (i = 0; (s[i] = getchar()) != '\n'; i++);
			s[i] = '\0';
			/* set line style */
			break;
		case 'm':		/* move */
			geti(x);
			geti(y);
			move2d(fildes,(float)x,(float)y);
			break;
		case 'n':		/* draw */
			geti(x);
			geti(y);
			draw2d(fildes,(float)x,(float)y);
			break;
		case 't':		/* text */
			for (i = 0; (s[i] = getchar()) != '\n'; i++);
			s[i] = '\0';
			/* draw text */
			break;
		case 's':		/* space */
			geti(x1);
			geti(y1);
			geti(x2);
			geti(y2);
			vdc_extent(fildes,(float)x1*ASPECT,(float)y1,0.0,
			    (float)x2*ASPECT,(float)y2,0.0);
			clip_rectangle(fildes,(float)x1*ASPECT,(float)x2*ASPECT,
			    (float)y1,(float)y2);
			mapping_mode(fildes,DISTORT);
			break;
		case 'C':		/* color */
			r = getchar();
			g = getchar();
			b = getchar();
			if (or != r || og != g || ob != b)
				line_color_index(fildes,++i%7 + 1);
			or = r;
			og = g;
			ob = b;
			break;
		default:
			break;
		}
		getb(c);
	} while (!feof(stdin));

	gclose(fildes);
	exit(0);
}
