/*
 *			P C D - P I X . C
 *
 *  Authors -
 *	Hadmut Danisch (original program pcdtoppm)
 *	Michael John Muuss (this adaptation)
 *
 *  Distribution Status -
 *	Original work Copyright (c) 1992 by Hadmut Danisch.
 *	The modifications by Mike Muuss are Public Domain, Distribution Unlimited.
 *
 *  Based upon -
 *	hpcdtoppm (Hadmut's pcdtoppm) v0.3
 *
 *  Copyright (c) 1992 by Hadmut Danisch (danisch@ira.uka.de).
 *  Permission to use and distribute this software and its
 *  documentation for noncommercial use and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation. It is not allowed to sell this software in
 *  any way. This software is not public domain.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/* define DEBUG for some debugging informations, just remove the x from xDEBUG */
#define xDEBUG

/* define MELDUNG if you want to see what is happening and what takes time,
   just remove the x from xMeldung */
#define xMELDUNG

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>

#include "machine.h"


#define own_BUsize (3*24*1024)

typedef unsigned char uBYTE;
typedef unsigned long dim;

#define BaseW ((dim)768)
#define BaseH ((dim)512)

#define SECSIZE 0x800



#define SeHead   2
#define L_Head   (1+SeHead)

#define SeBase16 18
#define L_Base16 (1+SeBase16)

#define SeBase4  72
#define L_Base4  (1+SeBase4)

#define SeBase   288
#define L_Base   (1+SeBase)


enum ERRORS {
	E_NONE,E_READ,E_WRITE,E_INTERN,E_ARG,E_OPT,E_MEM,E_HUFF,
	    E_SEQ,E_SEQ1,E_SEQ2,E_SEQ3,E_SEQ4,E_SEQ5,E_SEQ6,E_SEQ7,E_POS,E_IMP,E_OVSKIP,
	    E_TAUTO,E_TCANT };

enum SIZES  {
	S_UNSPEC,S_Base16,S_Base4,S_Base,S_4Base,S_16Base,S_Over };

/* Default taken when no size parameter given */
#define S_DEFAULT S_Base

struct _implane
{
	dim  mwidth,mheight,
	iwidth,iheight;
	uBYTE *im;
};
typedef struct _implane implane;

#define nullplane ((implane *) 0)


static enum ERRORS readplain(dim w, dim h, implane *l, implane *c1, implane *c2);
static void interpolate(implane *p);
static void halve(implane *p);
static void ycctorgb(dim w, dim h, implane *l, implane *c1, implane *c2);
#if 0
static void readlpt();
#endif
static void readhqt(dim w, dim h, int n);
static void decode(dim w, dim h, implane *f, implane *f1, implane *f2, int autosync);
static void druckeid(void);
static long Skip4Base(void);

static FILE *fin=0,*fout=0;
static char *pcdname=0;
static char nbuf[100];
static uBYTE sbuffer[SECSIZE];
static int do_bw;

/* Using preprocessor for inline-procs */
#ifdef DEBUG

static long bufpos;

#define SEEK(x) { if (fseek(fin,((x) * SECSIZE),0)) error(E_READ);\
                  fprintf(stderr,"S-Position %x\n",ftell(fin)); }
#define RPRINT  {fprintf(stderr,"R-Position %x\n",ftell(fin));}

#define READBUF   (bufpos=ftell(fin),fread(sbuffer,sizeof(sbuffer),1,fin))


#else

#define SEEK(x) { if (fseek(fin,((x) * SECSIZE),0)) error(E_READ);}
#define RPRINT
#define READBUF   fread(sbuffer,sizeof(sbuffer),1,fin)

#endif


#ifdef MELDUNG
#define melde(x) fprintf(stderr,x)
#else
#define melde(x)
#endif

#define EREADBUF {if(READBUF < (unsigned)1) error(E_READ);}

#define SKIP(n)  { if (fseek(fin,(n),1)) error(E_READ);}
#define SKIPr(n) { if (fseek(fin,(n),1)) return(E_READ);}

static int clipped_low=0;
static int clipped_high=0;
#define NORM(x) { if(x<0) {x=0; clipped_low++;} \
		else if (x>255) {x=255; clipped_high++;}}

static double lum_mul = 1.0;

static void error(enum ERRORS e)
{

	switch(e)
	{
	case E_NONE:
		return;
	case E_IMP:
		fprintf(stderr,"Sorry, Not yet implemented.\n");
		break;
	case E_READ:
		fprintf(stderr,"Error while reading.\n");
		break;
	case E_WRITE:
		fprintf(stderr,"Error while writing.\n");
		break;
	case E_INTERN:
		fprintf(stderr,"Internal error.\n");
		break;
	case E_ARG:
		fprintf(stderr,"Usage: pcd-pix [options] file.pcd\n");
		fprintf(stderr,"Options:	(file name may be just frame number)\n");
		fprintf(stderr,"     -x Overskip mode (tries to improve color quality.)\n");
		fprintf(stderr,"     -i Give some information from fileheader\n");
		fprintf(stderr,"     -b write .bw file instead of .pix\n");
		fprintf(stderr,"     -0 Extract thumbnails from Overview file\n");
		fprintf(stderr,"     -1 Extract  -w192  -n128 from Image file [Base/16]\n");
		fprintf(stderr,"     -2 Extract  -w384  -n256 from Image file [Base/4]\n");
		fprintf(stderr,"     -3 Extract  -w768  -n512 from Image file [Base].  Default size.\n");
		fprintf(stderr,"     -4 Extract -w1536 -n1024 from Image file [4Base]\n");
		fprintf(stderr,"     -5 Extract -w3072 -n2048 from Image file [16Base]\n");
		fprintf(stderr,"     -p Pipe output to pix-fb automaticly\n");
		fprintf(stderr,"     -l # Multiply luminance value by # (brighten dark photos) default=1\n");
		fprintf(stderr,"     -h Just print header dimensions\n");
		break;
	case E_OPT:
		fprintf(stderr,"These Options are not allowed together.\n");
		break;
	case E_MEM:
		fprintf(stderr,"Not enough memory !\n");
		break;
	case E_HUFF:
		fprintf(stderr,"Error in Huffman-Code-Table\n");
		break;
	case E_SEQ:
		fprintf(stderr,"Error in Huffman-Sequence\n");
		break;
	case E_SEQ1:
		fprintf(stderr,"Error1 in Huffman-Sequence\n");
		break;
	case E_SEQ2:
		fprintf(stderr,"Error2 in Huffman-Sequence\n");
		break;
	case E_SEQ3:
		fprintf(stderr,"Error3 in Huffman-Sequence\n");
		break;
	case E_SEQ4:
		fprintf(stderr,"Error4 in Huffman-Sequence\n");
		break;
	case E_SEQ5:
		fprintf(stderr,"Error5 in Huffman-Sequence\n");
		break;
	case E_SEQ6:
		fprintf(stderr,"Error6 in Huffman-Sequence\n");
		break;
	case E_SEQ7:
		fprintf(stderr,"Error7 in Huffman-Sequence\n");
		break;
	case E_POS:
		fprintf(stderr,"Error in file-position\n");
		break;
	case E_OVSKIP:
		fprintf(stderr,"Can't read this resolution in overskip-mode\n");
		break;
	case E_TAUTO:
		fprintf(stderr,"Can't determine the orientation in overview mode\n");
		break;
	case E_TCANT:
		fprintf(stderr,"Sorry, can't determine orientation for this file.\n");
		fprintf(stderr,"Please give orientation parameters. \n");
		break;
	default:
		fprintf(stderr,"Unknown error %d ???\n",e);
		break;
	}
	if(fin) fclose(fin);
	if(fout) fclose(fout);
	exit(9);
}

/*
 *			P L A N E A L L O C
 *
 *  Allocate memory for one YCC color plane.
 */
static void planealloc(implane *p, dim width, dim height)
{
	p->iwidth=p->iheight=0;
	p->mwidth=width;
	p->mheight=height;

	p->im = ( uBYTE * ) malloc  (width*height);
	if(!(p->im)) error(E_MEM);
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)


#define ASKIP { argc--; argv ++;}
{
	int bildnr;
	char *opt;
	dim w=0,h=0;
	long cd_offset,cd_offhelp;
	int do_info,do_overskip;
	int	do_pixfb = 0;
	int	do_headeronly = 0;

	enum SIZES size=S_UNSPEC;
	enum ERRORS eret;
	implane Luma, Chroma1,Chroma2;

	do_info=do_overskip=do_bw=0;

	ASKIP;

	/* Argument processing */
	while((argc>0) && **argv=='-')
	{
		opt= (*argv)+1;
		ASKIP;

		if( strcmp(opt, "l") == 0 )  {
			opt= (*argv);
			ASKIP;
			lum_mul = atof(opt);
			continue;
		}

		if( strcmp(opt, "p") == 0 )  {
			if (!do_pixfb) do_pixfb=1;
			else error(E_ARG);
			continue;
		}

		if( strcmp(opt, "h") == 0 )  {
			if (!do_headeronly) do_headeronly=1;
			else error(E_ARG);
			continue;
		}

		if(!strcmp(opt,"i"))
		{
			if (!do_info) do_info=1;
			else error(E_ARG);
			continue;
		}

		if(!strcmp(opt,"x"))
		{
			if (!do_overskip) do_overskip=1;
			else error(E_ARG);
			continue;
		}


		if(!strcmp(opt,"b"))
		{
			if (!do_bw) do_bw=1;
			else error(E_ARG);
			continue;
		}




		if((!strcmp(opt,"Base/16")) || (!strcmp(opt,"1"))  || (!strcmp(opt,"128x192")))
		{
			if (size == S_UNSPEC) size = S_Base16;
			else error(E_ARG);
			continue;
		}
		if((!strcmp(opt,"Base/4" )) || (!strcmp(opt,"2"))  || (!strcmp(opt,"256x384")))
		{
			if (size == S_UNSPEC) size = S_Base4;
			else error(E_ARG);
			continue;
		}
		if((!strcmp(opt,"Base"   )) || (!strcmp(opt,"3"))  || (!strcmp(opt,"512x768")))
		{
			if (size == S_UNSPEC) size = S_Base;
			else error(E_ARG);
			continue;
		}
		if((!strcmp(opt,"4Base"  )) || (!strcmp(opt,"4"))  || (!strcmp(opt,"1024x1536")))
		{
			if (size == S_UNSPEC) size = S_4Base;
			else error(E_ARG);
			continue;
		}
		if((!strcmp(opt,"16Base" )) || (!strcmp(opt,"5"))  || (!strcmp(opt,"2048x3072")))
		{
			if (size == S_UNSPEC) size = S_16Base;
			else error(E_ARG);
			continue;
		}

		if((!strcmp(opt,"Overview" )) || (!strcmp(opt,"0"))  || (!strcmp(opt,"O")))
		{
			if (size == S_UNSPEC) size = S_Over;
			else error(E_ARG);
			continue;
		}

		fprintf(stderr,"Unknown option: -%s\n",opt);
		error(E_ARG);
	}




	if(size==S_UNSPEC) size=S_DEFAULT;

	if(argc<1) error(E_ARG);
	pcdname= *argv;
	ASKIP;

	if(argc>0) error(E_ARG);
	if(do_info && (size==S_Over)) error(E_OPT);
	if(do_overskip && (size != S_Base16) && (size != S_Base4) && (size != S_Base) && (size != S_4Base) ) error(E_OVSKIP);

	if(!(fin=fopen(pcdname,"r")))  {
		/* If name is frame number only, create name and go again */
		if( atoi(pcdname) > 0 )  {
			sprintf(nbuf, "img%4.4d.pcd", atoi(pcdname) );
			if(!(fin=fopen(nbuf,"r")))  {
				perror(nbuf);
				error(E_READ);
			}
		} else {
			perror(pcdname);
			error(E_READ);
		}
	}

	if(do_info)
	{
		SEEK(1);
		EREADBUF;
	}

	switch(size)  {
	case S_Base16:
		w=BaseW/4;
		h=BaseH/4;
		break;
	case S_Base4:
		w=BaseW/2;
		h=BaseH/2;
		break;
	case S_Base:
		w=BaseW;
		h=BaseH;
		break;
	case S_4Base:
		w=BaseW*2;
		h=BaseH*2;
		break;
	case S_16Base:
		w=BaseW*4;
		h=BaseH*4;
		break;
	case S_Over:
		w=BaseW/4;
		h=BaseH/4;
		break;
	default:
		error(E_INTERN);
	}

	if( do_headeronly )  {
		printf("-w%ld -n%ld\n", w, h);
		exit(0);
	}

	if( do_pixfb )  {
		if( do_bw )
			sprintf(nbuf, "bw-fb -w%ld -n%ld", w, h);
		else
			sprintf(nbuf, "pix-fb -w%ld -n%ld", w, h);

		if( (fout = popen( nbuf, "w" )) == NULL )  {
			perror(nbuf);
			exit(42);
		}
	} else {
		fout = stdout;
	}

	if(do_info) druckeid();

	switch(size)
	{
	case S_Base16:
		w=BaseW/4;
		h=BaseH/4;

		if(!do_overskip)
		{
			planealloc(&Luma   ,w,h);
			planealloc(&Chroma1,w,h);
			planealloc(&Chroma2,w,h);
			SEEK(L_Head+1);
			error(readplain(w,h,&Luma,&Chroma1,&Chroma2));
			interpolate(&Chroma1);
			interpolate(&Chroma2);
		}
		else
		{
			planealloc(&Luma   ,w,h);
			planealloc(&Chroma1,2*w,2*h);
			planealloc(&Chroma2,2*w,2*h);
			SEEK(L_Head+1);
			error(readplain(w,h,&Luma,nullplane,nullplane));
			SEEK(L_Head+L_Base16+1);
			error(readplain(2*w,2*h,nullplane,&Chroma1,&Chroma2));
		}

		ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		break;

	case S_Base4:
		w=BaseW/2;
		h=BaseH/2;

		if(!do_overskip)
		{
			planealloc(&Luma   ,w,h);
			planealloc(&Chroma1,w,h);
			planealloc(&Chroma2,w,h);
			SEEK(L_Head+L_Base16+1);
			error(readplain(w,h,&Luma,&Chroma1,&Chroma2));
			interpolate(&Chroma1);
			interpolate(&Chroma2);
		}
		else
		{
			planealloc(&Luma   ,w,h);
			planealloc(&Chroma1,2*w,2*h);
			planealloc(&Chroma2,2*w,2*h);
			SEEK(L_Head+L_Base16+1);
			error(readplain(w,h,&Luma,nullplane,nullplane));
			SEEK(L_Head+L_Base16+L_Base4+1);
			error(readplain(2*w,2*h,nullplane,&Chroma1,&Chroma2));
		}

		ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		break;

	case S_Base:
		w=BaseW;
		h=BaseH;

		if(!do_overskip)
		{
			planealloc(&Luma   ,w,h);
			planealloc(&Chroma1,w,h);
			planealloc(&Chroma2,w,h);
			SEEK(L_Head+L_Base16+L_Base4+1);
			error(readplain(w,h,&Luma,&Chroma1,&Chroma2));
			interpolate(&Chroma1);
			interpolate(&Chroma2);
		}
		else
		{
			planealloc(&Luma   ,  w,  h);
			planealloc(&Chroma1,2*w,2*h);
			planealloc(&Chroma2,2*w,2*h);
			SEEK(L_Head+L_Base16+L_Base4+1);
			error(readplain(w,h,&Luma,&Chroma1,&Chroma2));
			interpolate(&Chroma1);
			interpolate(&Chroma2);
			interpolate(&Chroma1);
			interpolate(&Chroma2);

			cd_offset=Skip4Base();
			SEEK(cd_offset+10);
			EREADBUF;
			cd_offhelp=(((long)sbuffer[2])<<8)|sbuffer[3];
			SEEK(cd_offset+12);
			readhqt(w,h,3);
			SEEK(cd_offset+cd_offhelp);
			decode(4*w,4*h,nullplane,&Chroma1,&Chroma2,1);

			halve(&Chroma1);
			halve(&Chroma2);
		}
		ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		break;

	case S_4Base:
		w=BaseW*2;
		h=BaseH*2;

		if(!do_overskip)
		{
			planealloc(&Luma,w,h);
			planealloc(&Chroma1,w,h);
			planealloc(&Chroma2,w,h);
			SEEK(L_Head+L_Base16+L_Base4+1);
			error(readplain(w/2,h/2,&Luma,&Chroma1,&Chroma2));
			interpolate(&Luma);
			interpolate(&Chroma1);
			interpolate(&Chroma1);
			interpolate(&Chroma2);
			interpolate(&Chroma2);

			cd_offset = L_Head + L_Base16 + L_Base4 + L_Base ;
			SEEK(cd_offset + 4);
			readhqt(w,h,1);
			SEEK(cd_offset + 5);
			decode(w,h,&Luma,nullplane,nullplane,0);
		}
		else
		{
			planealloc(&Luma,w,h);
			planealloc(&Chroma1,2*w,2*h);
			planealloc(&Chroma2,2*w,2*h);
			SEEK(L_Head+L_Base16+L_Base4+1);
			error(readplain(w/2,h/2,&Luma,&Chroma1,&Chroma2));
			interpolate(&Luma);
			interpolate(&Chroma1);
			interpolate(&Chroma1);
			interpolate(&Chroma2);
			interpolate(&Chroma2);

			cd_offset = L_Head + L_Base16 + L_Base4 + L_Base ;
			SEEK(cd_offset + 4);
			readhqt(w,h,1);
			SEEK(cd_offset + 5);
			decode(w,h,&Luma,nullplane,nullplane,0);

			cd_offset=ftell(fin);
			if(cd_offset % SECSIZE) error(E_POS);
			cd_offset/=SECSIZE;
			SEEK(cd_offset+10);
			EREADBUF;
			cd_offhelp=(((long)sbuffer[2])<<8)|sbuffer[3];
			SEEK(cd_offset+12);
			readhqt(w,h,3);
			SEEK(cd_offset+cd_offhelp);
			decode(2*w,2*h,nullplane,&Chroma1,&Chroma2,1);

		}
		ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		break;

	case S_16Base:
		w=BaseW*4;
		h=BaseH*4;
		planealloc(&Luma,w,h);
		planealloc(&Chroma1,w,h);
		planealloc(&Chroma2,w,h);

		SEEK(L_Head+L_Base16+L_Base4+1);
		error(readplain(w/4,h/4,&Luma,&Chroma1,&Chroma2));
		interpolate(&Luma);
		interpolate(&Chroma1);
		interpolate(&Chroma1);
		interpolate(&Chroma2);
		interpolate(&Chroma2);

		cd_offset = L_Head + L_Base16 + L_Base4 + L_Base ;
		SEEK(cd_offset + 4);
		readhqt(w/2,h/2,1);
		SEEK(cd_offset + 5);
		decode(w/2,h/2,&Luma,nullplane,nullplane,0);
		interpolate(&Luma);

		cd_offset=ftell(fin);
		if(cd_offset % SECSIZE) error(E_POS);
		cd_offset/=SECSIZE;

		SEEK(cd_offset+12);
		readhqt(w,h,3);
		SEEK(cd_offset+14);
		decode(w,h,&Luma,&Chroma1,&Chroma2,0);

		interpolate(&Chroma1);
		interpolate(&Chroma2);

		ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		break;

	case S_Over:
		w=BaseW/4;
		h=BaseH/4;

		planealloc(&Luma   ,w,h);
		planealloc(&Chroma1,w,h);
		planealloc(&Chroma2,w,h);

		for(bildnr=0;!feof(fin);bildnr++)
		{
			SEEK(5+SeBase16*bildnr);

			eret=readplain(w,h,&Luma,&Chroma1,&Chroma2);
			if(eret==E_READ) break;
			error(eret);

			interpolate(&Chroma1);
			interpolate(&Chroma2);

			/* Write this image to 'fout' */
			ycctorgb(w,h,&Luma,&Chroma1,&Chroma2);
		}
		break;

	default:
		error(E_INTERN);
	}

	if( do_pixfb )  pclose(fout);

	if( clipped_low || clipped_high )  {
		fprintf(stderr, "pcd-pix: %d clipped low, %d clipped high\n",
			clipped_low, clipped_high );
	}

	return 0;
}
#undef ASKIP

/*
 *			R E A D P L A I N
 */
static enum ERRORS readplain(dim w, dim h, implane *l, implane *c1, implane *c2)
{
	dim i;
	uBYTE *pl=0,*pc1=0,*pc2=0;
	melde("readplain\n");

	if(l)
	{
		if ((l->mwidth<w) || (l->mheight<h) || (!l->im)) error(E_INTERN);
		l->iwidth=w;
		l->iheight=h;
		pl=l->im;
	}

	if(c1)
	{
		if ((c1->mwidth<w/2) || (c1->mheight<h/2) || (!c1->im)) error(E_INTERN);
		c1->iwidth=w/2;
		c1->iheight=h/2;
		pc1=c1->im;
	}

	if(c2)
	{
		if ((c2->mwidth<w/2) || (c2->mheight<h/2) || (!c2->im)) error(E_INTERN);
		c2->iwidth=w/2;
		c2->iheight=h/2;
		pc2=c2->im;
	}

	for(i=0;i<h/2;i++)
	{
		if(pl)
		{
			if(fread(pl,w,1,fin)<(unsigned)1) return(E_READ);
			pl+= l->mwidth;

			if(fread(pl,w,1,fin)<(unsigned)1) return(E_READ);
			pl+= l->mwidth;
		}
		else SKIPr(2*w);

		if(pc1)
		{
			if(fread(pc1,w/2,1,fin)<(unsigned)1) return(E_READ);
			pc1+= c1->mwidth;
		}
		else SKIPr(w/2);

		if(pc2)
		{
			if(fread(pc2,w/2,1,fin)<(unsigned)1) return(E_READ);
			pc2+= c2->mwidth;
		}
		else SKIPr(w/2);


	}
	RPRINT;
	return E_NONE;
}

/*
 *			I N T E R P O L A T E
 *
 *  Take in image where iwidth is half of mwidth,
 *  output image where iwidth has been doubled.  (Height too).
 */
static void interpolate(implane *p)
{
	dim w,h,x,y,yi;
	uBYTE *optr,*nptr,*uptr;

	melde("interpolate\n");
	if ((!p) || (!p->im)) error(E_INTERN);

	w=p->iwidth;
	h=p->iheight;

	if(p->mwidth  < 2*w ) error(E_INTERN);
	if(p->mheight < 2*h ) error(E_INTERN);

	p->iwidth=2*w;
	p->iheight=2*h;

	for(y=0;y<h;y++)
	{
		yi=h-1-y;
		optr=p->im+  yi*p->mwidth + (w-1);
		nptr=p->im+2*yi*p->mwidth + (2*w - 2);

		nptr[0]=nptr[1]=optr[0];

		for(x=1;x<w;x++)
		{
			optr--;
			nptr-=2;
			nptr[0]=optr[0];
			nptr[1]=(((int)optr[0])+((int)optr[1])+1)>>1;
		}
	}

	for(y=0;y<h-1;y++)
	{
		optr=p->im + 2*y*p->mwidth;
		nptr=optr+p->mwidth;
		uptr=nptr+p->mwidth;

		for(x=0;x<w-1;x++)
		{
			nptr[0]=(((int)optr[0])+((int)uptr[0])+1)>>1;
			nptr[1]=(((int)optr[0])+((int)optr[2])+((int)uptr[0])+((int)uptr[2])+2)>>2;
			nptr+=2;
			optr+=2;
			uptr+=2;
		}
		*(nptr++)=(((int)*(optr++))+((int)*(uptr++))+1)>>1;
		*(nptr++)=(((int)*(optr++))+((int)*(uptr++))+1)>>1;
	}

	/* Duplicate the last line */
	optr=p->im + (2*h-2)*p->mwidth;
	nptr=p->im + (2*h-1)*p->mwidth;
	for(x=0;x<w;x++)
	{
		*(nptr++) = *(optr++);
		*(nptr++) = *(optr++);
	}
}

/*
 *			H A L V E
 *
 *  Image is shrunk by half, by discarding data values.
 */
static void halve(implane *p)
{
	dim w,h,x,y;
	uBYTE *optr,*nptr;

	melde("halve\n");
	if ((!p) || (!p->im)) error(E_INTERN);

	w=p->iwidth/=2;
	h=p->iheight/=2;


	for(y=0;y<h;y++)
	{
		nptr=(p->im) +   y*(p->mwidth);
		optr=(p->im) + 2*y*(p->mwidth);

		for(x=0;x<w;x++,nptr++,optr+=2)
		{
			*nptr = *optr;
		}
	}
}

#define BUinit {BUcount=0;BUptr=BUF;}
#define BUflush {fwrite(BUF,BUptr-BUF,1,fout);BUinit; }
#define BUwrite(r,g,b) {if(BUcount>=own_BUsize/3) BUflush; \
	*BUptr++ = r ; *BUptr++ = g ; *BUptr++ = b ; BUcount++;}
#define BUwrite1(r) {if(BUcount>=own_BUsize) BUflush; \
	*BUptr++ = r ; BUcount++;}

#define BitShift 12

/*
 *			Y C C T O R G B
 *
 *  Convert and output.
 *  If "do_bw" flag is set, just output luminance channel.
 */
static void ycctorgb(dim w, dim h, implane *l, implane *c1, implane *c2)
{
	int x,y;		/* not dim! */
	uBYTE *pl,*pc1,*pc2;
	long red,green,blue,i;
	long L;
	static int init=0;
	static long XL[256],XC1[256],XC2[256],XC1g[256],XC2g[256];
	static uBYTE BUF[own_BUsize];
	register uBYTE	*BUptr;
	int   BUcount;

	melde("ycctorgb\n");
	if((!l ) || ( l->iwidth != w ) || ( l->iheight != h) || (! l->im)) error(E_INTERN);
	if((!c1) || (c1->iwidth != w ) || (c1->iheight != h) || (!c1->im)) error(E_INTERN);
	if((!c2) || (c2->iwidth != w ) || (c2->iheight != h) || (!c2->im)) error(E_INTERN);


	if(!init)
	{
		init=1;
		for(i=0;i<256;i++)
		{
			XL[i]= (5564 * i + 2048) * lum_mul;
			XC1[i]= 9085 * i - 1417185;
			XC2[i]= 7461 * i - 1022138;
			XC1g[i]= 274934 - 1762 * i;
			XC2g[i]= 520268 - 3798 * i;
		}
	}

	BUinit;
	if(do_bw)  {
		for( y=h-1; y >= 0; y-- )  {
			pl =  l->im + y *  l->mwidth;
			for(x=0;x<w;x++)  {
				L = XL[*pl++]>>BitShift;
				NORM(L);
				BUwrite1(L);
			}
		}
	} else {
		for( y=h-1; y >= 0; y-- )  {
			pl =  l->im + y *  l->mwidth;
			pc1= c1->im + y * c1->mwidth;
			pc2= c2->im + y * c2->mwidth;

			for(x=0;x<w;x++)  {
				L = XL[*pl];
				red  =(L + XC2[*pc2]               )>>BitShift;
				green=(L + XC1g[*pc1] + XC2g[*pc2] )>>BitShift;
				blue =(L + XC1[*pc1]               )>>BitShift;

				NORM(red);
				NORM(green);
				NORM(blue);

				BUwrite(red,green,blue);
				pl++;
				pc1++;
				pc2++;
			}
		}
	}
	BUflush;
}
#undef BitShift


struct ph1
{
	char  id1[8];
	uBYTE ww1[14];
	char  id2[20];
	char  id3[4*16+4];
	short ww2;
	char  id4[20];
	uBYTE ww3[2*16+1];
	char  id5[4*16];
	uBYTE idx[11*16];
};


static void druckeid(void)
{
	struct ph1 *d;
	char ss[100];

	d=(struct ph1 *)sbuffer;

#define dr(feld,kennung)   \
     strncpy(ss,feld,sizeof(feld));\
     ss[sizeof(feld)]=0;\
     fprintf(stderr,"%s: %s \n",kennung,ss);

#define db(feld) fprintf(stderr,"--%d\n",sizeof(feld)); for(i=0;i<sizeof(feld);i+=2) \
  fprintf(stderr,"%4d %6d\n",i,(signed int)((((unsigned int)feld[i])<<8)|feld[i+1]));\
  fprintf(stderr,"\n");

	dr(d->id1,"Id1")
	    dr(d->id2,"Id2")
	    dr(d->id3,"Id3")
	    dr(d->id4,"Id4")
	    dr(d->id5,"Id5")

	/*
		db(d->ww1)
		db(d->ww3)
		db(d->idx)
	*/

#undef dr
#undef db

}



#if 0
struct pcdword
{
	uBYTE high,low;
};

static int lpt[1024];

static void readlpt(w,h)
dim w,h;
{
	int i;
	struct pcdword *ptr;

	EREADBUF;

	ptr = (struct pcdword *)sbuffer;

	for(i=0;i<h/4;i++,ptr++)
	{
		lpt[i] = ((int)ptr->high)<<8 | ptr->low ;
	}



}
#endif


struct pcdquad {
	uBYTE len,highseq,lowseq,key;
};
struct pcdhqt  {
	uBYTE entries;
	struct pcdquad entry[256];
};
struct myhqt {
	unsigned long seq,mask,len;
	uBYTE key;
};


#define E ((unsigned long) 1)


static void readhqtsub(struct pcdhqt *source, struct myhqt *ziel, int *anzahl)
{
	int i;
	struct pcdquad *sub;
	struct myhqt *help;
	*anzahl=(source->entries)+1;

	for(i=0;i<*anzahl;i++)
	{
		sub = (struct pcdquad *)(((uBYTE *)source)+1+i*sizeof(*sub));
		help=ziel+i;

		help->seq = (((unsigned long) sub->highseq) << 24) |(((unsigned long) sub->lowseq) << 16);
		help->len = ((unsigned long) sub->len) +1;
		help->key = sub->key;

#ifdef DEBUGhuff
		fprintf(stderr," Anz: %d A1: %08x  A2: %08x X:%02x %02x %02x %02x Seq:  %08x   Laenge:  %d %d\n",
		    *anzahl,sbuffer,sub,((uBYTE *)sub)[0],((uBYTE *)sub)[1],((uBYTE *)sub)[2],((uBYTE *)sub)[3],
		    help->seq,help->len,sizeof(uBYTE));
#endif

		if(help->len > 16) error(E_HUFF);

		help->mask = ~ ( (E << (32-help->len)) -1);

	}
#ifdef DEBUG
	for(i=0;i<*anzahl;i++)
	{
		help=ziel+i;
		fprintf(stderr,"H: %3d  %08lx & %08lx (%2d) = %02x = %5d  %8x\n",
		    i, help->seq,help->mask,help->len,help->key,(signed char)help->key,
		    help->seq & (~help->mask));
	}
#endif

}

#undef E

static struct myhqt myhuff0[256],myhuff1[256],myhuff2[256];
static int          myhufflen0=0,myhufflen1=0,myhufflen2=0;

static void readhqt(dim w, dim h, int n)
{
	uBYTE *ptr;

	melde("readhqt\n");
	EREADBUF;
	ptr = sbuffer;

	readhqtsub((struct pcdhqt *)ptr,myhuff0,&myhufflen0);

	if(n<2) return;
	ptr+= 1 + 4* myhufflen0;
	readhqtsub((struct pcdhqt *)ptr,myhuff1,&myhufflen1);

	if(n<3) return;
	ptr+= 1 + 4* myhufflen1;
	readhqtsub((struct pcdhqt *)ptr,myhuff2,&myhufflen2);
}

/*
 *			D E C O D E
 *
 *  Read the input stream, decoding into the given buffer(s).
 */
static void decode(dim w, dim h, implane *f, implane *f1, implane *f2, int autosync)
{
	int i,htlen,sum;
	unsigned long maxwidth;
	register unsigned long sreg;
	unsigned int inh,n,zeile,segment,ident;
	struct myhqt *htptr,*hp;

	register uBYTE *nptr;
	uBYTE *lptr;

	melde("decode\n");
#define nextbuf  {  nptr=sbuffer;  EREADBUF; }
#define checkbuf { if (nptr >= sbuffer + sizeof(sbuffer)) nextbuf; }
#define shiftout(n){ sreg<<=n; inh-=n; \
                    while (inh<=24) \
                     {checkbuf; \
                      sreg |= ((unsigned long)(*(nptr++)))<<(24-inh);\
                      inh+=8;\
                     }\
                   }
#define issync ((sreg & 0xffffff00) == 0xfffffe00)
#define seeksync { while (!issync) shiftout(1);}


	if( f  && ((! f->im) || ( f->iheight < h  ) ||  (f->iwidth<w  ))) error(E_INTERN);
	if( f1 && ((!f1->im) || (f1->iheight < h/2) || (f1->iwidth<w/2))) error(E_INTERN);
	if( f2 && ((!f2->im) || (f2->iheight < h/2) || (f2->iwidth<w/2))) error(E_INTERN);

	htlen=sreg=maxwidth=0;
	htptr=0;
	nextbuf;
	inh=32;
	lptr=0;
	shiftout(16);
	shiftout(16);

	if(autosync) seeksync;

	n=0;
	for(;;)
	{
		if (issync)
		{
			shiftout(24);
			ident=sreg>>16;
			shiftout(16);

			zeile=(ident>>1) & 0x1fff;
			segment=ident>>14;

#ifdef DEBUG
			fprintf(stderr,"Ident %4x Zeile:  %6d  Segment %3d Pixels bisher: %5d   Position: %8lx\n",
			    ident,zeile,segment,n,bufpos);
#endif


			if(lptr && (n!=maxwidth)) error(E_SEQ1);
			n=0;

			if(zeile==h) {
				RPRINT;
				return;
			}
			if(zeile >h) error(E_SEQ2);

			switch(segment)
			{
			case 0:
				if((!f) && autosync) {
					seeksync;
					break;
				}
				if(!f) error(E_SEQ7);
				lptr=f->im + zeile*f->mwidth;
				maxwidth=f->iwidth;
				htlen=myhufflen0;
				htptr=myhuff0;
				break;

			case 2:
				if((!f1) && autosync) {
					seeksync;
					break;
				}
				if(!f1) error(E_SEQ7);
				lptr=f1->im + (zeile>>1)*f1->mwidth;
				maxwidth=f1->iwidth;
				htlen=myhufflen1;
				htptr=myhuff1;
				break;

			case 3:
				if((!f2) && autosync) {
					seeksync;
					break;
				}
				if(!f2) error(E_SEQ7);
				lptr=f2->im + (zeile>>1)*f2->mwidth;
				maxwidth=f2->iwidth;
				htlen=myhufflen2;
				htptr=myhuff2;
				break;

			default:
				error(E_SEQ3);
			}
		}
		else
		{
			register int	key;
			/*      if((!lptr) || (n>maxwidth)) error(E_SEQ4);*/
			if(!lptr)      error(E_SEQ6);
			if(n>maxwidth) error(E_SEQ4);
			for(i=0,hp=htptr;(i<htlen) && ((sreg & hp->mask)!= hp->seq); i++,hp++);
			if(i>=htlen) error(E_SEQ5);

			/* Ensure key is treated as signed byte */
			if( (key = hp->key) & 0x80 )
				key |= ((-1) & ~0x7F);
			sum=((int)(*lptr)) + key;
			/* NORM(sum); */
			if(sum<0) sum=0; else if (sum>255) sum=255;
			*(lptr++) = sum;

			n++;
			shiftout(hp->len);

		}

	}


#undef nextbuf
#undef checkbuf
#undef shiftout
#undef issync
#undef seeksync

}

static int testbegin(void)
{
	int i,j;
	for(i=j=0;i<32;i++)
		if(sbuffer[i]==0xff) j++;

	return (j>30);

}

static long Skip4Base(void)
{
	long cd_offset,cd_offhelp;

	cd_offset = L_Head + L_Base16 + L_Base4 + L_Base ;
	SEEK(cd_offset+3);
	EREADBUF;
	cd_offhelp = ( ((long)sbuffer[510]) << 8 ) | (sbuffer[511] + 1);

	cd_offset+=cd_offhelp;

	SEEK(cd_offset);
	EREADBUF;
	while(!testbegin())
	{
		cd_offset++;
		EREADBUF;
	}
	return cd_offset;
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
