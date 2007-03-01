
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include "brep.h"
#include "vector.h"

using namespace std;

const int COUNT = 100000000;

int
main(int argc, char** argv)
{
#ifdef __x86_vector__
  printf("using x86 vectorization\n");
#elif defined(__fpu_vector__)
  printf("using fpu vectorization\n");
#endif

  { // test correctness
    double _a[8] VEC_ALIGN = {0,1,2,3, 4, 5, 6, 7};
    double _b[8] VEC_ALIGN = {2,4,6,8,10,12,14,16};    
    dvec<8> a(_a, true);
    dvec<8> b(_b, true);
    
    dvec<8> c = a + b;
    double _c[8] VEC_ALIGN = {2,5,8,11,14,17,20,23};
    assert( c == dvec<8>(_c,true) );

    dvec<8> d = a - b;
    double _d[8] VEC_ALIGN = {-2,-3,-4,-5,-6,-7,-8,-9};
    assert( d == dvec<8>(_d,true) );

    dvec<8> e = a * b;
    double _e[8] VEC_ALIGN = {0,4,12,24,40,60,84,112};
    assert( e == dvec<8>(_e,true) );

    dvec<8> f = a / b;
    double _f[8] VEC_ALIGN = {0,0.25,0.333333333333,0.375,0.40,0.4166666666,0.42857142,0.4375};
    assert( f == dvec<8>(_f,true) );

    dvec<8> g = a.madd(c, b);
    double _g[8] VEC_ALIGN = {2,9,22,41,66,97,134,177};
    assert( g == dvec<8>(_g,true) );

    dvec<8> h = a.madd(2.0,b);
    double _h[8] VEC_ALIGN = {2,6,10,14,18,22,26,30};
    assert( h == dvec<8>(_h,true) );    

    srand(time(NULL));
    double total = 0.0;
    double rval[] = {
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0, 
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0,
      rand()/10000.0 };
    
    clock_t start = clock(); 
    for (int i = 0; i < COUNT; i++) { 
      double _a[8] VEC_ALIGN = {rval[i%8],rval[i%8+1],rval[i%8+2],rval[i%8+3],
				rval[i%8+4],rval[i%8+5],rval[i%8+6],rval[i%8+7]};
      double _b[8] VEC_ALIGN = {rval[i%8+1],rval[i%8+2],rval[i%8+3],rval[i%8+4],
				rval[i%8+5],rval[i%8+6],rval[i%8+7],rval[i%8+8]};

      dvec<8> a(_a,true);
      dvec<8> b(_b,true);
       
      dvec<8> c = a + b;
      dvec<8> d = c - a;
      dvec<8> e = d * c;
      dvec<8> f = e / a;
      dvec<8> g = a.madd(rval[i%16],f);
      dvec<8> h = g.madd(f, a+b*c-d/e*f);
      total += h[7];
    }
    printf("dvec<8> time: %3.4g\n", (double)(clock()-start)/(double)CLOCKS_PER_SEC);
  }

  /* test correctness */
  vec2d a(100.0,-100.0);
  vec2d b(200.0,-200.0);

  vec2d c = a + b;
  assert( vequals(c, vec2d(300.0,-300.0)) );
  
  vec2d d = a - b;
  assert( vequals(d, vec2d(-100.0,100.0)) );
  
  vec2d e = a * b;
  assert( vequals(e, vec2d(20000.0,20000.0)) );
  
  vec2d f = b / a;
  assert( vequals(f, vec2d(2.0,2.0)) );

  vec2d g = a.madd(20.0,b);
  assert( vequals(g, vec2d(2200.0, -2200.0)) );
  
  vec2d h = a.madd(d,b);
  assert( vequals(h, vec2d(-9800.0,-10200.0)) );

  /* simple performance test */

  srand(time(NULL));
  double total = 0.0;
  double rval[] = {
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0, 
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0,
    rand()/10000.0 };

  clock_t start = clock(); 
  for (int i = 0; i < COUNT; i++) { 
    vec2d a(rval[i%12], rval[i%12+2]);
    vec2d b(rval[i%12+1], rval[i%12+3]);

    vec2d c = a + b;
    vec2d d = c - a;
    vec2d e = d * c;
    vec2d f = e / a;
    vec2d g = a.madd(rval[i%12],f);
    vec2d h = g.madd(g, a + b - c + d - e + f);
    total += h.x() + h.y();
  }
  printf("vec2d time: %3.4g\n", (double)(clock()-start)/(double)CLOCKS_PER_SEC);

  return total > 0;
}

