/*
 *			C O S W I N
 *
 *  Do Cosine windowing effecting p percent of the samples in
 *  the buffer.  Precomputes the window function.
 */
#include <stdlib.h>
#include <stdio.h>	/* for stderr */
#include <math.h>	/* for double sin(), cos() */
#include "./complex.h"

static int	_init_length = 0;	/* Internal: last initialized size */
static int	maxinitlen = 0;
static double	*coswintab = NULL;

int init_coswintab( int size );

void
coswin( data, length, percent )
double	*data;
int	length;
double	percent;
{
	int	num, i;

	num = percent * length/2 + 0.5;

	/* Check for window table initialization */
	if( num != _init_length ) {
		if( init_coswintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < num; i++ ) {
		data[i] *= coswintab[i];
		data[length-i-1] *= coswintab[i];
	}
}

/*
 * Complex Data Version.
 */
void
ccoswin( data, length, percent )
COMPLEX	*data;
int	length;
double	percent;
{
	int	num, i;

	num = percent * length/2 + 0.5;

	/* Check for window table initialization */
	if( num != _init_length ) {
		if( init_coswintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < num; i++ ) {
		data[i].re *= coswintab[i];
		data[length-i-1].re *= coswintab[i];
	}
}

/*
 *		I N I T _ C O S W I N T A B
 *
 * Internal routine to initialize the cosine window table for
 *  a given effect length (number of sample at each end effected).
 */
int
init_coswintab( size )
int size;
{
	int	i;
	double	theta;

	if( size > maxinitlen ) {
		if( coswintab != NULL ) {
			free( coswintab );
			maxinitlen = 0;
		}
		if( (coswintab = (double *)malloc(size*sizeof(double))) == NULL ) {
			fprintf( stderr, "coswin: couldn't malloc space for %d elements\n", size );
			return( 0 );
		}
		maxinitlen = size;
	}

	/* Check for odd lengths? XXX */

	/*
	 * Size is okay.  Set up tables.
	 */
	for( i = 0; i < size; i++ ) {
		theta = PI * i / (double)(size);
		coswintab[ i ] = 0.5 - 0.5 * cos( theta );
	}

	/*
	 * Mark size and return success.
	 */
	_init_length = size;
	return( 1 );
}
