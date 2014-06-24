%
%  Main authors:
%     Guido Tack <tack@gecode.org>
%
%  Copyright:
%     Guido Tack, 2007
%
%  Last modified:
%     $Date$ by $Author$
%     $Revision$
%
%  This file is part of Gecode, the generic constraint
%  development environment:
%     http://www.gecode.org
%
%  Permission is hereby granted, free of charge, to any person obtaining
%  a copy of this software and associated documentation files (the
%  "Software"), to deal in the Software without restriction, including
%  without limitation the rights to use, copy, modify, merge, publish,
%  distribute, sublicense, and/or sell copies of the Software, and to
%  permit persons to whom the Software is furnished to do so, subject to
%  the following conditions:
%
%  The above copyright notice and this permission notice shall be
%  included in all copies or substantial portions of the Software.
%
%  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
%  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
%  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
%  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
%  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
%  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
%  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
%
%

% Sums over Boolean variables
predicate bool_lin_eq(array[int] of int: a, array[int] of var bool: x,
                      var int: c);
predicate bool_lin_ne(array[int] of int: a, array[int] of var bool: x,
                      var int: c);
predicate bool_lin_le(array[int] of int: a, array[int] of var bool: x,
                      var int: c);
predicate bool_lin_lt(array[int] of int: a, array[int] of var bool: x,
                      var int: c);
predicate bool_lin_ge(array[int] of int: a, array[int] of var bool: x,
                      var int: c);
predicate bool_lin_gt(array[int] of int: a, array[int] of var bool: x,
                      var int: c);

predicate bool_sum_eq(array[int] of var bool: x, var int: c) =
    bool_lin_eq([1 | i in index_set(x)],x,c);
predicate bool_sum_ne(array[int] of var bool: x, var int: c) =
    bool_lin_ne([1 | i in index_set(x)],x,c);
predicate bool_sum_le(array[int] of var bool: x, var int: c) =
    bool_lin_le([1 | i in index_set(x)],x,c);
predicate bool_sum_lt(array[int] of var bool: x, var int: c) =
    bool_lin_lt([1 | i in index_set(x)],x,c);
predicate bool_sum_ge(array[int] of var bool: x, var int: c) =
    bool_lin_ge([1 | i in index_set(x)],x,c);
predicate bool_sum_gt(array[int] of var bool: x, var int: c) =
    bool_lin_gt([1 | i in index_set(x)],x,c);

predicate float_sinh(var float: a, var float: b) =
    b == (exp(a)-exp(-a))/2.0;

predicate float_cosh(var float: a, var float: b) =
    b == (exp(a)+exp(-a))/2.0;

predicate float_tanh(var float: a, var float: b) =
    let { var float: e2a = exp(2.0*a) } in
    b == (e2a-1.0)/(e2a+1.0);

predicate float_ne_reif(var float: a, var float: b, var bool: c) =
    not c <-> a==b;
