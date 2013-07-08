# Copyright (c) 2011, Thomas Paviot (tpaviot@gmail.com)
# All rights reserved.

# This file is part StepClassLibrary (SCL).
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

import sys
import unittest

from SCL.SimpleDataTypes import *
from SCL.TypeChecker import *
from SCL.ConstructedDataTypes import *
from SCL.AggregationDataTypes import *

#
# Simple data types
#
class TestINTEGER(unittest.TestCase):
    '''
    INTEGER test
    '''
    def test_create_from_int(self):
        a = INTEGER(1)
        self.assertEqual(a,1)
        b = REAL(0)
        self.assertEqual(b,0)
        
    def test_create_from_string(self):
        a = REAL("10")
        self.assertEqual(a,10)
        b = REAL("0")
        self.assertEqual(b,0)
    
    def test_type(self):
        a = INTEGER(5)
        self.assertTrue(type(a) == INTEGER)
    
    def test_inherit_from_NUMBER(self):
        a = INTEGER(6)
        self.assertTrue(isinstance(a,INTEGER))
        self.assertTrue(isinstance(a,NUMBER))

    def test_INTEGER_ops(self):
        a = INTEGER(2)
        b = INTEGER(3)
        c = a*b
        self.assertEqual(c,6)
        self.assertTrue(type(c) == int)
    
    def test_create_from_string_exception(self):
        '''
        INT cannot be constructed from an ascii string
        '''
        try:
            INTEGER("c")
        except ValueError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestREAL(unittest.TestCase):
    '''
    REAL test
    '''
    def test_create_from_float(self):
        a = REAL(1.5)
        self.assertEqual(a,1.5)
        b = REAL(0)
        self.assertEqual(b,0)
        
    def test_create_from_string(self):
        a = REAL("1.5")
        self.assertEqual(a,1.5)
        b = REAL("0")
        self.assertEqual(b,0)
    
    def test_type(self):
        a = REAL(5)
        self.assertTrue(type(a) == REAL)
    
    def test_inherit_from_NUMBER(self):
        a = REAL(4.5)
        self.assertTrue(isinstance(a,REAL))
        self.assertTrue(isinstance(a,NUMBER))

    def test_REAL_ops(self):
        a = REAL(1.5)
        b = REAL(2)
        c = a*b
        self.assertEqual(c,3)
        self.assertTrue(type(c) == float)
    
    def test_REAL_INTEGER_ops(self):
        a = REAL(5.5)
        b = INTEGER(3)
        self.assertEqual(a+b,8.5)
        self.assertTrue(type(a+b) == float)
        
    def test_create_from_string_exception(self):
        '''
        REAL cannot be constructed from an ascii string
        '''
        try:
            REAL("c")
        except ValueError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestBOOLEAN(unittest.TestCase):
    '''
    BOOLEAN test
    '''
    def test_create_from_bool(self):
        a = BOOLEAN(True)
        self.assertTrue(a)
        b = BOOLEAN(False)
        self.assertFalse(b)
        
#
# AggregationDataType
#
class TestARRAY(unittest.TestCase):
    '''
    ARRAY test
    '''
    def test_create_array(self):
        ARRAY(1,7,REAL)
        #upper and lower bounds can be negative
        ARRAY(-1,5,INTEGER)
        ARRAY(-4,-3,INTEGER)
        # they even can be both 0
        ARRAY(0,0,REAL)
        ARRAY(1,1,BOOLEAN)
        # lower bound should be less or equal than upper bound
        try:
            ARRAY(3,2,REAL)
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
    
    def test_create_array_from_type_string(self):
        # the scope is the current module
        scp = sys.modules[__name__]
        a = ARRAY(1,7,'REAL',scope = scp)
        a[2] = REAL(2.3)

    def test_array_bounds(self):
        a = ARRAY(3,8,REAL)
        try:
            a[2]
        except IndexError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        try:
            a[9]
        except IndexError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_array_unique(self):
        # if UNIQUE is not set to True (False by default),
        # the array may contain the same instance at different
        # positions
        a = ARRAY(1,4,REAL)
        a[3] = REAL(4)
        a[4] = REAL(4)
        # however, if UNIQUE, then every instances in the 
        # array must be different
        a = ARRAY(1,4,REAL,UNIQUE=True)
        a[3] = REAL(4)
        try:
            a[4] = REAL(4)
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
    
    def test_array_optional(self):
        # if OPTIONAL is not set explicitely to True
        # then each value must be set
        a = ARRAY(1,3,REAL)
        try:
            a[1]
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        # if OPTIONAL is set to True, then values
        # can be indeterminated
        b = ARRAY(1,3,REAL,OPTIONAL=True)
        b[2] = REAL(5)
        b[3] = REAL(5)
    
    def test_array_of_array_of_real(self):
        '''
        create a 3*3 identify matrix
        '''
        my_matrix = ARRAY(1,3,ARRAY(1,3,REAL))
        my_matrix[1] = ARRAY(1,3,REAL)
        my_matrix[2] = ARRAY(1,3,REAL)
        my_matrix[3] = ARRAY(1,3,REAL)
        my_matrix[1][1] = REAL(1.0)
        my_matrix[2][2] = REAL(1.0)
        my_matrix[3][3] = REAL(1.0)
        my_matrix[1][2] = my_matrix[1][2] = REAL(0.0)
        my_matrix[1][3] = my_matrix[3][1] = REAL(0.0)
        my_matrix[2][3] = my_matrix[3][2] = REAL(0.0)

class TestLIST(unittest.TestCase):
    '''
    LIST test
    '''
    def test_create_bounded_list(self):
        LIST(1,7,REAL)
        #upper and lower bounds can be negative
        LIST(1,5,INTEGER)
        # they even can be both 0
        LIST(0,0,REAL)
        LIST(1,1,BOOLEAN)
        # lower bound should be less or equal than upper bound
        try:
            LIST(3,2,REAL)
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        # lower bound should be greater or equal than zero
        try:
            LIST(-1,2,REAL)
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_create_unbounded_list(self):
        a = LIST(0,None,REAL)
        a[6] = REAL(6)
        self.assertEqual(a[6],6)
        b = LIST(10,None,REAL)
        a[10] = REAL(7)
        self.assertEqual(a[10],7)

    def test_list_bounds(self):
        a = LIST(3,8,REAL)
        try:
            a[2]
        except IndexError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        try:
            a[9]
        except IndexError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_list_unique(self):
        # if UNIQUE is not set to True (False by default),
        # the array may contain the same instance at different
        # positions
        a = LIST(1,4,REAL)
        a[3] = REAL(4)
        a[4] = REAL(4)
        # however, if UNIQUE, then every instances in the 
        # array must be different
        a = LIST(1,4,REAL,UNIQUE=True)
        a[3] = REAL(4)
        try:
            a[3] = REAL(4)
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
    
class TestBAG(unittest.TestCase):
    '''
    BAG test
    '''
    def test_create_bounded_bag(self):
        BAG(1,7,REAL)
        BAG(1,5,INTEGER)
        BAG(0,0,REAL)
    
    def test_create_unbounded_bag(self):
        a = BAG(0,None,REAL)
    
    def test_fill_bounded_bag(self):
        b = BAG(1,3,REAL)
        b.add(REAL(1.0))
        b.add(REAL(2.0))
        b.add(REAL(3.0))
        # the bag is full, trying to add other item
        # will raise an exception
        try:
            b.add(REAL(4.0))
        except AssertionError:
             pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_fill_unbounded_bag(self):
        '''
        Fill an unbounded bag with one thousand reals
        This should not raise any exception
        '''
        b = BAG(0,None,REAL)
        for i in range(1000):
            b.add(REAL(1.0))

class TestSET(unittest.TestCase):
    '''
    SET test
    '''
    def test_create_bounded_set(self):
        SET(1,7,REAL)
        SET(1,5,INTEGER)
        SET(0,0,REAL)

    def test_create_unbounded_set(self):
        a = SET(0,None,REAL)

    def test_fill_bounded_set(self):
        b = SET(1,3,REAL)
        b.add(REAL(1.0))
        b.add(REAL(2.0))
        b.add(REAL(3.0))
        # the bag is full, trying to add other item
        # will raise an exception
        try:
            b.add(REAL(4.0))
        except AssertionError:
             pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_fill_bounded_set(self):
        # create a set with one value allowed
        c = SET(0,0,REAL)
        # fill in with the same value n times
        # the total size of the set should not increase
        for i in range(1000):
            c.add(REAL(1.0))
# 
# Constructed Data Types
#
class TestENUMERATION(unittest.TestCase):
    def test_simple_enum(self):
        scp = sys.modules[__name__]
        ahead_or_behind = ENUMERATION('ahead','behind',scope=scp)
        check_type(ahead,ahead_or_behind)
        check_type(behind,ahead_or_behind)
        try:
            check_type("some string",ahead_or_behind)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class ob1(object):
    pass
class ob2(object):
    pass
class TestSELECT(unittest.TestCase):
    def test_select(self):
        scp = sys.modules[__name__]
        select_typedef = SELECT('ob1','ob2',scope = scp)
        ob1_instance = ob1()
        ob2_instance = ob2()
        # test that line_instance is in enum type
        check_type(ob1_instance, select_typedef)
        check_type(ob2_instance,select_typedef)
        # this one should raise an exception:
        try:
            check_type(REAL(3.4),select_typedef)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

def suite():
   suite = unittest.TestSuite()
   suite.addTest(unittest.makeSuite(TestINTEGER))
   suite.addTest(unittest.makeSuite(TestREAL))
   suite.addTest(unittest.makeSuite(TestARRAY))
   suite.addTest(unittest.makeSuite(TestLIST))
   suite.addTest(unittest.makeSuite(TestBAG))
   suite.addTest(unittest.makeSuite(TestSET))
   suite.addTest(unittest.makeSuite(TestENUMERATION))
   suite.addTest(unittest.makeSuite(TestSELECT))
   return suite

if __name__ == '__main__':
    unittest.main()


