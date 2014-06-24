/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Copyright:
 *     Christopher Mears, 2012
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

namespace Gecode {
  
  /** \brief Interchangeable rows symmetry specification.
   */
  template<class A>
  SymmetryHandle
  rows_interchange(const Matrix<A>& m) {
    typename Matrix<A>::ArgsType xs;
    for (int r = 0 ; r < m.height() ; r++)
      xs << m.row(r);
    return VariableSequenceSymmetry(xs, m.width());
  }

  /** \brief Interchangeable columns symmetry specification.
   */
  template<class A>
  SymmetryHandle
  columns_interchange(const Matrix<A>& m) {
    typename Matrix<A>::ArgsType xs;
    for (int c = 0 ; c < m.width() ; c++)
      xs << m.col(c);
    return VariableSequenceSymmetry(xs, m.height());
  }

  /** \brief Reflect rows symmetry specification.
   */
  template<class A>
  SymmetryHandle
  rows_reflect(const Matrix<A>& m) {
    int nrows = m.height();
    int ncols = m.width();
    // Length of each sequence in the symmetry.
    int length = (nrows/2) * ncols;
    typename Matrix<A>::ArgsType xs(length * 2);
    for (int i = 0 ; i < length ; i++) {
      // Break position i into its coordinates
      int r1 = i/ncols;
      int c1 = i%ncols;
      // r2 is the row symmetric with r1
      int r2 = nrows - r1 - 1;
      // The column remains the same
      int c2 = c1;
      xs[i] = m(c1,r1);
      xs[length+i] = m(c2,r2);
    }
    return VariableSequenceSymmetry(xs, length);
  }
  
  /** \brief Reflect columns symmetry specification.
   */
  template<class A>
  SymmetryHandle columns_reflect(const Matrix<A>& m) {
    int nrows = m.height();
    int ncols = m.width();
    // Length of each sequence in the symmetry.
    int length = (ncols/2) * nrows;
    typename Matrix<A>::ArgsType xs(length * 2);
    for (int i = 0 ; i < length ; i++) {
      // Break position i into its coordinates
      int r1 = i/ncols;
      int c1 = i%ncols;
      // c2 is the column symmetric with c1
      int c2 = ncols - c1 - 1;
      // The row remains the same
      int r2 = r1;
      xs[i] = m(c1,r1);
      xs[length+i] = m(c2,r2);
    }
    return VariableSequenceSymmetry(xs, length);
  }

  /** \brief Reflect around main diagonal symmetry specification.
   */
  template<class A>
  SymmetryHandle diagonal_reflect(const Matrix<A>& m) {
    int nrows = m.height();
    int ncols = m.width();

    typename Matrix<A>::ArgsType a1;
    typename Matrix<A>::ArgsType a2;

    for (int i = 0 ; i < nrows ; i++) {
      for (int j = i+1 ; j < ncols ; j++) {
        a1 << m(j,i);
        a2 << m(i,j);
      }
    }

    typename Matrix<A>::ArgsType aboth;
    aboth << a1;
    aboth << a2;
    return VariableSequenceSymmetry(aboth, a1.size());
  }
}

// STATISTICS: minimodel-any
