/*
 *			H A M W I N
 *
 *  Apply a Hamming Window to the given samples.
 *  Precomputes the window function.
 */
#include <stdlib.h>
#include <stdio.h>	/* for stderr */
#include <math.h>	/* for double sin(), cos() */
#include "./complex.h"

int init_hamwintab( int size );

static int	_init_length = 0;	/* Internal: last initialized size */
static int	maxinitlen = 0;
static double	*hamwintab = NULL;

void
hamwin(double *data, int length)
{
	int	i;

	/* Check for window table initialization */
	if( length != _init_length ) {
		if( init_hamwintab( length ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < length; i++ ) {
		data[i] *= hamwintab[i];
	}
}

/*
 * Complex Data Version.
 */
void
chamwin(COMPLEX *data, int length)
{
	int	i;

	/* Check for window table initialization */
	if( length != _init_length ) {
		if( init_hamwintab( length ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < length; i++ ) {
		data[i].re *= hamwintab[i];
	}
}

/*
 *		I N I T _ H A M W I N T A B
 *
 *  Internal routine to initialize the hamming window table
 *  of a given length.
 *  Returns zero on failure.
 */
int
init_hamwintab(int size)
{
	int	i;
	double	theta;

	if( size > maxinitlen ) {
		if( hamwintab != NULL ) {
			free( hamwintab );
			maxinitlen = 0;
		}
		if( (hamwintab = (double *)malloc(size*sizeof(double))) == NULL ) {
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
		theta = TWOPI * i / (double)(size);
		hamwintab[ i ] = 0.54 - 0.46 * cos( theta );
	}

	/*
	 * Mark size and return success.
	 */
	_init_length = size;
	return( 1 );
}
