# Copyright (c) 2011-2012, Thomas Paviot (tpaviot@gmail.com)
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
from SCL.Builtin import *

float_epsilon = 1e-8
#
# Test builtin functions
#
class TestABS(unittest.TestCase):
    def test_abs_REAL(self):
        a = REAL(-4.5)
        b = ABS(a)
        self.assertEqual(b,4.5)
        self.assertTrue(isinstance(b,REAL))
    
    def test_abs_INTEGER(self):
        a = INTEGER(-4)
        b = ABS(a)
        self.assertEqual(b,4)
        self.assertTrue(isinstance(b,INTEGER))
    
    def test_abs_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = ABS(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestACOS(unittest.TestCase):
    def test_acos_REAL(self):
        a = REAL(0.3)
        b = ACOS(a)
        self.assertTrue(isinstance(b,REAL))
    
    def test_acos_INTEGER(self):
        a = INTEGER(1)
        b = ACOS(a)
        self.assertTrue(isinstance(b,REAL))
        c = INTEGER(-1)
        d = ACOS(c)
        self.assertTrue(isinstance(b,REAL))

    def test_acos_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = ACOS(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestASIN(unittest.TestCase):
    def test_asin_REAL(self):
        a = REAL(0.3)
        b = ASIN(a)
        self.assertTrue(isinstance(b,REAL))

    def test_asin_INTEGER(self):
        a = INTEGER(1)
        b = ASIN(a)
        self.assertTrue(isinstance(b,REAL))
        c = INTEGER(-1)
        d = ASIN(c)
        self.assertTrue(isinstance(b,REAL))

    def test_asin_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = ASIN(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestATAN(unittest.TestCase):
    def test_atan_REAL_INTEGER(self):
        a = REAL(0.3)
        b = INTEGER(0.5)
        c = ATAN(a,b)
        self.assertTrue(isinstance(c,REAL))

    def test_atan_INTEGER_REAL(self):
        a = INTEGER(1)
        b = REAL(0.6)
        c = ATAN(a,b)
        self.assertTrue(isinstance(c,REAL))
    
    def test_atan_REAL_REAL(self):
        a = REAL(-5.5)
        b = REAL(3)
        c = ATAN(a,b)
        self.assertTrue(isinstance(c,REAL))

    def test_atan_INTEGER_INTEGER(self):
        a = INTEGER(450)
        b = INTEGER(34)
        c = ATAN(a,b)
        self.assertTrue(isinstance(c,REAL))
 
    def test_atan_V2_is_zero(self):
        a = REAL(5.0)
        b = REAL(0)
        c = ATAN(a,b)
        self.assertTrue(isinstance(c,REAL))
        self.assertEqual(c,PI/2)
        d = REAL(-5.0)
        e = ATAN(d,b)
        self.assertTrue(isinstance(e,REAL))
        self.assertEqual(e,-PI/2)
    
    def test_atan_V1_and_V2_are_zero(self):
        a = REAL(0.0)
        b = REAL(0.0)
        try:
            c = ATAN(a,b)
        except ValueError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

    def test_asin_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = ASIN(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestBLENGTH(unittest.TestCase):
    def test_blength_BINARY(self):
        a = BINARY('0101')
        b = BLENGTH(a)
        self.assertTrue(isinstance(b,INTEGER))
        self.assertEqual(b,4)
    
    def test_blength_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = BLENGTH(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestCOS(unittest.TestCase):
    def test_cos_REAL(self):
        a = REAL(0.56)
        b = COS(a)
        self.assertTrue(isinstance(b,REAL))
    
    def test_cos_REAL(self):
        a = INTEGER(10)
        b = COS(a)
        self.assertTrue(isinstance(b,REAL))

class TestEXISTS(unittest.TestCase):
    def test_EXISTS(self):
        a = ARRAY(1,3,REAL,OPTIONAL=True)
        a[2] = REAL(4.0)
        # a[2] is defined, so EXISTS should result True
        self.assertTrue(EXISTS(a[2]))
        # other values does not exists
        self.assertFalse(EXISTS(a[1]))
        self.assertFalse(EXISTS(a[3]))

class TestEXP(unittest.TestCase):
    def test_exp_REAL(self):
        a = REAL(0.3)
        b = EXP(a)
        self.assertTrue(isinstance(b,REAL))

    def test_exp_INTEGER(self):
        a = INTEGER(-1)
        b = EXP(a)
        self.assertTrue(isinstance(b,REAL))

    def test_exp_other_types(self):
        ''' raises an exception is none of REAL orINTEGER types are passed to ABS'''
        a = STRING("some_str")
        try:
            b = EXP(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestFORMAT(unittest.TestCase):
    def test_FORMAT(self):
        n = INTEGER(10)
        f = STRING("+7I")
        result = FORMAT(n,f)
        self.assertTrue(isinstance(result,STRING))
        # 123.456789 8.2F ' 123.46'
        n = REAL(123.456789)
        f = STRING("8.2F")
        res2 = FORMAT(n,f)
        self.assertTrue(isinstance(result,STRING))
        self.assertEqual(res2,'  123.46')
        # 123.456789 8.2E '1.23E+02'
        f = STRING("8.2E")
        res3 = FORMAT(n,f)
        self.assertTrue(isinstance(result,STRING))
        self.assertEqual(res3,'1.23E+02')
        # @TODO: complete tests

class TestHIBOUND(unittest.TestCase):
    def test_HIBOUND_bounded(self):
        a = ARRAY(-3,19,REAL)
        a[2] = REAL(3.4)
        h1 = HIBOUND(a)
        self.assertEqual(h1,19)
    
    def test_HIBOUND_unbounded(self):
        l = LIST(0,None,INTEGER)
        l[4] = INTEGER(5)
        h1 = HIBOUND(l)
        self.assertEqual(h1,None)

class TestHIINDEX(unittest.TestCase):
    def test_HIINDEX_ARRAY(self):
        a = ARRAY(-3,19,REAL)
        a[2] = REAL(6)
        h1 = HIINDEX(a)
        self.assertEqual(h1,19)
    
    def test_HIINDEX_LIST(self):
        l = LIST(0,None,REAL)
        l[3] = REAL(6)
        l[45] = REAL(7)
        h2 = HIINDEX(l)
        self.assertEqual(h2,2)
        
    def test_HIINDEX_SET(self):
        s = SET(0,None,INTEGER)
        s.add(INTEGER(3))
        s.add(INTEGER(4))
        s.add(INTEGER(4))
        h3 = HIINDEX(s)
        self.assertEqual(h3,2)
    
    def test_HIINDEX_BAG(self):
        s = BAG(0,None,STRING)
        s.add(STRING('1st'))
        s.add(STRING('2nd'))
        s.add(STRING('2nd'))
        h4 = HIINDEX(s)
        self.assertEqual(h4,3)

class TestLENGTH(unittest.TestCase):
    def test_LENGTH(self):
        x1 = STRING('abc')
        l1 = LENGTH(x1)
        self.assertTrue(isinstance(l1,INTEGER))
        self.assertEqual(l1,3)
    
    def test_LENGTH_other_types(self):
        a = REAL(12.3)
        try:
            b = LENGTH(a)
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        
class TestLOBOUND(unittest.TestCase):
    def test_HIBOUND_bounded(self):
        a = ARRAY(-3,19,REAL)
        a[2] = REAL(3.4)
        h1 = LOBOUND(a)
        self.assertEqual(h1,-3)

    def test_LOBOUND_unbounded(self):
        l = LIST(0,None,INTEGER)
        l[4] = INTEGER(5)
        h1 = LOBOUND(l)
        self.assertEqual(h1,0)

class TestLOG(unittest.TestCase):
    def test_LOG(self):
        a = REAL(10)
        log_a = LOG(a)
        self.assertTrue(isinstance(log_a,REAL))
    
    def test_LOG2(self):
        a = REAL(8)
        log2_a = LOG2(a)
        self.assertTrue(isinstance(log2_a,REAL))
        self.assertEqual(log2_a,3.)
    
    def test_LOG10(self):
        a = REAL(10.0)
        log10_a = LOG10(a)
        self.assertTrue(isinstance(log10_a,REAL))
        self.assertEqual(log10_a,1.)

class TestLOINDEX(unittest.TestCase):
    def test_LOINDEX_ARRAY(self):
        a = ARRAY(-3,19,REAL)
        a[2] = REAL(6)
        h1 = LOINDEX(a)
        self.assertEqual(h1,-3)

    def test_LOINDEX_LIST(self):
        l = LIST(0,None,REAL)
        l[3] = REAL(6)
        l[45] = REAL(7)
        h2 = LOINDEX(l)
        self.assertEqual(h2,1)

    def test_LOINDEX_SET(self):
        s = SET(0,None,INTEGER)
        s.add(INTEGER(3))
        s.add(INTEGER(4))
        s.add(INTEGER(4))
        h3 = LOINDEX(s)
        self.assertEqual(h3,1)

    def test_LOINDEX_BAG(self):
        s = BAG(0,None,STRING)
        s.add(STRING('1st'))
        s.add(STRING('2nd'))
        s.add(STRING('2nd'))
        h4 = LOINDEX(s)
        self.assertEqual(h4,1)

class TestNVL(unittest.TestCase):
    def test_NVL_1(self):
        a = ARRAY(1,4,REAL, OPTIONAL = True)
        # a[1] should be None 
        a[2] = REAL(5)
        o1 = NVL(a[1],REAL(0.0))
        self.assertEqual(o1,0.0)
        o2 = NVL(a[2],REAL(0.0))
        self.assertEqual(o2,5.)

class TestODD(unittest.TestCase):
    def test_is_ODD(self):
        a = INTEGER(121)
        self.assertTrue(ODD(a))
        
    def test_not_ODD(self):
        b = INTEGER(8)
        self.assertFalse(ODD(b))

    def test_zero_is_not_odd(self):
        c = INTEGER(0)
        self.assertFalse(ODD(c))

class TestROLESOF(unittest.TestCase):
    # @TODO: not implemented yet
    pass

class TestSIN(unittest.TestCase):
    def test_sin_REAL(self):
        a = REAL(0.56)
        b = SIN(a)
        self.assertTrue(isinstance(b,REAL))

    def test_sin_REAL(self):
        a = INTEGER(10)
        b = SIN(a)
        self.assertTrue(isinstance(b,REAL))

class TestSIZEOF(unittest.TestCase):
    def test_SIZEOF_ARRAY(self):
        a = ARRAY(-3,19,REAL)
        a[2] = REAL(6)
        s1 = SIZEOF(a)
        self.assertEqual(s1,23)

    def test_SIZEOF_LIST(self):
        l = LIST(0,None,REAL)
        l[3] = REAL(6)
        l[45] = REAL(7)
        s2 = SIZEOF(l)
        self.assertEqual(s2,2)

    def test_SIZEOF_SET(self):
        s = SET(0,None,INTEGER)
        s.add(INTEGER(3))
        s.add(INTEGER(4))
        s.add(INTEGER(4))
        s3 = SIZEOF(s)           
        self.assertEqual(s3,2)

    def test_SIZEOF_BAG(self):
        s = BAG(0,None,STRING)
        s.add(STRING('1st'))
        s.add(STRING('2nd'))
        s.add(STRING('2nd'))
        s4 = SIZEOF(s)
        self.assertEqual(s4,3)

class TestSQRT(unittest.TestCase):
    def test_SQRT_INTEGER(self):
        self.assertEqual(SQRT(INTEGER(9)),REAL(3.0))
    
    def test_SQRT_REAL(self):
        self.assertEqual(SQRT(REAL(25.0)),REAL(5.0))

class TestTAN(unittest.TestCase):
    def test_tan_REAL(self):
        a = REAL(0.0)
        b = TAN(a)
        self.assertTrue(isinstance(b,REAL))
        self.assertEqual(b,0.)

    def test_tan_n_pi_with_n_odd(self):
        for i in range(10):
            angle = REAL(((i*2+1)*PI/2)) #angle is npi/2 with n odd
            self.assertEqual(TAN(angle),None) #result should be indeterminate

    def test_tan_n_pi_with_n_not_odd(self):
        for i in range(10):
            angle = REAL(i*2*PI/2) #angle is npi/2 with n odd
            self.assertAlmostEqual(TAN(angle),0.) #result should be indeterminate

class TestTYPEOF(unittest.TestCase):
    def test_TYPEOF_REAL(self):
        a = REAL(5.6)
        self.assertEqual(TYPEOF(a),set(['REAL','NUMBER']))

class TestUSEDIN(unittest.TestCase):
    # @TODO: not implemented yet
    pass

class TestVALUE(unittest.TestCase):
    def test_VALUE_to_INTEGER(self):
        i = STRING('10')
        self.assertEqual(VALUE(i),INTEGER(10))

    def test_VALUE_to_REAL(self):
        f = STRING('1.234')
        self.assertEqual(VALUE(f),REAL(1.234))

    def test_value_to_None(self):
        s = STRING('something')
        self.assertEqual(VALUE(s),None)

class TestVALUE_IN(unittest.TestCase):
    # @TODO: not implemented yet
    pass

class TestVALUE_UNIQUE(unittest.TestCase):
    # @TODO: not implemented yet
    def test_VALUE_UNIQUE_array_1(self):
        a = ARRAY(1,3,REAL)
        a[1] = REAL(4)
        a[2] = REAL(5)
        a[3] = REAL(6)
        self.assertTrue(VALUE_UNIQUE(a))
    
    def test_VALUE_UNIQUE_array_2(self):
        a = ARRAY(1,3,REAL)
        a[1] = REAL(4)
        a[2] = REAL(4)
        a[3] = REAL(6)
        self.assertFalse(VALUE_UNIQUE(a))
    
    def test_VALUE_UNIQUE_array_3(self):
       a = ARRAY(1,3,REAL)
       self.assertEqual(VALUE_UNIQUE(a),Unknown)

def suite():
   suite = unittest.TestSuite()
   suite.addTest(unittest.makeSuite(TestABS))
   suite.addTest(unittest.makeSuite(TestACOS))
   suite.addTest(unittest.makeSuite(TestASIN))
   suite.addTest(unittest.makeSuite(TestATAN))
   suite.addTest(unittest.makeSuite(TestBLENGTH))
   suite.addTest(unittest.makeSuite(TestEXISTS))   
   suite.addTest(unittest.makeSuite(TestEXP))
   suite.addTest(unittest.makeSuite(TestFORMAT)) 
   suite.addTest(unittest.makeSuite(TestHIBOUND)) 
   suite.addTest(unittest.makeSuite(TestHIINDEX))
   suite.addTest(unittest.makeSuite(TestLENGTH)) 
   suite.addTest(unittest.makeSuite(TestLOBOUND)) 
   suite.addTest(unittest.makeSuite(TestLOG)) 
   suite.addTest(unittest.makeSuite(TestNVL))
   suite.addTest(unittest.makeSuite(TestROLESOF))
   suite.addTest(unittest.makeSuite(TestSIN))
   suite.addTest(unittest.makeSuite(TestSIZEOF))
   suite.addTest(unittest.makeSuite(TestSQRT))
   suite.addTest(unittest.makeSuite(TestTAN))
   suite.addTest(unittest.makeSuite(TestTYPEOF))
   suite.addTest(unittest.makeSuite(TestUSEDIN))
   suite.addTest(unittest.makeSuite(TestVALUE))
   suite.addTest(unittest.makeSuite(TestVALUE_IN))
   suite.addTest(unittest.makeSuite(TestVALUE_UNIQUE))
   return suite

if __name__ == '__main__':
    unittest.main()


