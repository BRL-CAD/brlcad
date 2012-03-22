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

from SimpleDataTypes import *
from TypeChecker import *

class BaseAggregate(object):
    """ A class that define common properties to ARRAY, LIST, SET and BAG.
    """
    def __init__( self ,  bound1 , bound2 , base_type ):
        # check that bound1<bound2
        if (bound1!=None and bound2!=None):
            if bound1>bound2:
                raise AssertionError("bound1 shall be less than or equal to bound2")
        self._bound1 = bound1
        self._bound2 = bound2
        self._base_type = base_type

    def __getitem__(self, index):
        if index<self._bound1:
            raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound1,index))
        elif(self._bound2!=None and index>self._bound2):
            raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound2,index))
        else:
            return list.__getitem__(self,index)
    
    def __setitem__(self,index,value):
        if index<self._bound1:
            raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound1,index))
        elif (self._bound2!=None and index>self._bound2):
            raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound2,index))
        elif not isinstance(value,self._base_type):
            raise TypeError("%s type expected, passed %s."%(self._base_type, type(value)))
        else:
            # first find the length of the list, and extend it if ever
            # the index is
            list.__setitem__(self,index,value)

class ARRAY(object):
    """An array data type has as its domain indexed, fixed-size collections of like elements. The lower
    and upper bounds, which are integer-valued expressions, define the range of index values, and
    thus the size of each array collection.
    An array data type definition may optionally specify
    that an array value cannot contain duplicate elements.
    It may also specify that an array value
    need not contain an element at every index position.
    
    Given that m is the lower bound and n is the upper bound, there are exactly n-m+1 elements
    in the array. These elements are indexed by subscripts from m to n, inclusive (see 12.6.1).
    NOTE 1 { The bounds may be positive, negative or zero, but may not be indeterminate (?) (see
    14.2).
    """
    def __init__( self ,  bound_1 , bound_2 , base_type , UNIQUE = False, OPTIONAL=False):
        if not type(bound_1)==int:
            raise TypeError("ARRAY lower bound must be an integer")
        if not type(bound_2)==int:
            raise TypeError("ARRAY upper bound must be an integer")
        if not (bound_1 <= bound_2):
            raise AssertionError("ARRAY lower bound must be less than or equal to upper bound")
        # the base type can be either a type or a string that defines a type
        if type(base_type)==str:
            if globals().has_key(base_type):
                self._base_type = globals()[base_type]
            else:
                raise TypeError("%s does not name a type"%base_type)
        else:
            self._base_type = base_type
        # set up class attributes
        self._bound_1 = bound_1
        self._bound_2 = bound_2
        self._unique = UNIQUE
        self._optional = OPTIONAL
        # preallocate list elements
        list_size = bound_2 - bound_1 + 1
        self._container = list_size*[None]
    
    def bound_1(self):
        return self._bound_1

    def bound_2(self):
        return self._bound_2

    def __getitem__(self, index):
        if index<self._bound_1:
            raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound_1,index))
        elif(index>self._bound_2):
            raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound_2,index))
        else:
            value = self._container[index-self._bound_1]
            if not self._optional and value==None:
                raise AssertionError("Not OPTIONAL prevent the value with index %i from being None (default). Please set the value first."%index)
            return value
 
    def __setitem__(self, index, value):
        if index<self._bound_1:
            raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound_1,index))
        elif(index>self._bound_2):
            raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound_2,index))
        else:
            # first check the type of the value
            check_type(value,self._base_type)
            # then check if the value is already in the array
            if self._unique:
                if value in self._container:
                    raise AssertionError("UNIQUE keyword prevent inserting this instance.")
            self._container[index-self._bound_1] = value

class LIST(object):
    """A list data type has as its domain sequences of like elements. The optional lower and upper
    bounds, which are integer-valued expressions, define the minimum and maximum number of
    elements that can be held in the collection defined by a list data type.
    A list data type
    definition may optionally specify that a list value cannot contain duplicate elements.
    """
    def __init__( self ,  bound_1 , bound_2 , base_type , UNIQUE = False):
        if not type(bound_1)==int:
            raise TypeError("LIST lower bound must be an integer")
        # bound_2 can be set to None
        self._unbounded = False
        if bound_2 == None:
            self._unbounded = True
        elif not type(bound_2)==int:
            raise TypeError("LIST upper bound must be an integer")
        if not bound_1>=0:
            raise AssertionError("LIST lower bound must be greater of equal to 0")
        if (type(bound_2)==int and not (bound_1 <= bound_2)):
            raise AssertionError("ARRAY lower bound must be less than or equal to upper bound")
        # set up class attributes
        self._bound_1 = bound_1
        self._bound_2 = bound_2
        self._base_type = base_type
        self._unique = UNIQUE
        # preallocate list elements if bounds are both integers
        if not self._unbounded:
            list_size = bound_2 - bound_1 + 1
            self._container = list_size*[None]
        # for unbounded list, this will come after
        else:
            self._container = [None]

    def bound_1(self):
        return self._bound_1

    def bound_2(self):
        return self._bound_2

    def __getitem__(self, index):
        # case bounded
        if not self._unbounded:
            if index<self._bound_1:
                raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound_1,index))
            elif(index>self._bound_2):
                raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound_2,index))
            else:
                value = self._container[index-self._bound_1]
                if value == None:
                    raise AssertionError("Value with index %i not defined. Please set the value first."%index)
                return value
        #case unbounded
        else:
            if index-self._bound_1>len(self._container):
                raise AssertionError("Value with index %i not defined. Please set the value first."%index)
            else:
                value = self._container[index-self._bound_1]
                if value == None:
                    raise AssertionError("Value with index %i not defined. Please set the value first."%index)
                return value
 
    def __setitem__(self, index, value):
        # case bounded
        if not self._unbounded:
            if index<self._bound_1:
                raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound_1,index))
            elif(index>self._bound_2):
                raise IndexError("ARRAY index out of bound (upper bound is %i, passed %i)"%(self._bound_2,index))
            else:
                # first check the type of the value
                check_type(value,self._base_type)
                # then check if the value is already in the array
                if self._unique:
                    if value in self._container:
                        raise AssertionError("UNIQUE keyword prevent inserting this instance.")
                self._container[index-self._bound_1] = value
        # case unbounded
        else:
            if index<self._bound_1:
                raise IndexError("ARRAY index out of bound (lower bound is %i, passed %i)"%(self._bound_1,index))
            # if the _container list is of good size, just do like the bounded case
            if (index-self._bound_1<len(self._container)):
                # first check the type of the value
                check_type(value,self._base_type)
                # then check if the value is already in the array
                if self._unique:
                    if value in self._container:
                        raise AssertionError("UNIQUE keyword prevent inserting this instance.")
                self._container[index-self._bound_1] = value
            # in the other case, we have to extend the base _container list
            else:
                delta_size = (index-self._bound_1) - len(self._container) + 1
                #create a list of None, and extend the list
                list_extension = delta_size*[None]
                self._container.extend(list_extension)
                # first check the type of the value
                check_type(value,self._base_type)
                # then check if the value is already in the array
                if self._unique:
                    if value in self._container:
                        raise AssertionError("UNIQUE keyword prevent inserting this instance.")
                self._container[index-self._bound_1] = value

class BAG(tuple, BaseAggregate):
    """A bag data type has as its domain unordered collections of like elements. The optional lower
    and upper bounds, which are integer-valued expressions, dene the minimum and maximum
    number of elements that can be held in the collection dened by a bag data type.
    """
    def __init__( self ,  bound1 , bound2 , base_type ):
         BaseAggregate.__init__( self ,  bound1 , bound2 , base_type )

class SET(set, BaseAggregate):
    """A set data type has as its domain unordered collections of like elements. The set data type is
    a specialization of the bag data type. The optional lower and upper bounds, which are integer-
    valued expressions, dene the minimum and maximum number of elements that can be held in
    the collection dened by a set data type. The collection dened by set data type shall not
    contain two or more elements which are instance equal.
    """
    def __init__( self ,  bound1 , bound2 , base_type ):
         BaseAggregate.__init__( self ,  bound1 , bound2 , base_type )

if __name__=='__main__':
    # test ARRAY
    a = ARRAY(1,3,REAL)
    a[1] = REAL(3.)
    a[2] = REAL(-1.5)
    a[3] = REAL(4.)
    print a
    
    
