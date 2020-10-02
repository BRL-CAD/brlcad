/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * gdiam_test.cpp -
 *     Test code for computing diameter and tight-fitting
 * bounding box.
 *
 * Copyright 2000 Sariel Har-Peled (ssaarriieell@cs.uiuc.edu)
 * https://sarielhp.org/research/papers/00/diameter/diam_prog.html
 *
 * Note:  gdiam is available under multiple licenses - we make
 * use of it under the MIT license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Code is based on the paper:
 *   A Practical Approach for Computing the Diameter of a
 *   Point-Set (in ACM Sym. on Computation Geometry SoCG 2001)
 *   Sariel Har-Peled (http://www.uiuc.edu/~sariel)
 *---------------------------------------------------------------
 * History:
 * 3/28/01 - Extended test program, so that it can load a
 *           text-file with points. Format is
 *                    [npoints]
 *                    [x_1 y_1 z_1]
 8                     ...
 *                    [x_n y_n z_n]
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include  <memory.h>
#include  <math.h>

#include  "bu/app.h"
#include  "gdiam.hpp"

/*--- Start of Code ---*/


void   test_itself( gdiam_real  * points, int  num )
{
    GPointPair   pair;

    printf( "Computing the diameter for %d points selected "
            "uniformly from the unit cube\n", num );
    pair = gdiam_approx_diam_pair( (gdiam_real *)points, num, 0.0 );
    printf( "Diameter distance: %g\n", pair.distance );
    printf( "Points realizing the diameter\n"
            "\t(%g, %g, %g) - (%g, %g, %g)\n",
            pair.p[ 0 ], pair.p[ 1 ], pair.p[ 2 ],
            pair.q[ 0 ], pair.q[ 1 ], pair.q[ 2 ] );


    gdiam_point  * pnt_arr;
    gdiam_bbox   bb;

    pnt_arr = gdiam_convert( (gdiam_real *)points, num );

    printf( "Computing a tight-fitting bounding box of the point-set\n" );
    bb = gdiam_approx_mvbb_grid_sample( pnt_arr, num, 5, 400 );

    printf( "Resulting bounding box:\n" );
    bb.dump();

    printf( "Axis parallel bounding box\n" );
    GBBox   bbx;
    bbx.init();
    for  ( int  ind = 0; ind < num; ind++ )
        bbx.bound( points + (ind * 3) );
    bbx.dump();
        
}


void   standard_test()
{
    gdiam_real  * points;
    int  num;

    num = 1000000;

    points = (gdiam_point)malloc( sizeof( gdiam_point_t ) * num );
    assert( points != NULL );

    // Pick randomly points from the unit cube */
    for  ( int  ind = 0; ind < num; ind++ ) {
        points[ ind * 3 + 0 ] = drand48();
        points[ ind * 3 + 1 ] = drand48();
        points[ ind * 3 + 2 ] = drand48();
    }

    test_itself( points, num );
}


void  read_points( FILE   * fl, gdiam_real  * points, int  points_num )
{
    int  args;
    double  x, y, z;

    for  ( int  ind = 0; ind < points_num; ind++ ) {
        args = fscanf( fl, "%lg %lg %lg\n", &x, &y, &z );
        assert( args == 3 );

        points[ ind * 3 + 0 ] = x;
        points[ ind * 3 + 1 ] = y;
        points[ ind * 3 + 2 ] = z;
    }
}


void  test_file( const char  * file_name )
{
    gdiam_real  * points;
    FILE   * fl;
    int  args, points_num;

    fl = fopen( file_name, "rt" );
    if  ( fl == NULL ) {
        printf( "Unable to open file: [%s]\n", file_name );
        exit( -1 );
    }
    args = fscanf( fl, "%d\n", &points_num );
    assert( ( args > 0 )  &&  ( points_num > 0 ) );

    points = (gdiam_point)malloc( sizeof( gdiam_point_t ) * points_num );
    assert( points != NULL );

    read_points( fl, points, points_num );
    fclose( fl );

    test_itself( points, points_num );
}


int  main( int  argc, char  ** argv )
{
    bu_setprogname(argv[0]);
    if  ( argc == 1 ) {
        standard_test();
        return  0;
    }

    for  ( int  ind = 1; ind < argc; ind++ )
        test_file( argv[ ind ] );

    return  0;
}

/* gdiam_test.C - End of File ------------------------------------------*/
