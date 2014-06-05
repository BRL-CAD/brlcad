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

import unittest
import sys
sys.path.append('../examples/unitary_schemas')

from SCL.SCLBase import *
from SCL.SimpleDataTypes import *
from SCL.ConstructedDataTypes import *
from SCL.AggregationDataTypes import *
from SCL.TypeChecker import check_type
from SCL.Expr import *

class TestSelectDataType(unittest.TestCase):
    '''
    unitary_schemas/test_select_data_type.py
    '''
    def test_import_schema_module(self):
        import test_select_data_type
    
    def test_schema(self):
        import test_select_data_type
        my_glue = test_select_data_type.glue(STRING("comp"),STRING("solvent"))
        wm = test_select_data_type.wall_mounting(STRING("mounting"),STRING("on"),my_glue)

class TestSingleInheritance(unittest.TestCase):
    '''
    unitary_schemas/test_single_inheritance.py, generated from:
    SCHEMA test_single_inheritance;

    TYPE label = STRING;
    END_TYPE;

    TYPE length_measure = REAL;
    END_TYPE;

    TYPE point = REAL;
    END_TYPE;

    ENTITY shape
    SUPERTYPE OF (rectangle);
        item_name : label;
        number_of_sides : INTEGER;
    END_ENTITY;

    ENTITY rectangle
    SUBTYPE OF (shape);
        height : length_measure;
        width : length_measure;
    END_ENTITY;

    END_SCHEMA;
    '''
    def test_import_schema_module(self):
        import test_single_inheritance
    
    def test_schema(self):
        import test_single_inheritance
        my_base_shape = test_single_inheritance.shape(test_single_inheritance.label("spherical shape"),INTEGER(1))
        my_shape = test_single_inheritance.rectangle(test_single_inheritance.label("rect"),INTEGER(6),test_single_inheritance.length_measure(30.0),test_single_inheritance.length_measure(45.))


class TestSingleInheritanceMultiLevel(unittest.TestCase):
    '''
    unitary_schemas/test_single_inheritance_multi_level.py, generated from schema:
    SCHEMA test_single_inheritance_multi_level;

    TYPE label = STRING;
    END_TYPE;

    TYPE length_measure = REAL;
    END_TYPE;

    TYPE point = REAL;
    END_TYPE;

    ENTITY shape
    SUPERTYPE OF (subshape);
        item_name : label;
        number_of_sides : INTEGER;
    END_ENTITY;

    ENTITY subshape
    SUPERTYPE OF (rectangle)
    SUBTYPE OF (shape);
    END_ENTITY;

    ENTITY rectangle
    SUBTYPE OF (subshape);
        height : length_measure;
        width : length_measure;
    END_ENTITY;

    END_SCHEMA;
    '''
    def test_import_schema_module(self):
        import test_single_inheritance_multi_level
    
    def test_schema(self):
        import test_single_inheritance_multi_level
        my_base_shape = test_single_inheritance_multi_level.shape(test_single_inheritance_multi_level.label("spherical shape"),INTEGER(1))
        my_shape = test_single_inheritance_multi_level.rectangle(test_single_inheritance_multi_level.label("rect"),\
                                                                INTEGER(6),\
                                                                test_single_inheritance_multi_level.length_measure(30.0),\
                                                                test_single_inheritance_multi_level.length_measure(45.))


class TestEnumEntityName(unittest.TestCase):
    '''
    unitary_schemas/test_enum_entity_name.py, generated from schema:
    SCHEMA test_enum_entity_name;

    TYPE simple_datum_reference_modifier = ENUMERATION OF (
       line,
       translation);
    END_TYPE;

    ENTITY line;
    END_ENTITY;

    END_SCHEMA;
    '''
    def test_import_schema_module(self):
        import test_enum_entity_name
    
    def test_schema(self):
        import test_enum_entity_name
        check_type(test_enum_entity_name.simple_datum_reference_modifier.line,test_enum_entity_name.simple_datum_reference_modifier)
        check_type(test_enum_entity_name.translation,test_enum_entity_name.simple_datum_reference_modifier)
        # if line is passed, should raise an error since it is an entity name
        try:
            check_type(test_enum_entity_name.line,test_enum_entity_name.simple_datum_reference_modifier)      
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        
        
class TestArray(unittest.TestCase):
    '''
    unitary_schemas/test_array.py, generated from schema:
    SCHEMA test_array;

    ENTITY point;
        coords : ARRAY[1:3] OF REAL;
    END_ENTITY;

    END_SCHEMA;
    '''
    def test_import_schema_module(self):
        import test_array
    
    def test_schema(self):
        import test_array
        scp = sys.modules[__name__]
        my_coords = ARRAY(1,3,REAL)
        my_point = test_array.point(my_coords)
        # following cases should raise error
        # 1. passed LIST whereas ARRAY expected
        my_coords = LIST(1,3,REAL)
        try:
            my_point = test_array.point(my_coords)      
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        # 2. passed ARRAY OF INTEGER whereas ARRAY OF REAL expected
        my_coords = ARRAY(1,3,INTEGER)
        try:
            my_point = test_array.point(my_coords)      
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        # 3. passed UNIQUE= True whereas False expected
        my_coords = ARRAY(1,3,REAL, UNIQUE=True)
        try:
            my_point = test_array.point(my_coords)      
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        # 4. passed OPTIONAL= True whereas False expected
        my_coords = ARRAY(1,3,REAL, OPTIONAL=True)
        try:
            my_point = test_array.point(my_coords)      
        except TypeError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
                  
class TestArrayOfArrayOfReal(unittest.TestCase):
    '''
    unitary_schemas/test_array_of_array_of_real.py, generated from schema:
    SCHEMA test_array_of_array_of_simple_types;

    ENTITY transformation;
        rotation : ARRAY[1:3] OF ARRAY[1:3] OF REAL;
    END_ENTITY;

    END_SCHEMA;

    '''
    def test_import_schema_module(self):
        import test_array_of_array_of_simple_types

    def test_schema(self):
        import test_array_of_array_of_simple_types
        scp = sys.modules[__name__]
        # first build the double array my_matrix
        my_matrix = ARRAY(1,3,ARRAY(1,3,REAL))
        my_matrix[1] = ARRAY(1,3,REAL)
        my_matrix[2] = ARRAY(1,3,REAL)
        my_matrix[3] = ARRAY(1,3,REAL)
        my_matrix[1][1] = REAL(1.0)
        my_matrix[2][2] = REAL(1.0)
        my_matrix[3][3] = REAL(1.0)
        # create a new 'transformation' instance
        trsf = test_array_of_array_of_simple_types.transformation(my_matrix)

class TestDefinedTypesWhereRule(unittest.TestCase):
    '''
    unitary_schemas/test_where_rule.py, generated from schema:
    SCHEMA test_where_rule;

    TYPE positive = INTEGER;
    WHERE
    notnegative : SELF > 0;
    END_TYPE;

    TYPE month = INTEGER;
    WHERE
    SELF<=12;
    SELF>=1;
    END_TYPE;

    END_SCHEMA;
    '''
    def test_import_schema_module(self):
        import test_where_rule
    
    def test_rule_notnegative(self):
        import test_where_rule
        my_p = test_where_rule.positive(30)
        try:
            my_p = test_where_rule.positive(0)      
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
    
    def test_month(self):
        import test_where_rule
        m = test_where_rule.month(11)
        try:
            m = test_where_rule.month(0)      
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        try:
            m = test_where_rule.month(13)      
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')

class TestFunctions(unittest.TestCase):
    '''
    unitary_schemas/test_function.py, generated from schema:
    '''
    def test_import_schema_module(self):
        import test_function
    
    def test_add_function(self):
        '''
        FUNCTION add(r1:REAL;r2:REAL):REAL;
        LOCAL
        result : REAL;
        END_LOCAL;
        result := r1+r2;
        RETURN(result);
        END_FUNCTION;
        '''
        from test_function import add
        a = REAL(1.0)
        b = REAL(2.0)
        c = add(a,b)
        self.assertEqual(c,REAL(3.0))
    
    def test_pow_n_function(self):
        '''
        FUNCTION pow_n(r1:REAL;n:INTEGER):REAL;
        LOCAL
        result : REAL;
        END_LOCAL;
        IF n = 0 THEN
          RETURN(1);
        ELSE
          result := r1;
          REPEAT i := 1 TO n;
            result := result * r1;
          END_REPEAT;
          RETURN(result);
        END_IF;
        END_FUNCTION;
        '''
        from test_function import pow_n
        a = REAL(10)
        b = INTEGER(3)
        c = pow_n(a,b)
        self.assertEqual(c,REAL(1000))
        self.assertEqual(pow_n(REAL(10),INTEGER(0)),REAL(1.))
    
    def test_case_1_function(self):
        '''
        FUNCTION case_1(a:INTEGER):REAL;
        LOCAL
        x : REAL;
        END_LOCAL;
        CASE a OF
        1 : x := SIN(a) ;
        2 : x := EXP(a) ;
        3 : x := SQRT(a) ; 
        4, 5 : x := LOG(a) ;
        OTHERWISE : x := 0.0 ;
        END_CASE ;
        RETURN(x);
        END_FUNCTION;
        '''
        from test_function import case_1
        from math import sin,exp,sqrt,log
        self.assertEqual(case_1(INTEGER(1)),sin(1.))
        self.assertEqual(case_1(INTEGER(2)),exp(2.))
        self.assertEqual(case_1(INTEGER(3)),sqrt(3.))
        self.assertEqual(case_1(INTEGER(4)),log(4.))
        self.assertEqual(case_1(INTEGER(5)),log(5.))
        self.assertEqual(case_1(INTEGER(6)),0.)

class TestEntityWhereRules(unittest.TestCase):
    '''
    unitary/test_entity_where_rule
    '''
    def test_import_schema(self):
        import test_entity_where_rule
        
    def test_entity_unit_vector(self):
        '''
        ENTITY unit_vector;
          a, b, c : REAL;
        WHERE
          length_1 : a**2 + b**2 + c**2 = 1.0;
        END_ENTITY;
        '''
        from test_entity_where_rule import unit_vector
        u = unit_vector(REAL(1),REAL(0),REAL(0))
        self.assertTrue(u.length_1())
        v = unit_vector(REAL(0),REAL(0),REAL(0.5))
        
    def test_entity_address(self):
        '''
        TYPE label = STRING;
        END_TYPE; -- label

        ENTITY address;
          internal_location       : OPTIONAL label;
          street_number           : OPTIONAL label;
          street                  : OPTIONAL label;
          postal_box              : OPTIONAL label;
          town                    : OPTIONAL label;
          region                  : OPTIONAL label;
          postal_code             : OPTIONAL label;
          country                 : OPTIONAL label;
          facsimile_number        : OPTIONAL label;
          telephone_number        : OPTIONAL label;
          electronic_mail_address : OPTIONAL label;
          telex_number            : OPTIONAL label;
        WHERE
          WR1: EXISTS(internal_location)       OR
               EXISTS(street_number)           OR
               EXISTS(street)                  OR
               EXISTS(postal_box)              OR
               EXISTS(town)                    OR
               EXISTS(region)                  OR
               EXISTS(postal_code)             OR
               EXISTS(country)                 OR
               EXISTS(facsimile_number)        OR
               EXISTS(telephone_number)        OR
               EXISTS(electronic_mail_address) OR
               EXISTS(telex_number);
        END_ENTITY; -- address
        '''
        from test_entity_where_rule import address, label
        a = address(None,None,None,None,None,None,None,None,None,None,None,None)
        # wr1 should raise an issue since a.wr1()
        try:
            a.wr1()    
        except AssertionError:
            pass
        except e:
            self.fail('Unexpected exception thrown:', e)
        else:
            self.fail('ExpectedException not thrown')
        a.town = label('Paris')
        a.wr1()
        
def suite():
   suite = unittest.TestSuite()
   suite.addTest(unittest.makeSuite(TestSelectDataType))
   suite.addTest(unittest.makeSuite(TestSingleInheritance))
   suite.addTest(unittest.makeSuite(TestSingleInheritanceMultiLevel))
   suite.addTest(unittest.makeSuite(TestEnumEntityName))
   suite.addTest(unittest.makeSuite(TestDefinedTypesWhereRule))
   suite.addTest(unittest.makeSuite(TestFunctions))
   suite.addTest(unittest.makeSuite(TestEntityWhereRules))
   return suite

if __name__ == '__main__':
    unittest.main()


