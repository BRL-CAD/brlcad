/*
Matrix Inversion
by Richard Carling
from "Graphics Gems", Academic Press, 1990
*/

#define SMALL_NUMBER	1.e-8
/* 
 *   inverse( original_matrix, inverse_matrix )
 * 
 *    calculate the inverse of a 4x4 matrix
 *
 *     -1     
 *     A  = ___1__ adjoint A
 *         det A
 */

#include <stdlib.h>
#include <math.h>

typedef float Matrix4[4][4];

static void adjoint(Matrix4 in, Matrix4 out);
static float det4x4(Matrix4 m);
static float det3x3(float a1, float a2, float a3,
		    float b1, float b2, float b3,
		    float c1, float c2, float c3 );
static float det2x2( float a, float b, float c, float d);

void mat4Inverse( Matrix4 dst, Matrix4 src ) 
{
    int i, j;
    float det;

    /* calculate the adjoint matrix */

    adjoint( src, dst );

    /*  calculate the 4x4 determinent
     *  if the determinent is zero, 
     *  then the inverse matrix is not unique.
     */

    det = det4x4( src );

    if ( fabs( det ) < SMALL_NUMBER) {
        printf("Non-singular matrix, no inverse!\n");
        exit(12);
    }

    /* scale the adjoint matrix to get the inverse */

    for (i=0; i<4; i++)
        for(j=0; j<4; j++)
	    dst[i][j] = dst[i][j] / det;
}


/* 
 *   adjoint( original_matrix, inverse_matrix )
 * 
 *     calculate the adjoint of a 4x4 matrix
 *
 *      Let  a   denote the minor determinant of matrix A obtained by
 *           ij
 *
 *      deleting the ith row and jth column from A.
 *
 *                    i+j
 *     Let  b   = (-1)    a
 *          ij            ji
 *
 *    The matrix B = (b  ) is the adjoint of A
 *                     ij
 */

static void adjoint( Matrix4 in, Matrix4 out ) 
{
    float a1, a2, a3, a4, b1, b2, b3, b4;
    float c1, c2, c3, c4, d1, d2, d3, d4;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in[0][0]; b1 = in[0][1]; 
	c1 = in[0][2]; d1 = in[0][3];

	a2 = in[1][0]; b2 = in[1][1]; 
	c2 = in[1][2]; d2 = in[1][3];

	a3 = in[2][0]; b3 = in[2][1];
	c3 = in[2][2]; d3 = in[2][3];

	a4 = in[3][0]; b4 = in[3][1]; 
	c4 = in[3][2]; d4 = in[3][3];


    /* row column labeling reversed since we transpose rows & columns */

    out[0][0]  =   det3x3( b2, b3, b4, c2, c3, c4, d2, d3, d4);
    out[1][0]  = - det3x3( a2, a3, a4, c2, c3, c4, d2, d3, d4);
    out[2][0]  =   det3x3( a2, a3, a4, b2, b3, b4, d2, d3, d4);
    out[3][0]  = - det3x3( a2, a3, a4, b2, b3, b4, c2, c3, c4);
        
    out[0][1]  = - det3x3( b1, b3, b4, c1, c3, c4, d1, d3, d4);
    out[1][1]  =   det3x3( a1, a3, a4, c1, c3, c4, d1, d3, d4);
    out[2][1]  = - det3x3( a1, a3, a4, b1, b3, b4, d1, d3, d4);
    out[3][1]  =   det3x3( a1, a3, a4, b1, b3, b4, c1, c3, c4);
        
    out[0][2]  =   det3x3( b1, b2, b4, c1, c2, c4, d1, d2, d4);
    out[1][2]  = - det3x3( a1, a2, a4, c1, c2, c4, d1, d2, d4);
    out[2][2]  =   det3x3( a1, a2, a4, b1, b2, b4, d1, d2, d4);
    out[3][2]  = - det3x3( a1, a2, a4, b1, b2, b4, c1, c2, c4);
        
    out[0][3]  = - det3x3( b1, b2, b3, c1, c2, c3, d1, d2, d3);
    out[1][3]  =   det3x3( a1, a2, a3, c1, c2, c3, d1, d2, d3);
    out[2][3]  = - det3x3( a1, a2, a3, b1, b2, b3, d1, d2, d3);
    out[3][3]  =   det3x3( a1, a2, a3, b1, b2, b3, c1, c2, c3);
}
/*
 * float = det4x4( matrix )
 * 
 * calculate the determinent of a 4x4 matrix.
 */
static float det4x4( Matrix4 m ) 
{
    float ans;
    float a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4, d1, d2, d3, 			d4;

    /* assign to individual variable names to aid selecting */
	/*  correct elements */

	a1 = m[0][0]; b1 = m[0][1]; 
	c1 = m[0][2]; d1 = m[0][3];

	a2 = m[1][0]; b2 = m[1][1]; 
	c2 = m[1][2]; d2 = m[1][3];

	a3 = m[2][0]; b3 = m[2][1]; 
	c3 = m[2][2]; d3 = m[2][3];

	a4 = m[3][0]; b4 = m[3][1]; 
	c4 = m[3][2]; d4 = m[3][3];

    ans = a1 * det3x3( b2, b3, b4, c2, c3, c4, d2, d3, d4)
        - b1 * det3x3( a2, a3, a4, c2, c3, c4, d2, d3, d4)
        + c1 * det3x3( a2, a3, a4, b2, b3, b4, d2, d3, d4)
        - d1 * det3x3( a2, a3, a4, b2, b3, b4, c2, c3, c4);
    return ans;
}



/*
 * float = det3x3(  a1, a2, a3, b1, b2, b3, c1, c2, c3 )
 * 
 * calculate the determinent of a 3x3 matrix
 * in the form
 *
 *     | a1,  b1,  c1 |
 *     | a2,  b2,  c2 |
 *     | a3,  b3,  c3 |
 */

static float det3x3( float a1, float a2, float a3,
		     float b1, float b2, float b3,
		     float c1, float c2, float c3 )
{
    float ans;

    ans = a1 * det2x2( b2, b3, c2, c3 )
        - b1 * det2x2( a2, a3, c2, c3 )
        + c1 * det2x2( a2, a3, b2, b3 );
    return ans;
}

/*
 * float = det2x2( float a, float b, float c, float d )
 * 
 * calculate the determinent of a 2x2 matrix.
 */

static float det2x2( float a, float b, float c, float d)
{
    float ans;
    ans = a * d - b * c;
    return ans;
}
