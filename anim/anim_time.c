/*			A N I M _ T I M E . C
 *
 *  Given an animation path consiting of time stamps and 3-d points,
 *  estimate new time stamps based on the distances between points, the
 *  given starting and ending times, and optionally specified starting 
 *  and ending velocities.
 *	
 *  Author -
 *	Carl J. Nuzman
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1993 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */


#include <math.h>
#include <stdio.h>



extern int optind;
extern char *optarg;


#define MAXLEN		64
#define DIVIDE_TOL	(1.0e-10)
#define MAXITS		100
#define DELTA		(1.0e-6)

/* command line variables */
float inv0,inv1;
int v0_set =	 0;
int v1_set =	 0;
int query =	 0;
int verbose = 	 0;
int maxlines = 	 0;
int domem = 	 0;

float gettime(float dist,float a,float b,float c,float init)
{

	float old,new,temp,inv;
	int countdown,success;
	countdown = MAXITS;

	old = init;
	success = 0;
	while(countdown-->0){
		temp = (3.0*a*old+2.0*b)*old+c;
		if (temp<DIVIDE_TOL){
			new = 0.75*old;
		} else {
		 	new = old - (((a*old+b)*old+c)*old-dist)/temp;
		}
		if (((new-old)<DELTA)&&((old-new)<DELTA)){
			success = 1;
			break;
		}
		/*printf("c: %d %f\t%f\n",countdown,new,new-old);*/
		old = new;
	}
	if (!success) fprintf(stderr,"warning - max iterations reached\n");
	return (new);

}

main(argc,argv)
int argc;
char **argv;
{
	float *l, *x, *y, *z;
	float temp0,temp1,temp2,start,end,v0,v1;
	int i,j , num,plen;
	

	float time,dist,slope,a,b,c;


	plen = 0;

	if (!get_args(argc,argv))
		fprintf(stderr,"Usage: anim_time [-s#] [-e#] [-d] < in.table\n");

	if (!domem) {
		maxlines = MAXLEN;
	}

	l = (float *) rt_malloc(maxlines*sizeof(float),"l[]");
	if (verbose) {
		x = (float *) rt_malloc(maxlines*sizeof(float),"x[]");
		y = (float *) rt_malloc(maxlines*sizeof(float),"y[]");
		z = (float *) rt_malloc(maxlines*sizeof(float),"z[]");
	} else {
		x = (float *) rt_malloc(2*sizeof(float),"x[]");
		y = (float *) rt_malloc(2*sizeof(float),"y[]");
		z = (float *) rt_malloc(2*sizeof(float),"z[]");
	}
	l[0] = 0.0;

	while(plen<maxlines){
		i = (verbose) ? plen : plen%2;
		j = (verbose) ? (plen-1) : (plen+1)%2;
		num = scanf("%f %f %f %f",&end,x+i,y+i,z+i);
		if (num<4)
			break;
		if(plen){
			temp0 = x[i]-x[j];
			temp1 = y[i]-y[j];
			temp2 = z[i]-z[j];
			l[plen] = sqrtf(temp0*temp0+temp1*temp1+temp2*temp2);
			l[plen] += l[plen-1];
		} else {
			start = end;
		}
		
		plen++;
	}

	time = end - start;
	dist = l[plen-1];

	if (query){
		printf("%f\n",dist);
		return(0);
	}

	if (time < DIVIDE_TOL){
		fprintf(stderr,"TIME TOO SMALL! %f\n",time);
		exit(-1);
	}
	if (dist < DIVIDE_TOL){
		fprintf(stderr,"PATHLENGTH TOO SMALL! %f\n",dist);
		exit(-1);
	}
	slope = dist/time;
	v0 = v1 = slope;
	if (v0_set){
		v0 = inv0;
	}
	if (v1_set){
		v1 = inv1;
	}
	v0 = (v0<0.0) ? 0.0 : v0;
	v1 = (v1<0.0) ? 0.0 : v1;
	v0 = (v0>3*slope) ? 3*slope : v0;
	v1 = (v1>3*slope) ? 3*slope : v1;


	a = ((v1+v0) - 2.0*slope)/(time*time);
	b = (3*slope - (v1+2.0*v0))/time;
	c = v0;

	temp2 = 1.0/slope;
	if (verbose) {
		printf("%.12e\t%.12e\t%.12e\t%.12e\n",start,x[0],y[0],z[0]);
		for (i=1; i<plen-1; i++){
			temp0 = gettime(l[i],a,b,c,l[i]*temp2);
			printf("%.12e\t%.12e\t%.12e\t%.12e\n",temp0+start,x[i],y[i],z[i]);
		}
		printf("%.12e\t%.12e\t%.12e\t%.12e\n",end,x[plen-1],y[plen-1],z[plen-1]);
	} else {
		printf("%.12e\n",start);
		for (i=1; i<plen-1; i++){
			temp0 = gettime(l[i],a,b,c,l[i]*temp2);
			printf("%.12e\n",temp0+start);
		}
		printf("%.12e\n",end);
	}

	rt_free((char *) l, "l[]");
	rt_free((char *) x, "x[]");
	rt_free((char *) y, "y[]");
	rt_free((char *) z, "z[]");

}

/* code to read command line arguments*/
#define OPT_STR "s:e:qm:v"
int get_args(argc,argv)
int argc;
char **argv;
{
	int c;

	while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
		case 's':
			sscanf(optarg,"%f",&inv0);
			v0_set = 1;
			break;
		case 'e':
			sscanf(optarg,"%f",&inv1);
			v1_set = 1;
			break;
		case 'q':
			query = 1;
			break;
		case 'm':
			sscanf(optarg,"%d",&maxlines);
			domem = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}
