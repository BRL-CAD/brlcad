/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * gdiam.hpp -
 *     Implmeents an algorithm for computing a diameter,
 *
 * Copyright 2001 Sariel Har-Peled (ssaarriieell@cs.uiuc.edu)
 *
 * * the GNU General Public License as published by the Free
 *   Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 * or
 *
 * * the GNU Lesser General Public License as published by the Free
 *   Software Foundation; either version 2.1, or (at your option)
 *   any later version.
 *
 *
 * Code is based on the paper:
 *   A Practical Approach for Computing the Diameter of a Point-Set.
 *   Sariel Har-Peled (http://www.uiuc.edu/~sariel)
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#ifndef  __GDIAM__H
#define  __GDIAM__H

/* for g++ to quell warnings */
#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__))
#  pragma GCC diagnostic push /* start new diagnostic pragma */
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#elif (defined(__clang__) && (__clang_major__ > 2 || (__clang_major__ == 2 && __clang_minor__ >= 8)))
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#ifndef GDIAM_EXPORT
#  if defined(GDIAM_DLL_EXPORTS) && defined(GDIAM_DLL_IMPORTS)
#    error "Only GDIAM_DLL_EXPORTS or GDIAM_DLL_IMPORTS can be defined, not both."
#  elif defined(GDIAM_DLL_EXPORTS)
#    define GDIAM_EXPORT __declspec(dllexport)
#  elif defined(GDIAM_DLL_IMPORTS)
#    define GDIAM_EXPORT __declspec(dllimport)
#  else
#    define GDIAM_EXPORT
#  endif
#endif

#define  GDIAM_DIM   3
typedef  double  gdiam_real;
typedef  gdiam_real    gdiam_point_t[ GDIAM_DIM ];
typedef  gdiam_real    gdiam_point_2d_t[ 2 ];
typedef  gdiam_real  * gdiam_point_2d;
typedef  gdiam_real  * gdiam_point;
typedef  const gdiam_real  * gdiam_point_cnt;

#ifndef __MINMAX_DEFINED
#define __MINMAX_DEFINED

#include<algorithm>
#define min std::min
#define max std::max

#endif /* MIN_MAX */

template <class T>
inline void     gdiam_exchange( T   & a, T    & b )
{
    T   tmp = a;

    a = b;
    b = tmp;
}

inline gdiam_real   pnt_length( const gdiam_point  pnt ) 
{
    return  sqrt( pnt[ 0 ] * pnt[ 0 ] + pnt[ 1 ] * pnt[ 1 ]
                  + pnt[ 2 ] * pnt[ 2 ] );
}

inline void  pnt_normalize( gdiam_point  pnt )
{
    gdiam_real  len = pnt_length( pnt );
    if  ( len == 0.0 )
        return;

    pnt[ 0 ] /= len;
    pnt[ 1 ] /= len;
    pnt[ 2 ] /= len;
}

inline    void  pnt_copy( gdiam_point_t   dest,
                    gdiam_point_t   src )
    {
        dest[ 0 ] = src[ 0 ];
        dest[ 1 ] = src[ 1 ];
        dest[ 2 ] = src[ 2 ];
    }
inline void  pnt_zero( gdiam_point  dst ) {
    dst[ 0 ] = dst[ 1 ] = dst[ 2 ] = 0;
}
inline void  pnt_dump( gdiam_point_cnt  pnt ) {
    printf( "(%g, %g, %g)\n", pnt[ 0 ], pnt[ 1 ], pnt[ 2 ] );
}


inline gdiam_real  pnt_dot_prod( gdiam_point_cnt  a,
                                 gdiam_point_cnt  b )
{
    return  a[ 0 ] * b[ 0 ]
        + a[ 1 ] * b[ 1 ]
        + a[ 2 ] * b[ 2 ];
}

inline void   pnt_cross_prod( const gdiam_point   a, 
                              const gdiam_point   b,
                              const gdiam_point   out )
{    
    out[ 0 ] = a[ 1 ] * b[ 2 ] - a[ 2 ] * b[ 1 ];
    out[ 1 ] = - ( a[ 0 ] * b[ 2 ] - a[ 2 ] * b[ 0 ] );
    out[ 2 ] = a[ 0 ] * b[ 1 ] - a[ 1 ] * b[ 0 ];
}


inline gdiam_real   pnt_distance_2d( gdiam_point_2d  p, 
                                     gdiam_point_2d  q )
{
    gdiam_real  valx = (p[ 0 ] - q[ 0 ]);
    gdiam_real  valy = (p[ 1 ] - q[ 1 ]);

    return  sqrt( valx * valx + valy * valy );
}


inline gdiam_real   pnt_distance( gdiam_point  p, gdiam_point  q )
{
    gdiam_real  valx = (p[ 0 ] - q[ 0 ]);
    gdiam_real  valy = (p[ 1 ] - q[ 1 ]);
    gdiam_real  valz = (p[ 2 ] - q[ 2 ]);

    return  sqrt( valx * valx + valy * valy + valz * valz );
}

inline gdiam_real   pnt_distance( gdiam_point  p, gdiam_point  q,
                                  gdiam_point_cnt  dir )
{
    gdiam_real  valx = (p[ 0 ] - q[ 0 ]);
    gdiam_real  valy = (p[ 1 ] - q[ 1 ]);
    gdiam_real  valz = (p[ 2 ] - q[ 2 ]);

    gdiam_real  len, proj_len;

    len = sqrt( valx * valx + valy * valy + valz * valz );

    proj_len = dir[ 0 ] * valx + dir[ 1 ] * valy + dir[ 2 ] * valz;

    return  sqrt( len * len - proj_len * proj_len );
}

inline void  pnt_init( gdiam_point  pnt, 
                       gdiam_real  x,
                       gdiam_real  y,
                       gdiam_real  z )
{
    pnt[ 0 ] = x;
    pnt[ 1 ] = y;
    pnt[ 2 ] = z;
}


inline void  pnt_init_normalize( gdiam_point  pnt, 
                                 gdiam_real  x,
                                 gdiam_real  y,
                                 gdiam_real  z )
{
    pnt[ 0 ] = x;
    pnt[ 1 ] = y;
    pnt[ 2 ] = z;

    pnt_normalize( pnt );
}

inline bool  pnt_isEqual( const gdiam_point  p,
                          const gdiam_point  q ) 
{
    // Assuming here the GDIAM_DIM == 3 !!!!
    return  ( ( p[ 0 ] == q[ 0 ] )
              &&  ( p[ 1 ] == q[ 1 ] )
              &&  ( p[ 2 ] == q[ 2 ] ) );
}

inline void  pnt_scale_and_add( gdiam_point  dest,
                         gdiam_real  coef,
                         gdiam_point_cnt   vec ) {
    dest[ 0 ] += coef * vec[ 0 ];
    dest[ 1 ] += coef * vec[ 1 ];
    dest[ 2 ] += coef * vec[ 2 ];
}

class  GPointPair
{
public:
    gdiam_real  distance;
    gdiam_point  p, q;

    GPointPair() : p(), q() {
        distance = 0.0;
    }

    void  init( const gdiam_point  _p, 
                const gdiam_point  _q ) {
        p = _p;
        q = _q;
        distance = pnt_distance( p, q );
    }
    void  init( const gdiam_point  _p, 
                const gdiam_point  _q,
                const gdiam_point  proj ) {
        p = _p;
        q = _q;
        distance = pnt_distance( p, q, proj );
    }

    void  init( const gdiam_point  pnt ) {
        distance = 0;
        p = q = pnt;
    }

    void  update_diam_simple( const gdiam_point  _p, const gdiam_point  _q ) {
        gdiam_real  new_dist;

        new_dist = pnt_distance( _p, _q );
        if  ( new_dist <= distance )
            return;
        
        //printf( "new_dist: %g\n", new_dist );
        distance = new_dist;
        p = _p;
        q = _q;
    }
    void  update_diam_simple( const gdiam_point  _p, const gdiam_point  _q,
                              const gdiam_point  dir ) {
        gdiam_real  new_dist;

        new_dist = pnt_distance( _p, _q, dir );
        if  ( new_dist <= distance )
            return;
        
        distance = new_dist;
        p = _p;
        q = _q;
    }

    void  update_diam( const gdiam_point  _p, const gdiam_point  _q ) { 
        //update_diam_simple( p, _p );
        //update_diam_simple( p, _q );
        //update_diam_simple( q, _p );
        //update_diam_simple( q, _q );
        update_diam_simple( _p, _q );
    }
    void  update_diam( const gdiam_point  _p, const gdiam_point  _q,
                       const gdiam_point  dir ) { 
        //update_diam_simple( p, _p );
        //update_diam_simple( p, _q );
        //update_diam_simple( q, _p );
        //update_diam_simple( q, _q );
        update_diam_simple( _p, _q, dir );
    }


    void  update_diam( GPointPair  & pp ) {
        //update_diam_simple( p, pp.p );
        //update_diam_simple( p, pp.q );
        //update_diam_simple( q, pp.p );
        //update_diam_simple( q, pp.q );
        update_diam_simple( pp.p, pp.q );
    }
};




class   GBBox {
private:
    gdiam_real  min_coords[ GDIAM_DIM ];
    gdiam_real  max_coords[ GDIAM_DIM ];

public:
    void  init( const GBBox   & a,
                const GBBox   & b ) {
        for  (int  ind = 0; ind < GDIAM_DIM; ind++ ) { 
            min_coords[ ind ] = min( a.min_coords[ ind ],
                                     b.min_coords[ ind ] );
            max_coords[ ind ] = max( a.max_coords[ ind ],
                                     b.max_coords[ ind ] );
        }
    }
    void  dump() const {
        gdiam_real  prod, diff;

        prod = 1.0;
        printf( "__________________________________________\n" );
        for  (int  ind = 0; ind < GDIAM_DIM; ind++ ) { 
            printf( "%d: [%g...%g]\n", ind, min_coords[ ind ],
                    max_coords[ ind ] );
            diff = max_coords[ ind ] - min_coords[ ind ];
            prod *= diff;
        }
        printf( "volume = %g\n", prod );
        printf( "\\__________________________________________\n" );
    }

    void  center( gdiam_point  out ) const {
        for  (int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            out[ ind ] = ( min_coords[ ind ] + max_coords[ ind ] ) / 2.0;
        }
    }

    gdiam_real  volume() const {
        gdiam_real  prod, val;

        prod = 1;        
        for  ( int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            val = length_dim( ind );
            prod *= val;
        }

        return  prod;
    }
    void  init() { 
        for  (int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            min_coords[ ind ] = 1e20;
            max_coords[ ind ] = -1e20;
        }
    }
    /*
    void  dump() const {
        printf( "---(" );
        for  (int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            printf( "[%g, %g] ",
                    min_coords[ ind ],
                    max_coords[ ind ] );
        }
        printf( ")\n" );
        }*/

    const gdiam_real  & min_coord( int  coord ) const {
        return  min_coords[ coord ];
    }
    const gdiam_real  & max_coord( int  coord ) const {
        return  max_coords[ coord ];
    }
    
    void  bound( const gdiam_point   pnt ) {
        //cout << "bounding: " << pnt << "\n";
        for  ( int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            if  ( pnt[ ind ] < min_coords[ ind ] )
                min_coords[ ind ] = pnt[ ind ];
            if  ( pnt[ ind ] > max_coords[ ind ] )
                max_coords[ ind ] = pnt[ ind ];
        }
    }

    gdiam_real  length_dim( int  dim )  const {
        return  max_coords[ dim ] - min_coords[ dim ];
    }
    int  getLongestDim() const {
        int  dim = 0;
        gdiam_real  len = length_dim( 0 );

        for  ( int  ind = 1; ind < GDIAM_DIM; ind++ ) 
            if  ( length_dim( ind ) > len ) {
                len = length_dim( ind );
                dim = ind;
            }

        return  dim;
    }
    gdiam_real  getLongestEdge() const {
        return  length_dim( getLongestDim() );
    }

    gdiam_real   get_diam() const
    {
        gdiam_real  sum, val;

        sum = 0;        
        for  ( int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            val = length_dim( ind );
            sum += val * val;
        }

        return  sqrt( sum );
    }

    // in the following we assume that the length of dir is ONE!!!
    // Note that the following is an overestaime - the diameter of
    // projection of a cube, is bounded by the length of projections
    // of its edges....
    gdiam_real   get_diam_proj( gdiam_point  dir ) const
    {
        gdiam_real  sum, coord;
        gdiam_real  prod, val;
        
        //printf( "get_diam_proj: " );
        sum = 0;
        for  ( int  ind = 0; ind < GDIAM_DIM; ind++ ) {
            coord = length_dim( ind );
            //printf( "coord[%d]: %g\n",ind, coord );
            prod = coord * dir [ ind ];
            val = coord * coord - prod * prod;
            assert( val >= 0 );
            sum += sqrt( val );
        }
        //printf( "sum: %g, %g\n", sum, get_diam() );
        
        // sum = squard diameter of the bounding box
        // prod = length of projection of the diameter of cube on the
        //      direction.
        
        return  get_diam();
    }

    gdiam_real  get_min_coord( int  dim ) const {
        return  min_coords[ dim ];
    }
    gdiam_real  get_max_coord( int  dim ) const {
        return  max_coords[ dim ];
    }
};


// gdiam_bbox is the famous - arbitrary orieted bounding box
class  gdiam_bbox
{
private:
    gdiam_point_t  dir_1, dir_2, dir_3;
    
    gdiam_real  low_1, high_1;
    gdiam_real  low_2, high_2;
    gdiam_real  low_3, high_3;
    bool  f_init;

public:
    gdiam_bbox() { 
        f_init = false;
    }

    gdiam_real  volume() const {
        gdiam_real  len1, len2, len3;

        len1 = ( high_1 - low_1 );
        len2 = ( high_2 - low_2 );
        len3 = ( high_3 - low_3 );

        return  len1 * len2 * len3;
    }


    void  set_third_dim_longest() {
        gdiam_point_t  pnt_tmp;

        if  ( ( high_1 - low_1 ) > ( high_3 - low_3 ) ) {
            gdiam_exchange( high_1, high_3 );
            gdiam_exchange( low_1, low_3 );

            memcpy( pnt_tmp, dir_1, sizeof( dir_1 ) );
            //pnt_tmp = dir_1;
            memcpy( dir_1, dir_3, sizeof( dir_3) );
            //dir_1 = dir_3;
            memcpy( dir_3, pnt_tmp, sizeof( dir_3 ) );
            //dir_3 = pnt_tmp;
        }
        if  ( ( high_2 - low_2 ) > ( high_3 - low_3 ) ) {
            gdiam_exchange( high_2, high_3 );
            gdiam_exchange( low_2, low_3 );

            pnt_copy( pnt_tmp, dir_2 );
            pnt_copy( dir_2, dir_3 );
            pnt_copy( dir_3, pnt_tmp );
        }
    }

    gdiam_point  get_dir( int  ind ) {
        if  ( ind == 0 )
            return  dir_1;
        if  ( ind == 1 )
            return  dir_2;
        if  ( ind == 2 )
            return  dir_3;
     
        assert( false );

        return  NULL;
    }
    
    void  combine( gdiam_point  out,
                   double  a_coef, double  b_coef,
                   double  c_coef ) {
        for  ( int  ind = 0; ind < GDIAM_DIM; ind++ ) 
            out[ ind ] = a_coef * dir_1[ ind ] 
                + b_coef * dir_2[ ind ] 
                + c_coef * dir_3[ ind ];
    }

    void  get_vertex( double sel1, double sel2, double sel3, double *p_x, double *p_y, double *p_z ) {
        gdiam_real  coef1, coef2, coef3;
        gdiam_point_t  pnt;

        coef1 = low_1 + sel1 * ( high_1 - low_1 );
        coef2 = low_2 + sel2 * ( high_2 - low_2 );
        coef3 = low_3 + sel3 * ( high_3 - low_3 );

        pnt_zero( pnt );
        pnt_scale_and_add( pnt, coef1, dir_1 );
        pnt_scale_and_add( pnt, coef2, dir_2 );
        pnt_scale_and_add( pnt, coef3, dir_3 );

	(*p_x) = pnt[0];
	(*p_y) = pnt[1];
	(*p_z) = pnt[2];

    }


    void  dump_vertex( double  sel1, double  sel2, double  sel3 ) const {
        gdiam_real  coef1, coef2, coef3;

        //printf( "selection: (%g, %g, %g)\n", sel1, sel2, sel3 );
        coef1 = low_1 + sel1 * ( high_1 - low_1 );
        coef2 = low_2 + sel2 * ( high_2 - low_2 );
        coef3 = low_3 + sel3 * ( high_3 - low_3 );

        //printf( "coeficients: (%g, %g,  %g)\n", 
        //        coef1, coef2, coef3 );

        gdiam_point_t  pnt;
        pnt_zero( pnt );
        
        //printf( "starting...\n" );
        //pnt_dump( pnt );
        pnt_scale_and_add( pnt, coef1, dir_1 );
        //pnt_dump( pnt );
        pnt_scale_and_add( pnt, coef2, dir_2 );
        //pnt_dump( pnt );
        pnt_scale_and_add( pnt, coef3, dir_3 );
        //pnt_dump( pnt );

        pnt_dump( pnt );
    }

    void  dump() const {
        printf( "-----------------------------------------------\n" );
        dump_vertex( 0, 0, 0 );
        dump_vertex( 0, 0, 1 );
        dump_vertex( 0, 1, 0 );
        dump_vertex( 0, 1, 1 );
        dump_vertex( 1, 0, 0 );
        dump_vertex( 1, 0, 1 );
        dump_vertex( 1, 1, 0 );
        dump_vertex( 1, 1, 1 );
        printf( "volume: %g\n", volume() );
        printf( "Directions:\n" );
        pnt_dump( dir_1 );
        pnt_dump( dir_2 );
        pnt_dump( dir_3 );
        printf( "prods: %g, %g, %g\n",
                pnt_dot_prod( dir_1, dir_2 ),
                pnt_dot_prod( dir_1, dir_3 ),
                pnt_dot_prod( dir_2, dir_3 ) );

        printf( "--------------------------------------------------\n" );
        //printf( "range_1: %g... %g\n", low_1, high_1 );
        //printf( "range_2: %g... %g\n", low_2, high_2 );
        //printf( "range_3: %g... %g\n", low_3, high_3 );
        //printf( "prd: %g\n", pnt_dot_prod( dir_1, dir_2 ) );
        //printf( "prd: %g\n", pnt_dot_prod( dir_2, dir_3 ) );
        //printf( "prd: %g\n", pnt_dot_prod( dir_1, dir_3 ) );
    }


    void  init( const GBBox  & bb ) {
        pnt_init( dir_1, 1, 0, 0 );
        pnt_init( dir_2, 0, 1, 0 );
        pnt_init( dir_3, 0, 0, 1 );
    
        low_1 = bb.min_coord( 0 );
        high_1 = bb.max_coord( 0 );

        low_2 = bb.min_coord( 1 );
        high_2 = bb.max_coord( 1 );

        low_3 = bb.min_coord( 2 );
        high_3 = bb.max_coord( 2 );
        f_init = true;
    }

    void  init( const gdiam_point  _dir_1,
                const gdiam_point  _dir_2,
                const gdiam_point  _dir_3 ) {
        memset( this, 0, sizeof( gdiam_bbox ) );

        pnt_copy( dir_1, _dir_1 );
        pnt_copy( dir_2, _dir_2 );
        pnt_copy( dir_3, _dir_3 );
        pnt_normalize( dir_1 );
        pnt_normalize( dir_2 );
        pnt_normalize( dir_3 );

        if  ( ( ! (fabs( pnt_dot_prod( dir_1, dir_2 ) ) < 1e-6 ) )
              ||  ( ! (fabs( pnt_dot_prod( dir_1, dir_3 ) ) < 1e-6 ) )
              ||  ( ! (fabs( pnt_dot_prod( dir_2, dir_3 ) ) < 1e-6 ) ) ) {
            printf( "should be all close to zero: %g, %g, %g\n",
                    pnt_dot_prod( dir_1, dir_2 ),
                    pnt_dot_prod( dir_1, dir_3 ),
                    pnt_dot_prod( dir_2, dir_3 ) );
            pnt_dump( _dir_1 );
            pnt_dump( _dir_2 );
            pnt_dump( _dir_3 );
            fflush( stdout );
            fflush( stderr );
            assert( fabs( pnt_dot_prod( dir_1, dir_2 ) ) < 1e-6 );
            assert( fabs( pnt_dot_prod( dir_1, dir_3 ) ) < 1e-6 );
            assert( fabs( pnt_dot_prod( dir_2, dir_3 ) ) < 1e-6 );
        }

        // The following reduce the error by slightly improve the
        // orthoginality of the third vector. Doing to the second
        // vector is not efficient...
        pnt_scale_and_add( dir_3, -pnt_dot_prod( dir_3, dir_1 ),
                           dir_1 );
        pnt_scale_and_add( dir_3, -pnt_dot_prod( dir_3, dir_2 ),
                           dir_2 );
        pnt_normalize( dir_3 );
        //printf( "__should be all close to zero: %g, %g, %g\n",
        //         pnt_dot_prod( dir_1, dir_2 ),
        //        pnt_dot_prod( dir_1, dir_3 ),
        //        pnt_dot_prod( dir_2, dir_3 ) );        
    }

    void  bound( const gdiam_point  pnt ) {
        gdiam_real  prod_1, prod_2, prod_3;

        prod_1 = pnt_dot_prod( dir_1, pnt );
        prod_2 = pnt_dot_prod( dir_2, pnt );
        prod_3 = pnt_dot_prod( dir_3, pnt );

        if  ( ! f_init ) {
            f_init = true;
            low_1 = high_1 = prod_1;
            low_2 = high_2 = prod_2;
            low_3 = high_3 = prod_3;
            return;
        }
        if  ( prod_1 < low_1 )
            low_1 = prod_1;
        if  ( prod_2 < low_2 )
            low_2 = prod_2;
        if  ( prod_3 < low_3 )
            low_3 = prod_3;

        if  ( prod_1 > high_1 )
            high_1 = prod_1;
        if  ( prod_2 > high_2 )
            high_2 = prod_2;
        if  ( prod_3 > high_3 )
            high_3 = prod_3;
    }

    // compute the coordinates as if the bounding box is unit cube of
    // the grid
    void  get_normalized_coordinates( gdiam_point  in,
                                      gdiam_point  out ) {
        out[ 0 ] = ( pnt_dot_prod( dir_1, in ) - low_1 ) / ( high_1 - low_1 );
        out[ 1 ] = ( pnt_dot_prod( dir_2, in ) - low_2 ) / ( high_2 - low_2 );
        out[ 2 ] = ( pnt_dot_prod( dir_3, in ) - low_3 ) / ( high_3 - low_3 );
    }

};

GDIAM_EXPORT GPointPair   gdiam_approx_diam( gdiam_point  * start, int  size,
                                    gdiam_real  eps );
GDIAM_EXPORT GPointPair   gdiam_approx_diam( gdiam_point  * start, int  size,
                                gdiam_real  eps );
GDIAM_EXPORT gdiam_real   gdiam_approx_diam( gdiam_real  * start, int  size,
                                gdiam_real  eps );
GDIAM_EXPORT GPointPair   gdiam_approx_diam_pair( gdiam_real  * start, int  size,
                                     gdiam_real  eps );
GDIAM_EXPORT GPointPair   gdiam_approx_diam_pair_UDM( gdiam_real  * start, int  size,
                                         gdiam_real  eps );
GDIAM_EXPORT gdiam_bbox   gdiam_approx_const_mvbb( gdiam_point  * start, int  size,
                                      gdiam_real  eps, 
                                      GBBox  * p_ap_bbox );    
GDIAM_EXPORT gdiam_point  * gdiam_convert( gdiam_real  * start, int  size );
GDIAM_EXPORT gdiam_bbox   gdiam_approx_mvbb( gdiam_point  * start, int  size,
                                gdiam_real  eps ) ;
GDIAM_EXPORT gdiam_bbox   gdiam_approx_mvbb_grid( gdiam_point  * start, int  size,
                                     int  grid_size );
GDIAM_EXPORT gdiam_bbox   gdiam_approx_mvbb_grid_sample( gdiam_point  * start, int  size,
                                            int  grid_size, int  sample_size );
GDIAM_EXPORT gdiam_bbox   gdiam_approx_mvbb_grid_sample( gdiam_real  * start, int  size,
                                            int  grid_size, int  sample_size );


GDIAM_EXPORT void  gdiam_generate_orthonormal_base( gdiam_point  in,
                                       gdiam_point  out1,
                                       gdiam_point  out2 );

#else   /* __GDIAM__H */
#error  Header file gdiam.h included twice
#endif  /* __GDIAM__H */

/* gdiam.h - End of File ------------------------------------------*/
