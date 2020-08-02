/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors: Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright (c) 2012 Vincent Barichard
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "common.h"

#include <gecode/driver.hh>

#include <gecode/minimodel.hh>
#include <gecode/float.hh>

#include "bu/app.h"

using namespace Gecode;

/**
 * \brief %Example: Cartesian Heart
 *
 * There are many mathematical curves that produce heart shapes.
 * With a good solving effort, coordinates of a filled heart shape
 * can be computed by solving the cartesian equation:
 *
 * \f[
 * x^2+2\left(y-p\times\operatorname{abs}(x)^{\frac{1}{q}}\right)^2 = 1
 * \f]
 *
 * By setting \f$p=0.5\f$ and \f$q=2\f$, it yields to the equation:
 * 
 * \f[
 * x^2+2\left(y-\frac{\operatorname{abs}(x)^{\frac{1}{2}}}{2}\right)^2 = 1
 * \f]
 * 
 * To get reasonable interval starting sizes, \f$x\f$ and \f$y\f$
 * are restricted to \f$[-20;20]\f$.
 *
 * \ingroup Example
 */
class CartesianHeart : public Script {
protected:
  /// The numbers
  FloatVarArray f;
  /// Minimum distance between two solutions
  FloatNum step;
public:
  /// Actual model
  CartesianHeart(const Options&) 
    : f(*this,2,-20,20), step(0.01) {
    int q = 2;
    FloatNum p = 0.5;
    // Post equation
    rel(*this, sqr(f[0]) + 2*sqr(f[1]-p*nroot(abs(f[0]),q)) == 1);
    branch(*this, f[0], FLOAT_VAL_SPLIT_MIN());
    branch(*this, f[1], FLOAT_VAL_SPLIT_MIN());
  }
  /// Constructor for cloning \a p
  CartesianHeart(bool share, CartesianHeart& p) 
    : Script(share,p), step(p.step) {
    f.update(*this,share,p.f);
  }
  /// Copy during cloning
  virtual Space* copy(bool share) { 
    return new CartesianHeart(share,*this); 
  }
  /// Add constraints to current model to get next solution (not too close)
  virtual void constrain(const Space& _b) {
    const CartesianHeart& b = static_cast<const CartesianHeart&>(_b);
    rel(*this, 
        (f[0] >= (b.f[0].max()+step)) || 
        (f[1] >= (b.f[1].max()+step)) || 
        (f[1] <= (b.f[1].min()-step)));
  }
  /// Print solution coordinates
  virtual void print(std::ostream& os) const {
    os << "XY " << f[0].med() << " " << f[1].med()
       << std::endl;
  }

};

/** \brief Main-function
 *  \relates CartesianHeart
 */
int main(int argc, char* argv[]) {
  bu_setprogname(argv[0]);
  Options opt("CartesianHeart");
  opt.parse(argc,argv);
  opt.solutions(0);
  Script::run<CartesianHeart,BAB,Options>(opt);
  return 0;
}

// STATISTICS: example-any
