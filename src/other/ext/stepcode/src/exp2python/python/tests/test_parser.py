# Copyright (c) 2021, Devon Sparks (devonsparks.com)
# All rights reserved.

# This file is part of the STEPCODE project.
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
from stepcode.Part21 import Parser

ShapesSample ="""
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('simple example file'),'1');
FILE_NAME('testfile.step','1997-10-06T16:15:42',('long forgotten'),('nist'),'0','1','2');
FILE_SCHEMA(('example_schema'));
ENDSEC;
DATA;
#0=SQUARE('Square9',.BROWN.,13,15.,51.);
#1=CIRCLE('Circle8',.ORANGE.,19,12.);
#2=TRIANGLE('Triangle7',.BLACK.,67,84.,60.,25.);
#3=LINE(#6,#7);
#4=SHAPE('Shape4',.WHITE.,83);
#5=RECTANGLE('Rectangle8',.BROWN.,66,78.,95.);
#6=CARTESIAN_POINT(11.,67.,54.);
#7=CARTESIAN_POINT(1.,2.,3.);
ENDSEC;
END-ISO-10303-21;
"""

class TestSample(unittest.TestCase):
    def setUp(self):
        self.parser = Parser()
    def tearDown(self):
        self.parser = None


class TestShapesParse(TestSample):
    """
    Tests whether we're able to parse the shapes sample at all
    """
    def test_parse(self):
        model = self.parser.parse(ShapesSample)
        self.assertIsNotNone(model)


class TestShapesHeader(TestSample):
    """
    Test basic structure and payload of Header section
    """
    def test_header_name(self):
        model = self.parser.parse(ShapesSample)
        self.assertEqual(model.header.file_name.params[0][0], "testfile.step")

    def test_header_schema(self):
        model = self.parser.parse(ShapesSample)
        self.assertEqual(model.header.file_schema.params[0][0][0], "example_schema")


class TestShapesData(TestSample):
    """
    Test basic structure and shape of data section
    """
    def test_data_section_form(self):
        model = self.parser.parse(ShapesSample)
        self.assertEqual(len(model.sections), 1)
        self.assertEqual(len(model.sections[0].entities), 8)


class TestEntity(TestSample):
    """
    Test structure and contents of several entities within the DATA section
    """
    def test_line(self):
        model = self.parser.parse(ShapesSample)
        line = model.sections[0].entities[3]
        self.assertEqual(line.type_name, "LINE")
        self.assertEqual(line.ref, "#3")
        self.assertEqual(line.params[0], ["#6", "#7"])

    def test_rectangle8(self):
        model = self.parser.parse(ShapesSample)
        rect8 = model.sections[0].entities[5]
        self.assertEqual(rect8.type_name, "RECTANGLE")
        self.assertEqual(rect8.ref, "#5")
        self.assertEqual(rect8.params[0], ['Rectangle8', '.BROWN.', 66, 78.0, 95.0])

