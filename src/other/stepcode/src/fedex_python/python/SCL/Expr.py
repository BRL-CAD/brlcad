# Copyright (c) 2011, Thomas Paviot (tpaviot@gmail.com)
# All rights reserved.

# This file is part of the StepClassLibrary (SCL).
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
#   Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
#   Neither the name of the <ORGANIZATION> nor the names of its contributors may
#   be used to endorse or promote products derived from this software without
#   specific prior written permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from math import *
PI = pi #in EXPRESS, PI is uppercase

def EvalDerivedAttribute(class_instance, str_expr):
    '''
    This functions tries to evaluate EXPRESS math expr.
    For instance in the following example:
    ==
    ENTITY circle;
      centre : point;
      radius : REAL;
      axis : vector;
    DERIVE
      area : REAL := PI*radius**2;
      perimeter : REAL := 2.0*PI*radius;

    END_ENTITY;
    ==
    The following string should be passed to EvalExpr:
    'PI*radius**2'
    '''
    # first, try to find class paramters
    # in the previous example, we should replace radius with self.radius before eval'ing expr
    props = []
    for prop in dir(class_instance):
        if not prop.startswith("_"): #means prop is an item that might be considered
            props.append(prop)
    for item in props:
        str_expr = str_expr.replace(item,"class_instance.%s"%item)
    # after that step, the expression should be:
    # PI*class_instance.radius*class_instance.radius
    # this can be evaluated with the eval function
    # CAREFUL: eval is known to be unsafe. This should be changed in the future
    # (using a parser, or simpy for instance)
    try:
        return eval(str_expr.lower())
    except:
        print 'Error evaluating expression'
        return None

