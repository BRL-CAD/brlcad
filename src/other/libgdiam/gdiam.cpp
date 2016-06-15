/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * gdiam.cpp -
 *   Computes diameter, and a tight-fitting bounding box of a
 *   point-set in 3d.
 *
 * Copyright 2001 Sariel Har-Peled (ssaarriieell@cs.uiuc.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of either:
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
 * Code is based on the paper:
 *   A Practical Approach for Computing the Diameter of a
 *        Point-Set (in ACM Sym. on Computation Geometry
 *                   SoCG 2001)
 *   Sariel Har-Peled (http://www.uiuc.edu/~sariel)
 *--------------------------------------------------------------
 * History
 * 3/28/01 -
 *     This is a more robust version of the code. It should
 *     handlereally abnoxious inputs well (i.e., points with equal
 *     coordinates, etc.
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include  <memory.h>
#include  <math.h>

#include  <iostream>
#include  <vector>
#include  <algorithm>

#include  "gdiam.hpp"

/*--- Constants ---*/

#define  HEAP_LIMIT  10000
#define  HEAP_DELTA  10000
#define  PI 3.14159265958979323846

/*--- Start of Code ---*/

typedef long double  ldouble;

class GFSPPair;

class   GFSPTreeNode {
private:
    GBBox    bbox;
    gdiam_point   * p_pnt_left, * p_pnt_right;
    gdiam_real  bbox_diam, bbox_diam_proj;
    GFSPTreeNode  * left, * right;
    gdiam_point_t  center;

public:
    void  dump() const {
        printf( "dump...\n" );
    }
    int  size() const {
        return  (int)( p_pnt_right - p_pnt_left );
    }
    const gdiam_point  getCenter() const {
        return   (gdiam_point)center;
    }
    int  nodes_number() const {
        int  sum;

        if  ( ( left == NULL )  &&  ( right == NULL ) )
            return  1;
        sum = 1;

        if  ( left != NULL )
            sum += left->nodes_number();
        if  ( right != NULL )
            sum += right->nodes_number();

        return  sum;
    }

    GFSPTreeNode( gdiam_point   * _p_pnt_left,
                  gdiam_point   * _p_pnt_right ) {
        bbox.init();
        left = right = NULL;
        p_pnt_left = _p_pnt_left;
        p_pnt_right = _p_pnt_right;
        bbox_diam = bbox_diam_proj = -1;
    }

    gdiam_real  maxDiam() const {
        return  bbox_diam;
    }
    gdiam_real  maxDiamProj() const {
        return  bbox_diam_proj;
    }

    GFSPTreeNode  * get_left() {
        return  left;
    }
    GFSPTreeNode  * get_right() {
        return   right;
    }

    bool  isSplitable() const {
        return  (size() > 1);
    }

    const gdiam_point   * ptr_pnt_left() {
        return  p_pnt_left;
    }
    const gdiam_point   * ptr_pnt_rand() {
        return  p_pnt_left + ( rand() % ( p_pnt_right - p_pnt_left + 1 ) );
    }
    const gdiam_point   * ptr_pnt_right() {
        return  p_pnt_right;
    }


    friend class  GFSPTree;

    const GBBox  & getBBox() const {
        return  bbox;
    }
};


class  GFSPTree {
private:
    gdiam_point  * arr;
    GFSPTreeNode  * root;

public:
    void  init( const gdiam_point  * _arr,
                int  size ) {
        arr = (gdiam_point *)malloc( sizeof( gdiam_point ) * size );
        assert( arr != NULL );

        for  ( int  ind = 0; ind < size; ind++ ) {
            arr[ ind ] = _arr[ ind ];
        }

        root = build_node( arr, arr + size - 1 );
    }

    static void  terminate( GFSPTreeNode  * node ) {
        if  ( node == NULL )
            return;
        terminate( node->left );
        terminate( node->right );

        node->left = node->right = NULL;

        delete node;
    }
    void  term() {
        free( arr );
        arr = NULL;

        GFSPTree::terminate( root );
        root = NULL;
    }

    const gdiam_point   * getPoints() const {
        return  arr;
    }

    int  nodes_number() const {
        return  root->nodes_number();
    }

    gdiam_real   brute_diameter( int  a_lo, int  a_hi,
                                 int  b_lo, int  b_hi,
                                 GPointPair  & diam ) const {
        double  max_dist;

        max_dist = 0;
        for  ( int  ind = a_lo; ind <= a_hi; ind++ )
            for  ( int  jnd = b_lo; jnd <= b_hi; jnd++ )
                diam.update_diam( arr[ ind ], arr[ jnd ] );

        return  diam.distance;
    }
    gdiam_real   brute_diameter( int  a_lo, int  a_hi,
                                 int  b_lo, int  b_hi,
                                 GPointPair  & diam,
                                 const gdiam_point  dir ) const {
        double  max_dist;

        max_dist = 0;
        for  ( int  ind = a_lo; ind <= a_hi; ind++ )
            for  ( int  jnd = b_lo; jnd <= b_hi; jnd++ )
                diam.update_diam( arr[ ind ], arr[ jnd ], dir );

        return  diam.distance;
    }
    gdiam_real   brute_diameter( const gdiam_point  * a_lo,
                           const gdiam_point  * a_hi,
                           const gdiam_point  * b_lo,
                           const gdiam_point  * b_hi,
                           GPointPair  & diam ) const {
        double  max_dist;

        max_dist = 0;
        for  ( const gdiam_point  * ind = a_lo; ind <= a_hi; ind++ )
            for  ( const gdiam_point   * jnd = b_lo; jnd <= b_hi; jnd++ )
                diam.update_diam( *ind, *jnd );

        return  diam.distance;
    }
    gdiam_real   brute_diameter( const gdiam_point  * a_lo,
                                 const gdiam_point  * a_hi,
                                 const gdiam_point  * b_lo,
                                 const gdiam_point  * b_hi,
                                 GPointPair  & diam,
                                 const gdiam_point  dir ) const {
        double  max_dist;

        max_dist = 0;
        for  ( const gdiam_point  * ind = a_lo; ind <= a_hi; ind++ )
            for  ( const gdiam_point   * jnd = b_lo; jnd <= b_hi; jnd++ )
                diam.update_diam( *ind, *jnd, dir );

        return  diam.distance;
    }

    const gdiam_point  getPoint( int  ind ) const {
        return  arr[ ind ];
    }

    GFSPTreeNode  * getRoot() {
        return  root;
    }

    GFSPTreeNode  * build_node( gdiam_point  * left,
                                gdiam_point  * right ) {
        if  ( left > right ) {
            printf( "what!?\n" );
            fflush( stdout );
            assert( left <= right );
        }
        while  ( ( right > left )
                 &&  ( pnt_isEqual( *right, *left ) ) )
            right--;
        GFSPTreeNode  * node = new  GFSPTreeNode( left, right );

        node->bbox.init();
        for  ( const gdiam_point  * ind = left; ind <= right; ind++ )
            node->bbox.bound( *ind );
        node->bbox_diam = node->bbox.get_diam();

        node->bbox.center( node->center );

        return  node;
    }

    // return how many elements on left side.
    int    split_array( gdiam_point   * left,
                        gdiam_point   * right,
                        int  dim, const gdiam_real  & val ) {
        const gdiam_point   * start_left = left;

        while  ( left < right ) {
            if  ( (*left)[ dim ] < val )
                left++;
            else
                if  ( (*right)[ dim ] >= val )
                    right--;
                else {
                    gdiam_exchange( *right, *left );
                }
        }

        return  left - start_left;
    }


    void   split_node( GFSPTreeNode  * node )
    {
        int  dim, left_size;
        gdiam_real  split_val;

        if  ( node->left != NULL )
            return;

        GBBox  & bb( node->bbox );

        dim = bb.getLongestDim();
        split_val = ( bb.min_coord( dim ) + bb.max_coord( dim ) ) / 2.0;

        left_size = split_array( node->p_pnt_left, node->p_pnt_right,
                              dim, split_val );
        if  ( left_size <= 0.0 ) {
            printf( "bb: %g   %g\n",
                    bb.min_coord( dim ), bb.max_coord( dim ) );
            printf( "left: %p, right: %p\n",
                    node->p_pnt_left, node->p_pnt_right );
            assert( left_size > 0 );
        }
        if  ( left_size >= (node->p_pnt_right - node->p_pnt_left + 1 ) ) {
            printf( "left size too large?\n" );
            fflush( stdout );
            assert( left_size < (node->p_pnt_right - node->p_pnt_left + 1 ) );
        }

        node->left = build_node( node->p_pnt_left,
                                 node->p_pnt_left + left_size - 1 );
        node->right = build_node( node->p_pnt_left + left_size,
                                  node->p_pnt_right );
    }

    void  findExtremeInProjection( GFSPPair  & pair,
                                   GPointPair  & max_pair_diam );
};

void  bbox_vertex( const GBBox  & bb, gdiam_point_t  & ver,
                   int  vert ) {
    ver[ 0 ] = ((vert & 0x1) != 0)?
        bb.min_coord( 0 ) : bb.max_coord( 0 );
    ver[ 1 ] = ((vert & 0x2) != 0)?
        bb.min_coord( 1 ) : bb.max_coord( 1 );
    ver[ 2 ] = ((vert & 0x4) != 0)?
        bb.min_coord( 2 ) : bb.max_coord( 2 );
}


gdiam_real  bbox_proj_dist( const GBBox  & bb1,
                            const GBBox  & bb2,
                            gdiam_point_cnt  proj )
{
    gdiam_point_t  a, b;
    gdiam_real  rl;

    rl = 0;
    for  ( int  ind = 0; ind < 8; ind++ ) {
        bbox_vertex( bb1, a, ind );

        for  ( int  jnd = 0; jnd < 8; jnd++ ) {
            bbox_vertex( bb2, b, jnd );
            rl = max( rl, pnt_distance( a, b, proj ) );
        }
    }

    return  rl;
}



class  GFSPPair
{
private:
    GFSPTreeNode  * left, * right;
    double  max_diam;

public:
    void  init( GFSPTreeNode  * _left, GFSPTreeNode  * _right )
    {
        left = _left;
        right = _right;

        GBBox  bb;

        bb.init( left->getBBox(), right->getBBox() );
        max_diam = bb.get_diam();
    }

    void  init( GFSPTreeNode  * _left, GFSPTreeNode  * _right,
                gdiam_point  proj_dir, gdiam_real dist )
    {
        left = _left;
        right = _right;

        GBBox  bb;

        bb.init( left->getBBox(), right->getBBox() );
        max_diam = max( max( bbox_proj_dist( _left->getBBox(),
                                        _right->getBBox(),
                                        proj_dir ),
                             bbox_proj_dist( _left->getBBox(),
                                             _left->getBBox(),
                                             proj_dir ) ),
                        bbox_proj_dist( _right->getBBox(),
                                        _right->getBBox(),
                                        proj_dir ) );

    }

    // error 2r/l - approximate cos of the max possible angle in the pair
    double  getProjectionError() const
    {
        double  l, two_r;

        l = pnt_distance( left->getCenter(), right->getCenter() );
        two_r = max( left->maxDiam(), right->maxDiam() );

        if  ( l == 0.0 )
            return  10;

        return  two_r / l;
    }

    GFSPTreeNode  * get_left() {
        return  left;
        }
    GFSPTreeNode  * get_right() {
        return   right;
    }

    void  dump() const {
        printf( "[\n" );
        left->dump();
        right->dump();
        printf( "\tmax_diam; %g\n", max_diam );
    }

    bool  isBelowThreshold( int  threshold ) const {
        if  ( left->size() > threshold )
            return  false;
        if  ( right->size() > threshold )
            return  false;

        return  true;
    }

    double   directDiameter( const GFSPTree  & tree,
                             GPointPair  & diam ) const {
        return  tree.brute_diameter( left->ptr_pnt_left(),
                                     left->ptr_pnt_right(),
                                     right->ptr_pnt_left(),
                                     right->ptr_pnt_right(),
                                     diam );
    }

    double   directDiameter( const GFSPTree  & tree,
                             GPointPair  & diam,
                             const gdiam_point  dir ) const {
        return  tree.brute_diameter( left->ptr_pnt_left(),
                                     left->ptr_pnt_right(),
                                     right->ptr_pnt_left(),
                                     right->ptr_pnt_right(),
                                     diam, dir );
    }

    double  maxDiam()  const {
        return  max_diam;
    }
    double  minAprxDiam()  const {
        double   sub_diam;

        sub_diam = left->maxDiam() + right->maxDiam();
        if  ( sub_diam > (max_diam / 10.0) )
            return  max_diam;

        return  max_diam - sub_diam / 2.0;
    }
};


class g_heap_pairs_p;

#define  UDM_SIMPLE      1
#define  UDM_BIG_SAMPLE  2

class  GTreeDiamAlg
{
private:
    GFSPTree  tree;
    //double  diam;
    GPointPair   pair_diam;
    int  points_num;
    int  threshold_brute;
    int  update_diam_mode;
public:
    GTreeDiamAlg( gdiam_point  * arr, int  size, int  mode ) {
        tree.init( arr, size );
        //diam = 0;
        pair_diam.init( arr[ 0 ] );
        points_num = size;
        threshold_brute = 40;
        update_diam_mode = mode;
    }

    void  term() {
        tree.term();
    }

    int  size() const {
        return  points_num;
    }
    const GPointPair  & getDiameter() const {
        return  pair_diam;
    }

    int  nodes_number() const {
        return  tree.nodes_number();
    }

    void  addPairHeap( g_heap_pairs_p  & heap,
                       GFSPTreeNode  * left,
                       GFSPTreeNode  * right );
    void  addPairHeap( g_heap_pairs_p  & heap,
                       GFSPTreeNode  * left,
                       GFSPTreeNode  * right,
                       gdiam_point  proj,
                       GFSPPair  & father );
    void  split_pair( GFSPPair  & pair,
                      g_heap_pairs_p  & heap, double  eps );
    void  split_pair_proj( GFSPPair  & pair,
                           g_heap_pairs_p  & heap, double  eps,
                           gdiam_point  proj );
    void  compute_by_heap( double  eps );
    void  compute_by_heap_proj( double  eps, gdiam_point  proj );

    double  diameter() {
        return  pair_diam.distance;
    }
};


/*--- Heap implementation ---*/

typedef  int   ( *ptrCompareFunc )( void  * aPtr, void  * bPtr );
typedef  void  * voidPtr_t;

struct  heap_t
{
    voidPtr_t  * pArr;
    int  curr_size, max_size;
    ptrCompareFunc  pCompFunc;
};

void  heap_init( heap_t  * pHeap, ptrCompareFunc  _pCompFunc )
{
    assert( pHeap != NULL );
    assert( _pCompFunc != NULL );

    memset( pHeap, 0, sizeof( heap_t ) );

    pHeap->pCompFunc = _pCompFunc;
    pHeap->max_size = 100;
    pHeap->pArr = (voidPtr_t *)malloc( sizeof( void * )
                                       * pHeap->max_size );
    assert( pHeap->pArr != NULL );
    pHeap->curr_size = 0;
}


static void  resize( heap_t  * pHeap, int  size )
{
    int  max_sz;
    voidPtr_t  * pTmp;

    if   ( size <= pHeap->max_size )
        return;

    max_sz = size * 2;
    pTmp = (voidPtr_t *)malloc( max_sz * sizeof( void * ) );
    assert( pTmp != NULL );
    memset( pTmp, 0, max_sz * sizeof( void * ) );
    memcpy( pTmp, pHeap->pArr, pHeap->curr_size * sizeof( void * ) );
    free( pHeap->pArr );
    pHeap->pArr = pTmp;
}


void  heap_term( heap_t  * pHeap )
{
    assert( pHeap != NULL );

    if  ( pHeap->pArr != NULL )
        free( pHeap->pArr );
    memset( pHeap, 0, sizeof( heap_t ) );
}


void  heap_insert( heap_t  * pHeap, void  * pData )
{
    int  ind, father;
    voidPtr_t  pTmp;

    resize( pHeap, pHeap->curr_size + 1 );

    ind = pHeap->curr_size;
    pHeap->curr_size++;

    pHeap->pArr[ ind ] = pData;

    while  ( ind > 0 ) {
        father = ( ind - 1 ) / 2;

        if  ( (*pHeap->pCompFunc)( pHeap->pArr[ ind ],
                                   pHeap->pArr[ father ] ) <= 0 )
            break;

        pTmp = pHeap->pArr[ ind ];
        pHeap->pArr[ ind ] = pHeap->pArr[ father ];
        pHeap->pArr[ father ] = pTmp;

        ind = father;
    }
}


void  * heap_top( heap_t  * pHeap )
{
    return  pHeap->pArr[ 0 ];
}


void  * heap_delete_max( heap_t  * pHeap )
{
    void  * res;
    void  * pTmp;
    int  ind, left, right, max_ind;

    if  ( pHeap->curr_size <= 0 )
        return  NULL;

    res = pHeap->pArr[ 0 ];

    pHeap->curr_size--;
    pHeap->pArr[ 0 ] = pHeap->pArr[ pHeap->curr_size ];
    pHeap->pArr[ pHeap->curr_size ] = NULL;
    ind = 0;

    while  ( ind < pHeap->curr_size ) {
        left = 2 * ind + 1;
        right = 2 * ind + 2;
        if  ( left >= pHeap->curr_size )
            break;
        if  ( right >= pHeap->curr_size )
            right = left;

        if  ( (*pHeap->pCompFunc)( pHeap->pArr[ left ],
                                   pHeap->pArr[ right ] ) <= 0 )
            max_ind = right;
        else
            max_ind = left;
        if  ( (*pHeap->pCompFunc)( pHeap->pArr[ ind  ],
                                   pHeap->pArr[ max_ind ] ) >= 0 )
            break;

        pTmp = pHeap->pArr[ ind ];
        pHeap->pArr[ ind ] = pHeap->pArr[ max_ind ];
        pHeap->pArr[ max_ind ] = pTmp;

        ind = max_ind;
    }

    return  res;
}


bool  heap_is_empty( heap_t  * pHeap )
{
    assert( pHeap != NULL );

    return( pHeap->curr_size <= 0 );
}


int   compare_pairs( void  * _a, void  * _b )
{
    const GFSPPair  & a( *(GFSPPair *)_a );
    const GFSPPair  & b( *(GFSPPair *)_b );

    if  ( a.maxDiam() < b.maxDiam() )
        return  -1;
    if  ( a.maxDiam() > b.maxDiam() )
        return  1;

    return  0;
}



class  g_heap_pairs_p
{
private:
    heap_t  heap;

public:
    g_heap_pairs_p() {
        heap_init( &heap, compare_pairs );
    }

    ~g_heap_pairs_p() {
        while  ( size() > 0 ) {
            pop();
        }
        heap_term( &heap );
    }

    void  push( GFSPPair  & pair )
    {
        GFSPPair  * p_pair = new GFSPPair( pair );
        heap_insert( &heap, (void *)p_pair );
    }

    int  size() const {
        return  heap.curr_size;
    }
    GFSPPair  top() {
        return  *(GFSPPair *)heap_top( &heap );
    }
    void  pop() {
        GFSPPair  * ptr = (GFSPPair *)heap_delete_max( &heap );

        delete  ptr;
    }
};


/* heap.implementation end */


void  computeExtremePair( const gdiam_point  * arr, int  size,
                          int  dim, GPointPair  & pp )
{
    pp.init( arr[ 0 ] );

    for  ( int  ind = 1; ind < size; ind++ ) {
        const gdiam_point  pnt( arr[ ind ] );

        if  ( pnt[ dim ] < pp.p[ dim ] )
            pp.p = pnt;
        if  ( pnt[ dim ] > pp.q[ dim ] )
            pp.q = pnt;
    }

    pp.distance = pnt_distance( pp.p, pp.q );
}



void  GTreeDiamAlg::compute_by_heap( double  eps )
{
    g_heap_pairs_p  heap;
    int  heap_limit, heap_delta;

    heap_limit = HEAP_LIMIT;
    heap_delta = HEAP_DELTA;

    GFSPTreeNode  * root = tree.getRoot();

    computeExtremePair( tree.getPoints(), points_num,
                        root->getBBox().getLongestDim(),
                        pair_diam );

    GFSPPair  root_pair;

    root_pair.init( root, root );

    heap.push( root_pair );

    int  count =0;
    while  ( heap.size() > 0 ) {
        GFSPPair  pair = heap.top();
        heap.pop();
        split_pair( pair, heap, eps );
        if  ( ( count & 0x3ff ) == 0 ) {
            if  ( ((int)heap.size()) > heap_limit ) {
                threshold_brute *= 2;
                printf( "threshold_brute: %d\n", threshold_brute );
                heap_limit += heap_delta;
            }
        }
        count++;
    }
}


void  GTreeDiamAlg::compute_by_heap_proj( double  eps,
                                          gdiam_point  proj )
{
    g_heap_pairs_p  heap;
    int  heap_limit, heap_delta;
    GPointPair   pair_diam_x;

    heap_limit = HEAP_LIMIT;
    heap_delta = HEAP_DELTA;

    GFSPTreeNode  * root = tree.getRoot();

    computeExtremePair( tree.getPoints(), points_num,
                        root->getBBox().getLongestDim(),
                        pair_diam_x );
    pair_diam.init( pair_diam_x.p, pair_diam_x.q, proj );

    GFSPPair  root_pair;

    root_pair.init( root, root, proj, 0 );

    heap.push( root_pair );

    int  count =0;
    while  ( heap.size() > 0 ) {
        GFSPPair  pair = heap.top();
        heap.pop();
        split_pair_proj( pair, heap, eps, proj );
        if  ( ( count & 0x3ff ) == 0 ) {
            if  ( ((int)heap.size()) > heap_limit ) {
                threshold_brute *= 2;
                printf( "threshold_brute: %d\n", threshold_brute );
                heap_limit += heap_delta;
            } 
        }
        count++;
    }
}


void  GTreeDiamAlg::addPairHeap( g_heap_pairs_p  & heap,
                                 GFSPTreeNode  * left,
                                 GFSPTreeNode  * right )
{
    if  ( update_diam_mode == UDM_SIMPLE ) {
        const gdiam_point  p( *(left->ptr_pnt_left()) );
        const gdiam_point  q( *(right->ptr_pnt_left()) );

        pair_diam.update_diam( p, q );
    } else
        if  ( update_diam_mode == UDM_BIG_SAMPLE ) {
            if  ( ( left->size() < 100 )  ||  ( right->size() < 100 ) ) {
                const gdiam_point  p( *(left->ptr_pnt_left()) );
                const gdiam_point  q( *(right->ptr_pnt_left()) );

                pair_diam.update_diam( p, q );
                } else
            {
#define  UMD_SIZE        5
                gdiam_point  arr_left[ UMD_SIZE ],
                    arr_right[ UMD_SIZE ];
                for  ( int  ind = 0; ind < UMD_SIZE; ind++ ) {
                    const gdiam_point  p( *(left->ptr_pnt_rand()) );
                    const gdiam_point  q( *(right->ptr_pnt_rand()) );
                    arr_left[ ind ] = p;
                    arr_right[ ind ] = q;
                }
                for  ( int  ind = 0; ind < UMD_SIZE; ind++ )
                    for  ( int  jnd = 0; jnd < UMD_SIZE; jnd++ )
                        pair_diam.update_diam( arr_left[ ind ],
                                               arr_right[ jnd ] );
            }
        } else {
            assert( false );
        }

    GFSPPair  pair;

    pair.init( left, right );
    if  ( pair.maxDiam() <= pair_diam.distance )
        return;
    heap.push( pair );
}


void  GTreeDiamAlg::addPairHeap( g_heap_pairs_p  & heap,
                                 GFSPTreeNode  * left,
                                 GFSPTreeNode  * right,
                                 gdiam_point  proj,
                                 GFSPPair  & father )
{
    const gdiam_point  p( *(left->ptr_pnt_left()) );
    const gdiam_point  q( *(right->ptr_pnt_left()) );

    pair_diam.update_diam( p, q, proj );

    GFSPPair  pair;

    pair.init( left, right, proj,
               pnt_distance( p, q, proj ) );
    if  ( pair.maxDiam() <= pair_diam.distance )
        return;
    heap.push( pair );
}


void  GTreeDiamAlg::split_pair_proj( GFSPPair  & pair,
                                     g_heap_pairs_p  & heap, double  eps,
                                     gdiam_point  proj )
{
    bool  f_is_left_splitable, f_is_right_splitable;

    if  ( pair.maxDiam() <= ((1.0 + eps) * pair_diam.distance ))
        return;

    if  ( pair.isBelowThreshold( threshold_brute ) ) {
        pair.directDiameter( tree, pair_diam, proj );
        return;
    }

    f_is_left_splitable = pair.get_left()->isSplitable();
    f_is_right_splitable = pair.get_right()->isSplitable();
    assert( f_is_left_splitable  ||  f_is_right_splitable );
    if ( f_is_left_splitable )
        tree.split_node( pair.get_left() );
    if ( f_is_right_splitable )
        tree.split_node( pair.get_right() );

    if  ( f_is_left_splitable
          &&  f_is_right_splitable ) {
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right()->get_left(),
                     proj, pair );
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right()->get_right(),
                     proj, pair );
        // to avoid exponential blowup
        if  ( pair.get_left() != pair.get_right() )
            addPairHeap( heap,
                         pair.get_left()->get_right(),
                         pair.get_right()->get_left(),
                         proj, pair );
        addPairHeap( heap,
                     pair.get_left()->get_right(),
                     pair.get_right()->get_right(),
                     proj, pair );
        return;
    }
    if  ( f_is_left_splitable ) {
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right(), proj, pair );
        addPairHeap( heap,
                     pair.get_left()->get_right(),
                     pair.get_right(), proj, pair );
        return;
    }
    if  ( f_is_right_splitable ) {
        addPairHeap( heap,
                     pair.get_left(),
                     pair.get_right()->get_left(), proj, pair );
        addPairHeap( heap,
                     pair.get_left(),
                     pair.get_right()->get_right(), proj, pair );
        return;
    }
}


void  GTreeDiamAlg::split_pair( GFSPPair  & pair,
                                g_heap_pairs_p  & heap,
                                double  eps )
{
    bool  f_is_left_splitable, f_is_right_splitable;

    if  ( pair.maxDiam() <= ((1.0 + eps) * pair_diam.distance ))
        return;

    if  ( pair.isBelowThreshold( threshold_brute ) ) {
        pair.directDiameter( tree, pair_diam );
        return;
    }

    f_is_left_splitable = pair.get_left()->isSplitable();
    f_is_right_splitable = pair.get_right()->isSplitable();
    assert( f_is_left_splitable  ||  f_is_right_splitable );
    if ( f_is_left_splitable )
        tree.split_node( pair.get_left() );
    if ( f_is_right_splitable )
        tree.split_node( pair.get_right() );

    if  ( f_is_left_splitable
          &&  f_is_right_splitable ) {
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right()->get_left() );
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right()->get_right() );
        // to avoid exponential blowup
        if  ( pair.get_left() != pair.get_right() )
            addPairHeap( heap,
                         pair.get_left()->get_right(),
                         pair.get_right()->get_left() );
        addPairHeap( heap,
                     pair.get_left()->get_right(),
                     pair.get_right()->get_right() );
        return;
    }
    if  ( f_is_left_splitable ) {
        addPairHeap( heap,
                     pair.get_left()->get_left(),
                     pair.get_right() );
        addPairHeap( heap,
                     pair.get_left()->get_right(),
                     pair.get_right() );
        return;
    }
    if  ( f_is_right_splitable ) {
        addPairHeap( heap,
                     pair.get_left(),
                     pair.get_right()->get_left() );
        addPairHeap( heap,
                     pair.get_left(),
                     pair.get_right()->get_right() );
        return;
    }
}


GPointPair   gdiam_approx_diam( gdiam_point  * start, int  size,
                                gdiam_real  eps )
{
    GPointPair  pair;

    GTreeDiamAlg  * pAlg = new  GTreeDiamAlg( start, size, UDM_SIMPLE );
    pAlg->compute_by_heap( eps );

    pair = pAlg->getDiameter();

    delete  pAlg;

    return  pair;
}


GPointPair   gdiam_approx_diam_UDM( gdiam_point  * start, int  size,
                                    gdiam_real  eps )
{
    GPointPair  pair;

    GTreeDiamAlg  * pAlg = new  GTreeDiamAlg( start, size, UDM_BIG_SAMPLE );
    pAlg->compute_by_heap( eps );

    pair = pAlg->getDiameter();

    delete  pAlg;

    return  pair;
}


gdiam_point  * gdiam_convert( gdiam_real  * start, int  size )
{
    gdiam_point  * p_arr;

    assert( start != NULL );
    assert( size > 0 );

    p_arr = (gdiam_point *)malloc( sizeof( gdiam_point ) * size );
    assert( p_arr != NULL );

    for  ( int  ind = 0; ind < size; ind++ )
        p_arr[ ind ] = (gdiam_point)&(start[ ind * 3 ]);

    return  p_arr;
}


GPointPair   gdiam_approx_diam_pair( gdiam_real  * start, int  size,
                                     gdiam_real  eps )
{
    gdiam_point  * p_arr;
    GPointPair  pair;

    p_arr = gdiam_convert( start, size );
    pair = gdiam_approx_diam( p_arr, size, eps );
    free( p_arr );

    return  pair;
}


GPointPair   gdiam_approx_diam_pair_UDM( gdiam_real  * start, int  size,
                                         gdiam_real  eps )
{
    gdiam_point  * p_arr;
    GPointPair  pair;

    p_arr = gdiam_convert( start, size );
    pair = gdiam_approx_diam_UDM( p_arr, size, eps );
    free( p_arr );

    return  pair;
}


gdiam_bbox   gdiam_approx_const_mvbb( gdiam_point  * start, int  size,
                                      gdiam_real  eps,
                                      GBBox  * p_ap_bbox )
{
    GPointPair  pair, pair_2;

    GTreeDiamAlg  * pAlg = new  GTreeDiamAlg( start, size, UDM_SIMPLE );
    pAlg->compute_by_heap( eps );

    pair = pAlg->getDiameter();

    /* Compute the resulting diameter */
    gdiam_point_t  dir, dir_2, dir_3;

    dir[ 0 ] = pair.p[ 0 ] - pair.q[ 0 ];
    dir[ 1 ] = pair.p[ 1 ] - pair.q[ 1 ];
    dir[ 2 ] = pair.p[ 2 ] - pair.q[ 2 ];
    pnt_normalize( dir );

    pAlg->compute_by_heap_proj( eps, dir );

    pair_2 = pAlg->getDiameter();
    dir_2[ 0 ] = pair_2.p[ 0 ] - pair_2.q[ 0 ];
    dir_2[ 1 ] = pair_2.p[ 1 ] - pair_2.q[ 1 ];
    dir_2[ 2 ] = pair_2.p[ 2 ] - pair_2.q[ 2 ];

    gdiam_real prd;

    prd = pnt_dot_prod( dir, dir_2 );
    dir_2[ 0 ] = dir_2[ 0 ] - prd * dir[ 0 ];
    dir_2[ 1 ] = dir_2[ 1 ] - prd * dir[ 1 ];
    dir_2[ 2 ] = dir_2[ 2 ] - prd * dir[ 2 ];

    pnt_normalize( dir );
    pnt_normalize( dir_2 );

    pnt_cross_prod( dir, dir_2, dir_3 );
    pnt_normalize( dir_3 );

    gdiam_bbox  bbox;
    GBBox  ap_bbox;

    if  ( ( pnt_length( dir_2 ) < 1e-6 )
          &&  ( pnt_length( dir_3 ) < 1e-6 ) )
        gdiam_generate_orthonormal_base( dir, dir_2, dir_3 );

    if  ( ( pnt_length( dir ) == 0.0 )
          ||  ( pnt_length( dir_2 ) < 1e-6 )
          ||  ( pnt_length( dir_3 ) < 1e-6 ) ) {
        gdiam_generate_orthonormal_base( dir, dir_2, dir_3 );
        pnt_dump( dir );
        pnt_dump( dir_2 );
        pnt_dump( dir_3 );

        fflush( stdout );
        fflush( stderr );
        assert( false );
    }

    bbox.init( dir, dir_2, dir_3 );
    ap_bbox.init();
    for  ( int  ind = 0; ind < size; ind++ ) {
        bbox.bound( start[ ind ] );
        ap_bbox.bound( start[ ind ] );
    }

    delete  pAlg;
    if  ( ap_bbox.volume() < bbox.volume() )
        bbox.init( ap_bbox );

    if  ( p_ap_bbox != NULL )
        *p_ap_bbox = ap_bbox;

    return  bbox;
}


gdiam_bbox   gdiam_approx_const_mvbb( gdiam_real  * start, int  size,
                                      gdiam_real  eps )
{
    gdiam_point  * p_arr;
    gdiam_bbox  gbbox;

    p_arr = gdiam_convert( start, size );
    gbbox = gdiam_approx_const_mvbb( p_arr, size, eps, NULL );
    free( p_arr );

    return   gbbox;
}

/*-----------------------------------------------------------------------
 * Code for computing best bounding box in a given drection
\*-----------------------------------------------------------------------*/

void  gdiam_generate_orthonormal_base( gdiam_point  in,
                                       gdiam_point  out1,
                                       gdiam_point  out2 )
{
    assert( pnt_length( in ) > 0.0 );

    pnt_normalize( in );

    // stupid cases..
    if  ( in[ 0 ] == 0.0 ) {
        if  ( in[ 1 ] == 0.0 ) {
            pnt_init_normalize( out1, 1, 0, 0 );
            pnt_init_normalize( out2, 0, 1, 0 );
            return;
        }
        if  ( in[ 2 ] == 0.0 ) {
            pnt_init_normalize( out1, 1, 0, 0 );
            pnt_init_normalize( out2, 0, 0, 1 );
            return;
        }
        pnt_init_normalize( out1, 0, -in[ 2 ], in[ 1 ] );
        pnt_init_normalize( out2, 1, 0, 0 );
        return;
    }
    if  ( ( in[ 1 ] == 0.0 )  &&  ( in[ 2 ] == 0.0 ) )  {
        pnt_init_normalize( out1, 0, 1, 0 );
        pnt_init_normalize( out2, 0, 0, 1 );
        return;
    }
    if  ( in[ 1 ] == 0.0 ) {
        pnt_init_normalize( out1, -in[ 2 ], 0, in[ 0 ] );
        pnt_init_normalize( out2, 0, 1, 0 );
        return;
    }
    if  ( in[ 2 ] == 0.0 ) {
        pnt_init_normalize( out1, -in[ 1 ], in[ 0 ], 0 );
        pnt_init_normalize( out2, 0, 0, 1 );
        return;
    }

    // all entries in the vector are not zero.
    pnt_init_normalize( out1, -in[ 1 ], in[ 0 ], 0 );
    pnt_cross_prod( in, out1, out2 );
    pnt_normalize( out2 );
}


class  point2d
{
public:
    gdiam_real  x, y;
    gdiam_point  src;

    void  init( gdiam_point  pnt,
                gdiam_point  base_x,
                gdiam_point  base_y ) {
        src = pnt;
        x = pnt_dot_prod( pnt, base_x );
        y = pnt_dot_prod( pnt, base_y );
    }

    gdiam_real  dist( const point2d  & pnt ) {
        return  sqrt( ( x - pnt.x ) * ( x - pnt.x )
                      + ( y - pnt.y ) * ( y - pnt.y ) );
    }

    void  dump() const {
        printf( "(%g, %g)\n", x, y );
    }
    bool  equal( const point2d  & pnt ) const {
        return  ( ( x == pnt.x )  &&  ( y == pnt.y ) );
    }
    bool  equal_real( const point2d  & pnt ) const {
        return  ( ( fabs( x - pnt.x ) < 1e-8 )
                  && ( fabs( y - pnt.y ) < 1e-8 ) );
    }
};

typedef  point2d  * point2d_ptr;
class  vec_point_2d : public std::vector<point2d_ptr> {};



inline ldouble     Area( const point2d  & a,
                        const point2d  & b,
                        const point2d  & c )
{
    double  x1, y1, x2, y2, len;

    x1 = b.x - a.x;
    y1 = b.y - a.y;

    x2 = c.x - a.x;
    y2 = c.y - a.y;

    if  ( ( x1 != 0.0 )  ||  ( y1 != 0.0 ) ) {
        len = sqrt( x1 * x1 + y1 * y1 );
        x1 /= len;
        y1 /= len;
    }

    if  ( ( x2 != 0.0 )  ||  ( y2 != 0.0 ) ) {
        len = sqrt( x2 * x2 + y2 * y2 );
        x2 /= len;
        y2 /= len;
    }

    ldouble  area;

    area = x1 * y2 - x2 * y1;
    return  area;
}


inline int     AreaSign( const point2d  & a,
                         const point2d  & b,
                         const point2d  & c )
{
    ldouble  area, area1, area2;

    area = a.x * b.y - a.y * b.x +
        a.y * c.x - a.x * c.y +
        b.x * c.y - c.x * b.y;

    return  ( ( area < (ldouble)0.0 )? -1:
              ( (area > (ldouble)0.0)? 1 : 0 ) );
}

inline  bool    Left(  const point2d  & a,
                         const point2d  & b,
                         const point2d  & c )
{
    return  AreaSign( a, b, c ) > 0;
}

inline  bool    Left_colinear(  const point2d  & a,
                                const point2d  & b,
                                const point2d  & c )
{
    return  AreaSign( a, b, c ) >= 0;
}


struct CompareByAngle {
public:
    point2d  base;

    bool operator()(const point2d_ptr  & a, const point2d_ptr   & b ) {
	int  sgn;
	gdiam_real  len1, len2;

        if  ( a->equal( *b ) )
            return  false;
        assert( a != NULL );
        assert( b != NULL );
        if  ( a->equal( base ) ) {
            assert( false );
            return  true;
        }
        if  ( b->equal( base ) ) {
            assert( false );
            return  false;
        }

	sgn = AreaSign( base, *a, *b );
        if  ( sgn != 0 )
            return  (sgn > 0);
        len1 = base.dist( *a );
        len2 = base.dist( *b );

        return (len1 > len2);
    }
};


point2d_ptr  get_min_point( vec_point_2d  & in,
                            int  & index )
{
    point2d_ptr  min_pnt = in[ 0 ];

    index = 0;

    for  ( int  ind = 0; ind < (int)in.size(); ind++ ) {
        if  ( in[ ind ]->y < min_pnt->y ) {
            min_pnt = in[ ind ];
            index = ind;
        } else
            if  ( ( in[ ind ]->y == min_pnt->y )
                  &&  ( in[ ind ]->x < min_pnt->x ) ) {
                min_pnt = in[ ind ];
                index = ind;
            }
    }

    return  min_pnt;
}


const void  dump( vec_point_2d   & vec )
{
    for  ( int  ind = 0; ind < (int)vec.size(); ind++ ) {
        printf( "-- %11d (%-11g, %-11g)\n",
                ind, vec[ ind ]->x,
                vec[ ind ]->y );
    }
    printf( "\n\n" );
}


static void  print_pnt( point2d_ptr  pnt )
{
    printf( "(%g, %g)\n", pnt->x, pnt->y );
}


void   verify_convex_hull( vec_point_2d  & in, vec_point_2d  & ch )
{
    ldouble  area;

    for  ( int  ind = 0; ind < (int)( ch.size() - 2 ); ind++ )
        assert( Left( *( ch[ ind ] ), *( ch[ ind + 1 ] ),
                      *( ch[ ind + 2 ] ) ) );
    assert( Left( *( ch[ ch.size() - 2 ] ), *( ch[ ch.size() - 1 ] ),
                  *( ch[ 0 ] ) ) );
    assert( Left( *( ch[ ch.size() - 1 ] ), *( ch[ 0 ] ),
                  *( ch[ 1 ] ) ) );

    for  ( int  ind = 0; ind < (int)( in.size() - 2 ); ind++ ) {
         for  ( int  jnd = 0; jnd < (int)( ch.size() - 1 ); jnd++ ) {
             if  ( ch[ jnd ]->equal_real( *( in[ ind ] ) ) )
                 continue;
             if  ( ch[ jnd + 1 ]->equal( *( in[ ind ] ) ) )
                 continue;

             if  ( ! Left_colinear( *( ch[ jnd ] ), *( ch[ jnd + 1 ] ),
                                    *( in[ ind ] ) ) ) {
                 area = Area( *( ch[ jnd ] ), *( ch[ jnd + 1 ] ),
                              *( in[ ind ] ) );
                 if  ( fabs( area ) < 1e-12 )
                     continue;
                 printf( "Failure in progress!\n\n" );
                 print_pnt( ch[ jnd ] );
                 print_pnt( ch[ jnd + 1 ] );
                 print_pnt( in[ ind ] );

                 printf( "ch[ jnd ]: (%g, %g)\n",
                         ch[ jnd ]->x, ch[ jnd ]->y );
                 printf( "ch[ jnd + 1 ]: (%g, %g)\n",
                         ch[ jnd + 1 ]->x, ch[ jnd + 1 ]->y );
                 printf( "ch[ ind ]: (%g, %g)\n",
                         in[ ind ]->x, in[ ind ]->y );
                 printf( "Area: %g\n", (double)Area( *( ch[ jnd ] ),
                                             *( ch[ jnd + 1 ] ),
                                             *( in[ ind ] ) ) );
                 printf( "jnd: %d, jnd + 1: %d, ind: %d\n",
                         jnd, jnd+1, ind );
                 fflush( stdout );
                 fflush( stderr );

                 assert( Left( *( ch[ jnd ] ), *( ch[ jnd + 1 ] ),
                               *( in[ ind ] ) ) );
             }
         }
         if  ( ch[ 0 ]->equal( *( in[ ind ] ) ) )
             continue;
         if  ( ch[ ch.size() - 1 ]->equal( *( in[ ind ] ) ) )
             continue;
         assert( Left( *( ch[ ch.size() - 1 ] ),
                       *( ch[ 0 ] ),
                       *( in[ ind ] ) ) );
    }
    printf( "Convex hull seems to be ok!\n" );
}



static  void   remove_duplicate( vec_point_2d  & in,
                                 point2d_ptr  ptr,
                                 int  start )
{
    int  dest;

    dest = start;
    while  ( start < (int)in.size() ) {
        if  ( in[ start ]->equal_real( *ptr ) ) {
            start++;
            continue;
        }
        in[ dest++ ] = in[ start++ ];
    }
    while  ( (int)in.size() > dest )
        in.pop_back();
}


static  void   remove_consecutive_dup( vec_point_2d  & in )
{
    int  dest, pos;

    if  ( in.size() < 1 )
        return;

    dest = pos = 0;

    // copy first entry
    in[ dest++ ] = in[ pos++ ];
    while  ( pos < ( (int)in.size() - 1 ) ) {
        if  ( in[ pos - 1 ]->equal_real( *(in[ pos ]) ) ) {
            pos++;
            continue;
        }
        in[ dest++ ] = in[ pos++ ];
    }
    in[ dest++ ] = in[ pos++ ];

    while  ( (int)in.size() > dest )
        in.pop_back();
}


// Copyright 2001 softSurfer, 2012 Dan Sunday
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
// isLeft(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2 on the line
//            <0 for P2 right of the line
//    See: Algorithm 1 on Area of Triangles
inline float
isLeft( const point2d &P0, const point2d &P1, const point2d &P2 )
{
    return (P1.x - P0.x)*(P2.y - P0.y) - (P2.x - P0.x)*(P1.y - P0.y);
}




// TODO - need to try switching to Monotone Chain Algorithm to avoid the area computation, which
// is problematic for strict weak ordering when doing sorting.  Useful links:
// http://stackoverflow.com/questions/1041620/most-efficient-way-to-erase-duplicates-and-sort-a-c-vector
// http://geomalgorithms.com/a10-_hull-1.html

void  convex_hull( vec_point_2d  & in, vec_point_2d  & out )
{

    CompareByAngle  comp;
    int  ind, position;

    assert( in.size() > 1 );

    comp.base = *( get_min_point( in, position ));
    std::swap( in[ 0 ], in[ position ] );

    remove_duplicate( in, in[ 0 ], 1 );

    for  ( int  ind = 0; ind < (int)in.size(); ind++ ) {
        assert( in[ ind ] != NULL );
    }

#if 1
    int  size;

    size = in.size();
    sort( in.begin() + 1, in.end(), comp );
    remove_consecutive_dup( in );
    out.push_back( in[ in.size() - 1 ] );
    out.push_back( in[ 0 ] );

    // perform the graham scan
    ind = 1;
    int  last;

    while ( ind < (int)in.size() ) {
        if  ( out[ out.size() -1 ]->equal_real( *( in[ ind ] ) ) ) {
            ind++;
            continue;
        }
        last = out.size();
        assert( last > 1 );

        if ( Left( *(out[ last - 2 ]),
                   *(out[ last - 1 ]),
                   *(in[ ind ]) ) ) {
            if  ( ! out[ last - 1 ]->equal( *( in[ ind ] ) ) )
                out.push_back( in[ ind ] );
            ind++;
        } else {
            if  ( out.size() < 3 ) {
                dump( out );
                printf( "Input:\n" );
                dump( in );
                printf( "ind: %d\n", ind );
                fflush( stdout );
                assert( out.size() > 2 );
            }

            out.pop_back();
        }
    }

    // we pushed in[ in.size() -1 ] twice in the output
    out.pop_back();
    std::vector<point2d_ptr>::iterator it;
    for(it = out.begin(); it != out.end(); it++){
	    std::cout << "point: " << (*it)->x << "," << (*it)->y << "\n";
    }

    //verify_convex_hull( in, out );
#endif


#if 0

    // Copyright 2001 softSurfer, 2012 Dan Sunday
    // This code may be freely used and modified for any purpose
    // providing that this copyright notice is included with it.
    // SoftSurfer makes no warranty for this code, and cannot be held
    // liable for any real or imagined damage resulting from its use.
    // Users of this code must verify correctness for their application.

    int n = in.size();
    sort( in.begin() + 1, in.end(), comp );
    remove_consecutive_dup( in );
    out.reserve(n);

    // the output array out[] will be used as the stack
    int    bot=0, top=(-1);   // indices for bottom and top of the stack
    int    i;                 // array scan index


    // Get the indices of points with min x-coord and min|max y-coord
    int minmin = 0, minmax;
    float xmin = in[0]->x;
    for (i=1; i<n; i++)
        if (in[i]->x != xmin) break;
    minmax = i-1;
    if (minmax == n-1) {       // degenerate case: all x-coords == xmin
        out[++top] = in[minmin];
        if (in[minmax]->y != in[minmin]->y) // a  nontrivial segment
            out[++top] =  in[minmax];
        out[++top] = in[minmin];            // add polygon endpoint
        return;
    }

    // Get the indices of points with max x-coord and min|max y-coord
    int maxmin, maxmax = n-1;
    float xmax = in[n-1]->x;
    for (i=n-2; i>=0; i--)
        if (in[i]->x != xmax) break;
    maxmin = i+1;

    // Compute the lower hull on the stack H
    out[++top] = in[minmin];      // push  minmin point onto stack
    i = minmax;
    while (++i <= maxmin)
    {
        // the lower line joins in[minmin]  with in[maxmin]
        if (isLeft( *in[minmin], *in[maxmin], *in[i])  >= 0 && i < maxmin)
            continue;           // ignore in[i] above or on the lower line

        while (top > 0)         // there are at least 2 points on the stack
        {
            // test if  in[i] is left of the line at the stack top
            if (isLeft(  *out[top-1], *out[top], *in[i]) > 0)
                 break;         // in[i] is a new hull  vertex
            else
                 top--;         // pop top point off  stack
        }
        out[++top] = in[i];        // push in[i] onto stack
    }
    // Next, compute the upper hull on the stack out above  the bottom hull
    if (maxmax != maxmin)      // if  distinct xmax points
         out[++top] = in[maxmax];  // push maxmax point onto stack
    bot = top;                  // the bottom point of the upper hull stack
    i = maxmin;
    while (--i >= minmax)
    {
        // the upper line joins in[maxmax]  with in[minmax]
        if (isLeft( *in[maxmax], *in[minmax], *in[i])  >= 0 && i > minmax)
            continue;           // ignore in[i] below or on the upper line

            out.pop_back();
        while (top > bot)     // at least 2 points on the upper stack
        {
            // test if  in[i] is left of the line at the stack top
            if (isLeft( *out[top-1], *out[top], *in[i]) > 0)
                 break;         // in[i] is a new hull  vertex
            else
                 top--;         // pop top point off  stack
        }
        out[++top] = in[i];        // push in[i] onto stack
    }
    if (minmax != minmin)
        out[++top] = in[minmin];  // push  joining endpoint onto stack

    std::vector<point2d_ptr>::iterator it;
    for(it = out.begin(); it != out.end(); it++){
	    std::cout << "point: " << (*it)->x << "," << (*it)->y << "\n";
    }
#endif

}


struct bbox_2d_info
{
    gdiam_point_2d_t  vertices[ 4 ];
    gdiam_real  area;

    void  get_dir( int  ind, gdiam_point_2d  out ) {
        out[ 0 ] = vertices[ ind ][ 0 ]
            - vertices[ (ind + 1) % 4 ][ 0 ];
        out[ 1 ] = vertices[ ind ][ 1 ]
            - vertices[ (ind + 1) % 4 ][ 1 ];
    }

    void  dump() {
        printf( "--- bbox 2d ----------------------------\n" );
        for  ( int  ind = 0; ind < 4; ind++ )
            printf( "%d: (%g, %g)\n", ind, vertices[ ind ][0],
                    vertices[ ind ][1] );
        for  ( int  ind = 0; ind < 4; ind++ ) {
            gdiam_point_2d_t  dir;

            get_dir( ind, dir );

            printf( "dir %d: (%g, %g)\n", ind, dir[0],
                    dir[1] );
        }
        printf( "\\----------------------------------\n" );
    }

};

#define  EPS  1e-6
inline  bool   eq_real( gdiam_real  a, gdiam_real  b ) {
    return  ( ( ( b - EPS ) < a )
              &&  ( a < (b + EPS) ) );
}

class  MinAreaRectangle
{
private:
    vec_point_2d   ch;
    gdiam_real  * angles;

public:
    void  dump_ch() {
        for  ( int  ind = 0; ind < (int)ch.size(); ind++ ) {
            printf( "%d: (%g, %g)\n", ind, ch[ ind ]->x,
                    ch[ ind ]->y );
        }
    }

    void  compute_edge_dir( int  u ) {
        int  u2;
        double  ang;
        double  x1, y1, x2, y2;

        u2 = (u + 1) % ch.size();

        x1 = ch[ u ]->x;
        y1 = ch[ u ]->y;
        x2 = ch[ u2 ]->x;
        y2 = ch[ u2 ]->y;

        ang = atan2( y2 - y1, x2 - x1 );
        if  ( ang < 0 )
            ang += 2.0 * PI;
        angles[ u ] = ang;

        // floating point nonsence. A huck to solve this...
        if  ( ( u > 0 )
              &&  ( angles[ u ] < angles[ u - 1 ] )
              &&   eq_real( angles[ u ], angles[ u - 1 ] ) )
            angles[ u ] = angles[ u - 1 ];
    }

    /* Finding the first vertex whose edge is over a given angle */
    int  find_vertex( int  start_vertex,
                      double  trg_angle ) {
        double  prev_angle, curr_angle;
        bool  f_found;
        int  ver, prev_vertex;

        f_found = false;

        prev_vertex = start_vertex - 1;
        if  ( prev_vertex < 0 )
            prev_vertex = ch.size() - 1;

        prev_angle = angles[ prev_vertex ];
        ver = start_vertex;
        while  ( ! f_found ) {
            curr_angle = angles[ ver ];
            if ( prev_angle <= curr_angle )
                f_found = ( ( prev_angle < trg_angle)
                            &&  ( trg_angle <= curr_angle ) );
            else
                f_found = ( ( prev_angle < trg_angle )
                            ||  ( trg_angle <= curr_angle ));

            if ( f_found)
                break;
            else
                prev_angle = curr_angle;

            ver = ( ver + 1 ) % ch.size();
        }

        return  ver;
    }


    /* Computing the intersection point of two lines, each given by a
   point and slope */
    void  compute_crossing( int  a_ind, double  a_angle,
                            int  b_ind, double  b_angle,
                            gdiam_real  & x,
                            gdiam_real  & y ) {
        double  a_slope, b_slope, x_a, x_b, y_a, y_b;

        if ( eq_real( a_angle, 0.0 )  ||  eq_real( a_angle, PI ) ) {
            x = ch[ b_ind ]->x;
            y = ch[ a_ind ]->y;
            return;
        }
        if ( eq_real( a_angle, PI/2.0 )  ||  eq_real( a_angle, PI * 1.5 ) ) {
            x = ch[ a_ind ]->x;
            y = ch[ b_ind ]->y;
            return;
        }

        a_slope = tan( a_angle );
        b_slope = tan( b_angle );
        x_a = ch[ a_ind ]->x;
        y_a = ch[ a_ind ]->y;
        x_b = ch[ b_ind ]->x;
        y_b = ch[ b_ind ]->y;

        double  a_const, b_const, x_out, y_out;
        a_const = y_a - a_slope * x_a;
        b_const = y_b - b_slope * x_b;
        x_out = - ( a_const - b_const ) / (a_slope - b_slope);
        y_out = a_slope * x_out + a_const;
        x = x_out;
        y = y_out;
    }


    void  get_bbox( int   a_ind, gdiam_real   a_angle,
                    int   b_ind, gdiam_real   b_angle,
                    int   c_ind, gdiam_real   c_angle,
                    int   d_ind, gdiam_real   d_angle,
                    bbox_2d_info   & bbox,
                    gdiam_real  & area ) {
        compute_crossing( a_ind, a_angle, b_ind, b_angle,
                          bbox.vertices[ 0 ][ 0 ],
                          bbox.vertices[ 0 ][ 1 ] );
        compute_crossing( b_ind, b_angle, c_ind, c_angle,
                          bbox.vertices[ 1 ][ 0 ],
                          bbox.vertices[ 1 ][ 1 ] );
        compute_crossing( c_ind, c_angle, d_ind, d_angle,
                          bbox.vertices[ 2 ][ 0 ],
                          bbox.vertices[ 2 ][ 1 ] );
        compute_crossing( d_ind, d_angle, a_ind, a_angle,
                          bbox.vertices[ 3 ][ 0 ],
                          bbox.vertices[ 3 ][ 1 ] );

        area = pnt_distance_2d( bbox.vertices[ 0 ],
                              bbox.vertices[ 1 ] )
            * pnt_distance_2d( bbox.vertices[ 0 ],
                               bbox.vertices[ 3 ] );
    }

    /* Given one angle (bbx direction), find the other three */
    void  get_angles( int  ind,
                      gdiam_real   & angle_1,
                      gdiam_real   & angle_2,
                      gdiam_real   & angle_3 ) {
        double   angle;

        angle = angles[ ind ];
        angle_1 = angle + PI * 0.5;

        if   ( angle_1 >= (2.0 * PI) )
            angle_1 -= 2.0 * PI;

        angle_2 = angle + PI;
        if ( angle_2 >= (2.0 * PI) )
            angle_2 -= 2.0 * PI;

        angle_3 = angle + 1.5 * PI;
        if ( angle_3 >= (2.0 * PI) )
            angle_3 -= 2.0 * PI;
    }

    void  compute_min_bbox_inner( bbox_2d_info   & min_bbox,
                                  gdiam_real  & min_area ) {
        int        u, v, s, t;
        gdiam_real     ang1, ang2, ang3, tmp_area;
        bbox_2d_info  tmp_bbox;

        angles = (gdiam_real *)malloc( sizeof( gdiam_real )
                                           * (int)ch.size() );
        assert( angles != NULL );

        /* Pre-computing all edge directions */
        for (u = 0; u < (int)ch.size(); u ++)
            compute_edge_dir( u );

        /* Initializing the search */
        get_angles( 0, ang1, ang2, ang3 );

        s = find_vertex( 0, ang1 );
        v = find_vertex( s, ang2 );
        t = find_vertex( v, ang3 );

        get_bbox( 0, angles[ 0 ],
                  s, ang1,
                  v, ang2,
                  t, ang3,
                  min_bbox, min_area );

        /* Performing a double rotating calipers */
        for  ( u = 1; u < (int)ch.size(); u ++ ) {
            get_angles( u, ang1, ang2, ang3 );
            s = find_vertex( s, ang1 );
            v = find_vertex( v, ang2 );
            t = find_vertex( t, ang3 );
            get_bbox( u, angles[ u ],
                      s, ang1,
                      v, ang2,
                      t, ang3,
                      tmp_bbox, tmp_area );
            if  ( min_area > tmp_area ) {
                min_area = tmp_area;
                min_bbox = tmp_bbox;
            }
        }
        free( angles );
        angles = NULL;
    }

    void  compute_min_bbox( vec_point_2d   & points,
                            bbox_2d_info   & min_bbox,
                            gdiam_real  & min_area ) {
        convex_hull( points, ch );
        compute_min_bbox_inner( min_bbox, min_area );
    }

};

void   dump_points( gdiam_point  * in_arr,
                    int  size )
{
    for  ( int  ind = 0; ind < size; ind++ ) {
        printf( "%d: (%g, %g, %g)\n", ind,
                in_arr[ ind ][ 0 ],
                in_arr[ ind ][ 1 ],
                in_arr[ ind ][ 2 ] );
    }
}

#define  ZERO_EPS  (1e-6)

static void   construct_base_inner( gdiam_point  pnt1,
                             gdiam_point  pnt2,
                             gdiam_point  pnt3 )
{
    pnt_normalize( pnt1 );
    pnt_normalize( pnt2 );
    pnt_normalize( pnt3 );

    if  ( ( pnt_length( pnt1 ) < ZERO_EPS )
          &&  ( pnt_length( pnt2 ) < ZERO_EPS )
          &&  ( pnt_length( pnt3 ) < ZERO_EPS ) ){
        assert( false );
    }

    if  ( ( pnt_length( pnt1 ) < ZERO_EPS )
          &&  ( pnt_length( pnt2 ) < ZERO_EPS ) ) {
        gdiam_generate_orthonormal_base( pnt3, pnt1, pnt2 );
        return;
    }
    if  ( ( pnt_length( pnt1 ) < ZERO_EPS )
          &&  ( pnt_length( pnt3 ) < ZERO_EPS ) ) {
        gdiam_generate_orthonormal_base( pnt2, pnt1, pnt3 );
        return;
    }
    if  ( ( pnt_length( pnt2 ) < ZERO_EPS )
          &&  ( pnt_length( pnt3 ) < ZERO_EPS ) ) {
        gdiam_generate_orthonormal_base( pnt1, pnt2, pnt3 );
        return;
    }
    if  ( pnt_length( pnt1 ) < ZERO_EPS ) {
        pnt_cross_prod( pnt2, pnt3, pnt1 );
        return;
    }
    if  ( pnt_length( pnt2 ) < ZERO_EPS ) {
        pnt_cross_prod( pnt1, pnt3, pnt2 );
        return;
    }
    if  ( pnt_length( pnt3 ) < ZERO_EPS ) {
        pnt_cross_prod( pnt1, pnt2, pnt3 );
        return;
    }
}

void   construct_base( gdiam_point  pnt1,
                       gdiam_point  pnt2,
                       gdiam_point  pnt3 )
{
    construct_base_inner( pnt1, pnt2, pnt3 );
    pnt_scale_and_add( pnt2, -pnt_dot_prod( pnt1, pnt2 ),
                       pnt1 );
    pnt_normalize( pnt2 );

    pnt_scale_and_add( pnt3, -pnt_dot_prod( pnt1, pnt3 ),
                       pnt1 );
    pnt_scale_and_add( pnt3, -pnt_dot_prod( pnt2, pnt3 ),
                       pnt2 );
    pnt_normalize( pnt3 );
}


class  ProjPointSet
{
private:
    gdiam_point_t  base_x, base_y, base_proj;
    point2d  * arr;
    vec_point_2d   points, ch;
    gdiam_bbox   bbox;
    gdiam_point  * in_arr;
    int  size;

public:
    ~ProjPointSet() {
        term();
    }
    void  init( gdiam_point  dir,
                gdiam_point  * _in_arr,
                int  _size ) {
        arr = NULL;

        if  ( pnt_length( dir ) == 0.0 ) {
            dump_points( _in_arr, _size );
            pnt_dump( dir );
            fflush( stdout );
            fflush( stderr );
            assert( pnt_length( dir ) > 0.0 );
        }
        size = _size;
        in_arr = _in_arr;
        assert( size > 0 );

        pnt_copy( base_proj, dir );
        gdiam_generate_orthonormal_base( base_proj, base_x, base_y );

        arr = (point2d *)malloc( sizeof( point2d ) * size );
        assert( arr != 0 );

        for  ( int  ind = 0; ind < size; ind++ ) {
            arr[ ind ].init( in_arr[ ind ], base_x, base_y );
            points.push_back( &( arr[ ind ] ) );
        }
    }

    void  compute() {
        MinAreaRectangle  mar;
        bbox_2d_info   min_bbox;
        gdiam_real   min_area;
        double  x1, y1, x2, y2;
        gdiam_point_t  out_1, out_2;

        mar.compute_min_bbox( points, min_bbox, min_area );

        // We take the resulting min_area rectangle and lift it back to 3d...
        x1 = min_bbox.vertices[ 0 ][ 0 ] -
            min_bbox.vertices[ 1 ][ 0 ];
        y1 = min_bbox.vertices[ 0 ][ 1 ] -
            min_bbox.vertices[ 1 ][ 1 ];
        x2 = min_bbox.vertices[ 0 ][ 0 ] -
            min_bbox.vertices[ 3 ][ 0 ];
        y2 = min_bbox.vertices[ 0 ][ 1 ] -
            min_bbox.vertices[ 3 ][ 1 ];

        double  len;

        len = sqrt( x1 * x1 + y1 * y1 );
        if  ( len > 0.0 ) {
            x1 /= len;
            y1 /= len;
        }

        len = sqrt( x2 * x2 + y2 * y2 );
        if  ( len > 0.0 ) {
            x2 /= len;
            y2 /= len;
        }

        pnt_zero( out_1 );
        pnt_scale_and_add( out_1, x1, base_x );
        pnt_scale_and_add( out_1, y1, base_y );
        pnt_normalize( out_1 );

        pnt_zero( out_2 );
        pnt_scale_and_add( out_2, x2, base_x );
        pnt_scale_and_add( out_2, y2, base_y );
        pnt_normalize( out_2 );

        construct_base( base_proj, out_1, out_2 );
        if  ( ( ! (fabs( pnt_dot_prod( base_proj, out_1 ) ) < 1e-6 ) )
              ||  ( ! (fabs( pnt_dot_prod( base_proj, out_2 ) ) < 1e-6 ) )
              ||  ( ! (fabs( pnt_dot_prod( out_1, out_2 ) ) < 1e-6 ) ) ) {
            printf( "should be all close to zero: %g, %g, %g\n",
                    pnt_dot_prod( base_proj, out_1 ),
                    pnt_dot_prod( base_proj, out_2 ),
                    pnt_dot_prod( out_1, out_2 ) );
            pnt_dump( base_proj );
            pnt_dump( out_1 );
            pnt_dump( out_2 );


            printf( "real points:\n" );
            dump_points( in_arr, size );

            printf( "points on CH:\n" );
            dump( points );

            printf( "BBox:\n" );
            min_bbox.dump();

            fflush( stdout );
            fflush( stderr );
            assert( fabs( pnt_dot_prod( base_proj, out_1 ) ) < 1e-6 );
            assert( fabs( pnt_dot_prod( base_proj, out_2 ) ) < 1e-6 );
            assert( fabs( pnt_dot_prod( out_1, out_2 ) ) < 1e-6 );
        }

        bbox.init( base_proj, out_1, out_2 );
        for  ( int  ind = 0; ind < size; ind++ )
            bbox.bound( in_arr[ ind ] );
    }

    gdiam_bbox   & get_bbox() {
        return  bbox;
    }

    void  term() {
        if  ( arr != NULL ) {
            free( arr );
            arr = NULL;
        }
    }
    void  init() {
    }
};


gdiam_bbox   gdiam_mvbb_optimize( gdiam_point  * start, int  size,
                                  gdiam_bbox  bb_out, int  times  )
{
    gdiam_bbox  bb_tmp;

    for  ( int  ind = 0; ind < times; ind++ ) {
        ProjPointSet  pps;

        if  ( pnt_length( bb_out.get_dir( ind % 3 ) ) == 0.0 ) {
            printf( "Dumping!\n" );
            bb_out.dump();
            fflush( stdout );
            continue;
        }

        pps.init( bb_out.get_dir( ind % 3 ), start, size );

        pps.compute();
        bb_tmp = pps.get_bbox();

        if  ( bb_tmp.volume() < bb_out.volume() )
            bb_out = bb_tmp;
    }

    return  bb_out;
}


gdiam_bbox   gdiam_approx_mvbb( gdiam_point  * start, int  size,
                                gdiam_real  eps )
{
    gdiam_bbox  bb, bb2;

    bb = gdiam_approx_const_mvbb( start, size, 0.0, NULL );
    bb2 = gdiam_mvbb_optimize( start, size,  bb, 10 );

    return  bb2;
}


static int  gcd2( int  a, int  b )
{
    if  ( a == 0  ||  a == b )
        return  b;
    if  ( b == 0 )
        return  a;
    if  ( a > b )
        return  gcd2( a % b, b );
    else
        return  gcd2( a, b % a );
}


static int  gcd3( int  a, int  b, int  c )
{
    a = abs( a );
    b = abs( b );
    c = abs( c );
    if   ( a == 0 )
        return  gcd2( b, c );
    if   ( b == 0 )
        return  gcd2( a, c );
    if   ( c == 0 )
        return  gcd2( a, b );

    return  gcd2( a, gcd2( b, c ) );
}


static void  try_direction( gdiam_bbox  & bb,
                            gdiam_bbox  & bb_out,
                            gdiam_point  * start,
                            int  size,
                            int  x_coef,
                            int  y_coef,
                            int  z_coef )
{
    gdiam_bbox  bb_new;

    if  ( gcd3( x_coef, y_coef, z_coef ) > 1 )
        return;
    if  ( ( x_coef == 0 )  &&  ( y_coef == 0 )
          &&  ( z_coef == 0 ) )
        return;
    gdiam_point_t  new_dir;
    bb.combine( new_dir, x_coef, y_coef, z_coef );

    ProjPointSet  pps;

    pps.init( new_dir, start, size );

    pps.compute();

    bb_new = pps.get_bbox();
    bb_new = gdiam_mvbb_optimize( start, size, bb_new, 10 );

    if  ( bb_new.volume() < bb_out.volume() )
        bb_out = bb_new;
}


gdiam_bbox   gdiam_approx_mvbb_grid( gdiam_point  * start, int  size,
                                     int  grid_size )
{
    gdiam_bbox  bb, bb_out;

    bb_out = bb = gdiam_approx_const_mvbb( start, size, 0.0, NULL );
    if  ( bb.volume() == 0 ) {
        dump_points( start, size );
        printf( "1zero volume???\n" );
        bb.dump();
    }

    bb_out = bb = gdiam_mvbb_optimize( start, size, bb_out, 10 );

    if  ( bb.volume() == 0 ) {
        printf( "2zero volume???\n" );
        bb.dump();
    }

    for  ( int  x_coef = -grid_size; x_coef <= grid_size; x_coef++ )
        for  ( int  y_coef = -grid_size; y_coef <= grid_size; y_coef++ )
            for  ( int  z_coef = 0; z_coef <= grid_size; z_coef++ )
                try_direction( bb, bb_out, start, size,
                               x_coef, y_coef, z_coef );

    bb_out = gdiam_mvbb_optimize( start, size, bb_out, 10 );

    return  bb_out;
}


static void   register_point( gdiam_point  pnt,
                              gdiam_point  * tops,
                              gdiam_point  * bottoms,
                              int  grid_size,
                              gdiam_bbox  & bb )
{
    gdiam_point_t  pnt_bb, pnt_bottom, pnt_top;
    int  x_ind, y_ind, position;

    bb.get_normalized_coordinates( pnt, pnt_bb );

    // x_ind
    x_ind = (int)( pnt_bb[ 0 ] * grid_size );
    assert( ( -1 <= x_ind )  &&  ( x_ind <= grid_size ) );
    if  ( x_ind < 0 )
        x_ind = 0;
    if  ( x_ind >= grid_size )
        x_ind = grid_size - 1;

    // y_ind
    y_ind = (int)( pnt_bb[ 1 ] * grid_size );
    assert( ( -1 <= y_ind )  &&  ( y_ind <= grid_size ) );
    if  ( y_ind < 0 )
        y_ind = 0;
    if  ( y_ind >= grid_size )
        y_ind = grid_size - 1;

    position = x_ind + y_ind * grid_size;
    if  ( tops[ position ] == NULL ) {
        tops[ position ] = bottoms[ position ] = pnt;
        return;
    }

    bb.get_normalized_coordinates( tops[ position ], pnt_top );
    if  ( pnt_top[ 2 ] < pnt_bb[ 2 ] )
        tops[ position ] = pnt;

    bb.get_normalized_coordinates( bottoms[ position ], pnt_bottom );
    if  ( pnt_bottom[ 2 ] > pnt_bb[ 2 ] )
        bottoms[ position ] = pnt;
}



// gdiam_convex_sample

//   We are given a point set, and (hopefully) a tight fitting
// bounding box. We compute a sample of the given size that represents
// the point-set. The only guarenteed is that if we use ample of size m,
// we get an approximation of quality about 1/\sqrt{m}. Note that we pad
// the sample if necessary to get the desired size.
gdiam_point  * gdiam_convex_sample( gdiam_point  * start, int  size,
                                    gdiam_bbox  & bb,
                                    int  sample_size )
{
    int  grid_size, mem_size, grid_entries;
    gdiam_point  * bottoms, * tops, * out_arr;
    int  out_count;

    assert( sample_size > 1 );

    // we want the grid to be on the two minor dimensions.
    bb.set_third_dim_longest();

    grid_size = (int)sqrt((double)(sample_size / 2) );

    grid_entries = grid_size * grid_size;
    mem_size = (int)( sizeof( gdiam_point ) * grid_size * grid_size );

    bottoms = (gdiam_point *)malloc( mem_size );
    tops = (gdiam_point *)malloc( mem_size );
    out_arr = (gdiam_point *)malloc( sizeof( gdiam_point ) * sample_size );

    assert( bottoms != NULL );
    assert( tops != NULL );
    assert( out_arr != NULL );

    for  ( int  ind = 0; ind < grid_entries; ind++ )
        tops[ ind ] = bottoms[ ind ] = NULL;

    // Now we stream the points registering them with the relevant
    // shaft in the grid.
    for  ( int  ind = 0; ind < size; ind++ )
        register_point( start[ ind ], tops, bottoms,
                        grid_size, bb );

    out_count = 0;
    for  ( int  ind = 0; ind < grid_entries; ind++ ) {
        if  ( tops[ ind ] == NULL )
            continue;

        out_arr[ out_count++ ] = tops[ ind ];
        if  ( tops[ ind ] != bottoms[ ind ] )
            out_arr[ out_count++ ] = bottoms[ ind ];
    }

    // We dont have enough entries in our sample - lets randomly pick
    // points.
    while  ( out_count < sample_size )
        out_arr[ out_count++ ] = start[ rand() % size ];

    free( tops );
    free( bottoms );

    return  out_arr;
}


gdiam_bbox   gdiam_approx_mvbb_grid_sample( gdiam_point  * start, int  size,
                                            int  grid_size, int  sample_size )
{
    gdiam_bbox  bb, bb_new;
    gdiam_point  * sample;

    if  ( sample_size >= size )
        return   gdiam_approx_mvbb_grid( start, size, grid_size );

    bb = gdiam_approx_const_mvbb( start, size, 0.0, NULL );

    sample = gdiam_convex_sample( start, size, bb, sample_size );

    bb_new = gdiam_approx_mvbb_grid( sample, sample_size, grid_size );

    for  ( int  ind = 0; ind < size; ind++ )
        bb_new.bound( start[ ind ] );

    return  bb_new;
}



gdiam_bbox   gdiam_approx_mvbb_grid_sample( gdiam_real  * start, int  size,
                                            int  grid_size, int  sample_size )
{
    gdiam_point  * p_arr;
    gdiam_bbox  bb;

    p_arr = gdiam_convert( start, size );
    bb = gdiam_approx_mvbb_grid_sample( p_arr, size, grid_size, sample_size );
    free( p_arr );

    return  bb;
}


/* gdiam.C - End of File ------------------------------------------*/

