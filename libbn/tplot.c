/*
 *
 *			T P _ P L O T
 *
 *
 *			George W. Hartwig, Jr.
 *				16 March 1979
 *
 *
 *	This routine is designed to simplify the creation of 
 * X,Y plots for user. The user need only furnish this program
 * the data arrays to be plotted, the lengths of the respective
 * axis, titles for the axis, and the point on the page corresponding
 * to data point (0,0).
 *	The program will then do everything else required to make
 * the plot appear on the user's terminal including scaling of the 
 * data, centering of the titles and positioning on the page.
 *
 *
 * where
 *	int xp,yp	page point corresponding to (0,0) of the data
 *	int xl,yl	lengths of the x and y axis, respectively
 *	char xtitle[], ytitle[]	titles for the axis
 *	float x[], y[]	the floating point data arrays
 *	int n		the number of points in the data arrays
 *
 *
 *		R E V I S I O N  H I S T O R Y
 *
 *	WHO	WHEN		WHAT
 *	GWH	5/21/79		Modified ftoa so that nos. < e-15
 *				map to zero.
 *	GWH	6/29/79		Changed the axis drawing loops to
 *				prevent a one tic mark overrun.
 *	GWH	7/10/79		Subtracted one from n to allow for the
 *				fact that fortran arrays start at one
 *				and not zero as with c.
 */
#include <stdio.h>
#include <math.h>

#define TIC		100
#define REF_WIDTH	0.857143
#define NUM_DISTANCE	250
#define LAB_LNGTH	860

tp_plot( fp, xp, yp, xl, yl, xtitle, ytitle, x, y, n, cscale )
FILE	*fp;
int	xp, yp;		/* page point desired to be (0,0) for plot */
int	xl, yl;		/* lengths of x,y axis */
char	xtitle[];
char	ytitle[];
float	x[], y[];
int	n;		/* number of points */
double	cscale;		/* character scale factor */
{
	int  ddx, ddy, xend, yend, xpen, ypen;
	float fxl, fyl, xs, ys, xmin, xmax, ymin, ymax, dx, dy;
	float lab;
	int xtics, ytics, i, xtl, ytl, j;
	int ix[101], iy[101], isave;
	char str[32];

	if( xl == 0 ){
		j = 0;
		goto loop;
	}
	fxl = xl/1000.0;
	fyl = yl/1000.0;
	n -= 1; /* allow for the fact that fortran starts arrays at 1 */
	fixsc (x, n, fxl, &xs, &xmin, &xmax, &dx);
	fixsc (y, n, fyl, &ys, &ymin, &ymax, &dy);
	ddx = dx*xs;
	ddy = dy*ys;
	xtics = LAB_LNGTH / ddx + 1.0;
	ytics = 500/ddy + 1.0;
	xend = xl+xp;
	xpen = xp;

	pl_move(fp, xpen, yp-TIC);
	pl_cont(fp, xpen,yp);

	/* label first tic */
	lab = xmin;
	sprintf( str, "%3.3g", xmin );
	tp_2symbol( fp, str, xpen-171,yp-TIC-NUM_DISTANCE, cscale, 0.0);
	i = 0;
	while((xpen+ddx)<=xend){
		i++;
		xpen += ddx;
		pl_line( fp, xpen, yp, xpen, yp-TIC );
		/* while here label this tic mark if no overlapping will occur */
		lab += dx;
		/* need if test here to check for overlap */
		if( (i%xtics) == 0){
			sprintf( str, "%3.3g", lab );
			tp_2symbol( fp, str, xpen-171, yp-TIC-NUM_DISTANCE, cscale, 0.0);
		}
	}

	/* insert axis label here */
	xtl = xp+(xl - strlen(xtitle)*cscale)/2;
	ytl = yp - 8 * cscale;
	tp_2symbol( fp, xtitle,xtl, ytl, 100, 0.0);
	yend = yl+yp;
	ypen= yp;
	pl_line( fp, xp-TIC, ypen, xp, ypen );

	/* draw first y label */
	lab = ymin;
	sprintf( str, "%3.3g", lab );
	tp_2symbol( fp,str, xp-TIC-LAB_LNGTH-NUM_DISTANCE, ypen, cscale, 0.0);

	i=0;
	while((ypen+ddy)<=yend){
		i++;
		ypen += ddy;
		pl_line( fp, xp, ypen, xp-TIC, ypen );
		/* label the y-axis now, nicely */
		lab += dy;
		if(( i%ytics) ==0){
			sprintf( str, "%3.3g", lab );
			tp_2symbol( fp,str, xp-TIC-LAB_LNGTH-NUM_DISTANCE, ypen, cscale, 0.0);
		}
	}

	/* insert y-axis title here */
	xtl= xp-1500;
	ytl= yp + (yl - strlen(ytitle)*cscale)/2 ;
	tp_2symbol( fp,ytitle,xtl,ytl,100,90.0);

	/* now at long last plot the data */
	j = 0;
loop:
	if( n <= 100 )
		isave = n-1;
	else{
		isave = 100;
		n -= 101;
	}
	if(j == 0){
		ix[0] = (x[j] - xmin)*xs + xp;
		iy[0] = (y[j] - ymin)*ys + yp;
		j++;
	}
	else{
		ix[0] = (x[j-1] - xmin)*xs + xp;
		iy[0] = (y[j-1] - ymin)*ys + yp;
	}
	i = 1;
	while( i <= isave ){
		ix[i] = (x[j] - xmin)*xs + xp;
		iy[i] = (y[j] - ymin)*ys + yp;
		i++;
		j++;
	}
	tp_2list( fp, ix, iy, isave+1 );
	if( isave == 100 )
		goto loop;
}

/*
 *			F T O A
 *
 *
 *			George W Hartwig, Jr.
 *			16 March 1979
 *
 * This routine converts a floating point number into a string
 * of ascii characters of the form "sX.XXXesXX". The string is
 * null terminated.
 */
ftoa(x, s)
float x;
char *s;
{
	int exp,tmp;
	float coef;
	char esgn, nsgn;
	char i;
	sep(x, &coef, &exp);
	if( exp < -15 ){
		exp = 0;
		*s++ = '0';
		*s++ = '.';
		*s++ = '0';
		*s++ = '0';
		*s++ = '0';
		*s++ = 'e';
		*s++ = '+';
		*s++ = '0';
		*s++ = '0';
		*s   =  0 ;
		return;
	}
	if(exp < 0){
		esgn = '-';
		exp = -exp;
	}
	else
		esgn = '+';
	if( coef < 0.0){
		nsgn = '-';
		coef = -coef;
	}
	else
		nsgn = ' ';
	*s++ = nsgn;
	/* load the first numeral and the decimal point */
	tmp = coef;
	*s++ = tmp + '0';
	coef = (coef - tmp)*10.0;
	*s++ = '.';
	/* now do the three after the decimal */
	for( i=1 ; i<=3 ; ++i){
		tmp = coef;
		coef = (coef - tmp)*10.0;
		*s++ = tmp + '0';
	}
	/* put the e in */
	*s++ = 'e';
	/* the sign for the exponent */
	*s++ = esgn;

	/* and the exponent */
	if( exp < 0)
		exp = -exp;
	if( exp < 10 ){
		*s++ = '0';
		*s++ = exp + '0';
	}
	else{
		tmp = exp/10;
		*s++ = tmp + '0';
		exp = exp - tmp*10;
		*s++ = exp +'0';
	}
	/* add a null byte terminator */
	*s = 0;
}
/*
 *			F I X S C
 *
 *		written by George Hartwig
 *			6 March 1979
 *
 *   fixsc is a scaling routine intended to be used in conjunction
 *   with plotting routines. What fixsc does is scale the user supplied
 *   data so that it fits on a specified axis and has 'nice' numbers
 *   for labels.
 *
 *   Calling sequence
 *
 *   fixsc(x, npts, size, xs, xmin, xmax, dx)
 *   where
 *
 *	float x[]	the data array to be scaled
 *	int npts	the number of elements in x[]
 *	float size	the length into which x[] is supposed to be fitted
 *			(in inches)
 *	float *xs	the returned scale facter to integer space
 *	float *xmin	the new minimum value for the data array (a returned
 *			value)
 *	float *xmax	the new maximum value for the data array (a returned
 *			value)
 *	float *dx	the value in data units between tic marks (a returned
 *			value)
 *
 */
fixsc(x,npts,size,xs,xmin,xmax,dx)
int npts;
float x[], size, *xs, *xmin, *xmax, *dx ;
{
	float txmi, txma, coef, delta, diff;
	float pwr(), tabs();
	int i, exp;
	txmi=txma=x[0];
	i = 0;
	while( i <= npts ) {
		if( x[i] < txmi)
			txmi = x[i];
		if( x[i] > txma)
			txma = x[i];
		i++;
	}
	diff = txma - txmi;
	if( diff < .000001 )
		diff = .000001;
	sep (diff, &coef, &exp);
	if( coef < 2.0 )
		delta = .1;
	else if ( coef < 4.0 )
		delta = .2;
	else
	    delta = .5;
	i = 0;
	if(exp < 0 ){
		exp = -exp;
		i=12;
	}
	delta *= pwr(10.0,exp);
	if(i == 12)
		delta = 1.0/delta;
	*dx = delta;
	i = (tabs(txmi)/delta);
	*xmin = i*delta;
	if( txmi < 0.0 )
		*xmin = -(*xmin+delta);
	i = (tabs(txma)/delta);
	*xmax = i*delta;
	if( txma < 0.0)
		*xmax = - *xmax;
	else
	    *xmax = *xmax+delta;
	*xs = 1000.*size/(*xmax - *xmin);
}
sep( x, coef, exp )
/*
 *  sep() divides a floating point number into a coefficient
 *  and an exponent. works in base ten.
 */
float x, *coef;
int *exp;
{
	int i, isv;
	float xx;
	isv = 1;
	if(x < 0.0 ){
		isv = -1;
		x = -x;
	}
	if( x > 1.0 ){
		xx = x;
		*exp = 0;
		*coef = 0.0;
		if ( xx < 10.0){
			*coef = xx*isv;
			return;
		}
		for ( i=1 ; i < 39 ; ++i){
			*exp += 1;
			xx = xx/10.0;
			if( xx < 10.0 )
				break;
		}
		*coef = xx*isv;
		return;
	}
	else{
		xx = x;
		*exp = 0;
		*coef = 0.0;
		for ( i=1 ; i<39 ; ++i){
			*exp -= 1;
			xx *= 10.0;
			if( xx >= 1.0 )
				break;
		}
		*coef = xx*isv;
		return;
	}
}
float pwr (x, n)
/*
 *  pwr() raises a floating point number to a positve integer
 *  power.
 */
float x;
int n;
{
	return(n>0?x*pwr(x,n-1):1);
}
float tabs(x)
/*
 *  This routine returns the absolute value of a floating
 *  point number.
 */
float x;
{
	return(x>0.0?x:-x);
}


/*
 *	CULC FORTRAN-IV Interface Entry
 */

FPLOT(xp, yp, xl, yl, xtitle, ytitle, x, y, n )
char *xtitle, *ytitle;
int *xp, *yp, *xl, *yl, *n;
float *x, *y;
{
	tp_plot(*xp, *yp, *xl, *yl, xtitle, ytitle, x, y, *n);
}
