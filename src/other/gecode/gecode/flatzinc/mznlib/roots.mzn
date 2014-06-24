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

include "int_set_channel.mzn";

% i in z <-> exists (j in x) (i in y[j])
predicate gecode_array_set_element_union(var set of int: x,
    array[int] of var set of int: y, var set of int: z);

predicate roots(array[int] of var int: x, var set of int: s,
                                          var set of int: t) =
    assert(ub(s) subset index_set(x),
        "roots: upper bound of 's' must be a subset of the index set of 'x'",
        if (min(index_set(x)) < 0 \/ min(ub(t)) < 1) then
            % All values in 's' must map to a value in 't'.
            forall(i in ub(s)) (
                i in s -> x[i] in t
            ) /\
            % All values in 't' must be mapped from a value in 's'.
            forall(i in ub(t)) (
                i in t -> forall(j in index_set(x)) (x[j] == i -> j in s)
            )
        else let {
            array[1..max(ub(t))] of var set of 0..max(index_set(x)): y,
        } in
            int_set_channel(x,y) 
            /\
            gecode_array_set_element_union(t,y,s)
        endif
    );
