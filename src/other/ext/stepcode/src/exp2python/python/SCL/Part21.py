#
# STEP Part 21 Parser
#
# Copyright (c) 2011, Thomas Paviot (tpaviot@gmail.com)
# Copyright (c) 2014, Christopher HORLER (cshorler@googlemail.com)
#
# All rights reserved.
#
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

import logging

import ply.lex as lex
import ply.yacc as yacc
from ply.lex import LexError

logger = logging.getLogger(__name__)

# ensure Python 2.6 compatibility
if not hasattr(logging, 'NullHandler'):
    class NullHandler(logging.Handler):
        def handle(self, record):
            pass
        def emit(self, record):
            pass
        def createLock(self):
            self.lock = None
        
    setattr(logging, 'NullHandler', NullHandler)
        
logger.addHandler(logging.NullHandler())

####################################################################################################
# Common Code for Lexer / Parser
####################################################################################################
base_tokens = ['INTEGER', 'REAL', 'USER_DEFINED_KEYWORD', 'STANDARD_KEYWORD', 'STRING', 'BINARY',
               'ENTITY_INSTANCE_NAME', 'ENUMERATION', 'PART21_END', 'PART21_START', 'HEADER_SEC',
               'ENDSEC', 'DATA']

####################################################################################################
# Lexer 
####################################################################################################
class Lexer(object):
    tokens = list(base_tokens)
    states = (('slurp', 'exclusive'),)
        
    def __init__(self, debug=0, optimize=0, compatibility_mode=False, header_limit=4096):
        self.base_tokens = list(base_tokens)
        self.schema_dict = {}
        self.active_schema = {}
        self.input_length = 0
        self.compatibility_mode = compatibility_mode
        self.header_limit = header_limit
        self.lexer = lex.lex(module=self, debug=debug, debuglog=logger, optimize=optimize,
                             errorlog=logger)
        self.reset()

    def __getattr__(self, name):
        if name == 'lineno':
            return self.lexer.lineno
        elif name == 'lexpos':
            return self.lexer.lexpos
        else:
            raise AttributeError

    def input(self, s):
        self.lexer.input(s)
        self.input_length += len(s)

    def reset(self):
        self.lexer.lineno = 1
        self.lexer.begin('slurp')
        
    def token(self):
        try:
            return next(self.lexer)
        except StopIteration:
            return None

    def activate_schema(self, schema_name):
        if schema_name in self.schema_dict:
            self.active_schema = self.schema_dict[schema_name]
        else:
            raise ValueError('schema not registered')

    def register_schema(self, schema_name, entities):
        if schema_name in self.schema_dict:
            raise ValueError('schema already registered')

        for k in entities:
            if k in self.base_tokens: raise ValueError('schema cannot override base_tokens')
        
        if isinstance(entities, list):
            entities = dict((k, k) for k in entities)

        self.schema_dict[schema_name] = entities
  
    def t_slurp_PART21_START(self, t):
        r'ISO-10303-21;'
        t.lexer.begin('INITIAL')
        return t
    
    def t_slurp_error(self, t):
        offset = t.value.find('\nISO-10303-21;', 0, self.header_limit)
        if offset == -1 and self.header_limit < len(t.value): # not found within header_limit
            raise LexError("Scanning error. try increasing lexer header_limit parameter",
                           "{0}...".format(t.value[0:20]))
        elif offset == -1: # not found before EOF
            t.lexer.lexpos = self.input_length
        else: # found ISO-10303-21;
            offset += 1  # also skip the \n
            t.lexer.lineno += t.value[0:offset].count('\n')
            t.lexer.skip(offset)
    
    # Comment (ignored)
    def t_COMMENT(self, t):
        r'/\*(.|\n)*?\*/'
        t.lexer.lineno += t.value.count('\n')
    
    def t_PART21_END(self, t):
        r'END-ISO-10303-21;'
        t.lexer.begin('slurp')
        return t

    def t_HEADER_SEC(self, t):
        r'HEADER;'
        return t

    def t_ENDSEC(self, t):
        r'ENDSEC;'
        return t

    # Keywords
    def t_STANDARD_KEYWORD(self, t):
        r'(?:!|)[A-Za-z_][0-9A-Za-z_]*'
        if self.compatibility_mode:
            t.value = t.value.upper()
        elif not t.value.isupper():
            raise LexError('Scanning error. Mixed/lower case keyword detected, please use compatibility_mode=True', t.value)
        
        if t.value in self.base_tokens:
            t.type = t.value
        elif t.value in self.active_schema:
            t.type = self.active_schema[t.value]
        elif t.value.startswith('!'):
            t.type = 'USER_DEFINED_KEYWORD'
        return t

    def t_newline(self, t):
        r'\n+'
        t.lexer.lineno += len(t.value)
        
    # Simple Data Types
    def t_REAL(self, t):
        r'[+-]*[0-9][0-9]*\.[0-9]*(?:E[+-]*[0-9][0-9]*)?'
        t.value = float(t.value)
        return t

    def t_INTEGER(self, t):
        r'[+-]*[0-9][0-9]*'
        t.value = int(t.value)
        return t

    def t_STRING(self, t):
        r"'(?:[][!\"*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_\\ ]|'')*'"
        t.value = t.value[1:-1]
        return t

    def t_BINARY(self, t):
        r'"[0-3][0-9A-F]*"'
        try:
            t.value = int(t.value[2:-1], base=16)
        except ValueError:
            t.value = None
        return t

    t_ENTITY_INSTANCE_NAME = r'\#[0-9]+'
    t_ENUMERATION = r'\.[A-Z_][A-Z0-9_]*\.'

    # Punctuation
    literals = '()=;,*$'

    t_ANY_ignore  = ' \t'
            

####################################################################################################
# Simple Model
####################################################################################################
class P21File:
    def __init__(self, header, *sections):
        self.header = header
        self.sections = list(*sections)

class P21Header:
    def __init__(self, file_description, file_name, file_schema):
        self.file_description = file_description
        self.file_name = file_name
        self.file_schema = file_schema
        self.extra_headers = []

class HeaderEntity:
    def __init__(self, type_name, *params):
        self.type_name = type_name
        self.params = list(params) if params else []

class Section:
    def __init__(self, entities):
        self.entities = entities

class SimpleEntity:
    def __init__(self, ref, type_name, *params):
        self.ref = ref
        self.type_name = type_name
        self.params = list(params) if params else []

class ComplexEntity:
    def __init__(self, ref, *params):
        self.ref = ref
        self.params = list(params) if params else []

class TypedParameter:
    def __init__(self, type_name, *params):
        self.type_name = type_name
        self.params = list(params) if params else None

####################################################################################################
# Parser
####################################################################################################
class Parser(object):
    tokens = list(base_tokens)
    start = 'exchange_file'
    
    def __init__(self, lexer=None, debug=0):
        self.lexer = lexer if lexer else Lexer()

        try: self.tokens = lexer.tokens
        except AttributeError: pass

        self.parser = yacc.yacc(module=self, debug=debug, debuglog=logger, errorlog=logger)
        self.reset()
    
    def parse(self, p21_data, **kwargs):
        #TODO: will probably need to change this function if the lexer is ever to support t_eof
        self.lexer.reset()
        self.lexer.input(p21_data)

        if 'debug' in kwargs:
            result = self.parser.parse(lexer=self.lexer, debug=logger,
                                       ** dict((k, v) for k, v in kwargs.iteritems() if k != 'debug'))
        else:
            result = self.parser.parse(lexer=self.lexer, **kwargs)
        return result

    def reset(self):
        self.refs = {}
        self.is_in_exchange_structure = False
        
    def p_exchange_file(self, p):
        """exchange_file : check_p21_start_token header_section data_section_list check_p21_end_token"""
        p[0] = P21File(p[2], p[3])

    def p_check_start_token(self, p):
        """check_p21_start_token : PART21_START"""
        self.is_in_exchange_structure = True
        p[0] = p[1]
    
    def p_check_end_token(self, p):
        """check_p21_end_token : PART21_END"""
        self.is_in_exchange_structure = False
        p[0] = p[1]
    
    # TODO: Specialise the first 3 header entities
    def p_header_section(self, p):
        """header_section : HEADER_SEC header_entity header_entity header_entity ENDSEC"""
        p[0] = P21Header(p[2], p[3], p[4])

    def p_header_section_with_entity_list(self, p):
        """header_section : HEADER_SEC header_entity header_entity header_entity header_entity_list ENDSEC"""
        p[0] = P21Header(p[2], p[3], p[4])
        p[0].extra_headers.extend(p[5])

    def p_header_entity(self, p):
        """header_entity : keyword '(' parameter_list ')' ';'"""
        p[0] = HeaderEntity(p[1], p[3])

    def p_check_entity_instance_name(self, p):
        """check_entity_instance_name : ENTITY_INSTANCE_NAME"""
        if p[1] in self.refs:
            logger.error('Line: {0}, SyntaxError - Duplicate Entity Instance Name: {1}'.format(p.lineno(1), p[1]))
            raise SyntaxError
        else:
            self.refs[p[1]] = None
            p[0] = p[1]

    def p_simple_entity_instance(self, p):
        """simple_entity_instance : check_entity_instance_name '=' simple_record ';'"""
        p[0] = SimpleEntity(p[1], *p[3])

    def p_entity_instance_error(self, p):
        """simple_entity_instance  : error '=' simple_record ';'
           complex_entity_instance : error '=' subsuper_record ';'"""
        pass

    def p_complex_entity_instance(self, p):
        """complex_entity_instance : check_entity_instance_name '=' subsuper_record ';'"""
        p[0] = ComplexEntity(p[1], p[3])

    def p_subsuper_record(self, p):
        """subsuper_record : '(' simple_record_list ')'"""
        p[0] = [TypedParameter(*x) for x in p[2]]

    def p_data_section_list(self, p):
        """data_section_list : data_section_list data_section
                             | data_section"""
        try: p[0] = p[1] + [p[2],]
        except IndexError: p[0] = [p[1],]

    def p_header_entity_list(self, p):
        """header_entity_list : header_entity_list header_entity
                              | header_entity"""
        try: p[0] = p[1] + [p[2],]
        except IndexError: p[0] = [p[1],]

    def p_parameter_list(self, p):
        """parameter_list : parameter_list ',' parameter
                          | parameter"""
        try: p[0] = p[1] + [p[3],]
        except IndexError: p[0] = [p[1],]

    def p_keyword(self, p):
        """keyword : USER_DEFINED_KEYWORD
                   | STANDARD_KEYWORD"""
        p[0] = p[1]

    def p_parameter_simple(self, p):
        """parameter : STRING
                     | INTEGER
                     | REAL
                     | ENTITY_INSTANCE_NAME
                     | ENUMERATION
                     | BINARY
                     | '*'
                     | '$'
                     | typed_parameter
                     | list_parameter"""
        p[0] = p[1]

    def p_list_parameter(self, p):
        """list_parameter : '(' parameter_list ')'"""
        p[0] = p[2]

    def p_typed_parameter(self, p):
        """typed_parameter : keyword '(' parameter ')'"""
        p[0] = TypedParameter(p[1], p[3])

    def p_parameter_empty_list(self, p):
        """parameter : '(' ')'"""
        p[0] = []

    def p_data_start(self, p):
        """data_start : DATA '(' parameter_list ')' ';'"""
        pass

    def p_data_start_empty(self, p):
        """data_start : DATA '(' ')' ';'
                      | DATA ';'"""
        pass

    def p_data_section(self, p):
        """data_section : data_start entity_instance_list ENDSEC""" 
        p[0] = Section(p[2])

    def p_entity_instance_list(self, p):
        """entity_instance_list : entity_instance_list entity_instance
                                | entity_instance"""
        try: p[0] = p[1] + [p[2],]
        except IndexError: p[0] = [p[1],]

    def p_entity_instance_list_empty(self, p):
        """entity_instance_list : empty"""
        p[0] = []

    def p_entity_instance(self, p):
        """entity_instance : simple_entity_instance
                           | complex_entity_instance"""
        p[0] = p[1]

        
    def p_simple_record_empty(self, p):
        """simple_record : keyword '(' ')'"""
        p[0] = (p[1], [])

    def p_simple_record_with_params(self, p):
        """simple_record : keyword '(' parameter_list ')'"""
        p[0] = (p[1], p[3])

    def p_simple_record_list(self, p):
        """simple_record_list : simple_record_list simple_record
                              | simple_record"""
        try: p[0] = p[1] + [p[2],]
        except IndexError: p[0] = [p[1],]
   
    def p_empty(self, p):
        """empty :"""
        pass

def test_debug():
    import os.path

    logging.basicConfig()
    logger.setLevel(logging.DEBUG)

    parser = Parser()
    parser.reset()
    
    logger.info("***** parser debug *****")
    p = os.path.expanduser('~/projects/src/stepcode/data/ap214e3/s1-c5-214/s1-c5-214.stp')
    with open(p, 'rU') as f:
        s = f.read()
        try:
            parser.parse(s, debug=1)
        except SystemExit:
            pass
        
    logger.info("***** finished *****")

def test():
    import os, os.path, itertools, codecs
    
    logging.basicConfig()
    logger.setLevel(logging.INFO)

    parser = Parser()
    compat_list = []

    def parse_check(p):
        logger.info("processing {0}".format(p))
        parser.reset()
        with open(p, 'rU') as f:
            iso_wrapper = codecs.EncodedFile(f, 'iso-8859-1')
            s = iso_wrapper.read()
            parser.parse(s)

    logger.info("***** standard test *****")
    for d, _, files in os.walk(os.path.expanduser('~/projects/src/stepcode')):
        for f in itertools.ifilter(lambda x: x.endswith('.stp'), files):
            p = os.path.join(d, f)
            try:
                parse_check(p)
            except LexError:
                logger.exception('Lexer issue, adding {0} to compatibility test list'.format(os.path.basename(p)))
                compat_list.append(p)

    lexer =  Lexer(compatibility_mode=True)
    parser = Parser(lexer=lexer)
    
    logger.info("***** compatibility test *****")
    for p in compat_list:
        parse_check(p)
            
    logger.info("***** finished *****")

if __name__ == '__main__':
    test()
