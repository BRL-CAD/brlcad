%
%  Main authors:
%     Guido Tack <tack@gecode.org>
%
%  Copyright:
%     Guido Tack, 2010
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

include "bin_packing_load.mzn";

predicate bin_packing_capa(array[int] of int: c,
                           array[int] of var int: bin,
                           array[int] of int: w) =
    let {
        array[min(index_set(c))..max(index_set(c))] of var 0..ub_array(c): l
    } in (
        forall( i in index_set(l) ) ( l[i] <= c[i] )
    /\  bin_packing_load(l,bin,w)
    );
