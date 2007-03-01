
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

  {
    double _a[8] VEC_ALIGN = {0,1,2,3, 4, 5, 6, 7};
    double _b[8] VEC_ALIGN = {2,4,6,8,10,12,14,16};
    dvec<8> a(_a, true);
    dvec<8> b(_b, true);
    
    dvec<8> c = a + b;
    double _c[8] VEC_ALIGN = {2,5,8,11,14,17,20,23};
    assert( c == dvec<8>(_c,true) );
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
  printf("Time: %3.4g\n", (double)(clock()-start)/(double)CLOCKS_PER_SEC);

  return total > 0;
}

