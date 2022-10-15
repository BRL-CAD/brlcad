#
# STEP Part 21 Parser
#
# Copyright (c) 2020, Christopher HORLER (cshorler@googlemail.com)
#
# All rights reserved.
#
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

import logging
import os.path, sqlite3, datetime, tempfile

import re
import ply.lex as lex
import ply.yacc as yacc
from ply.lex import LexError, TOKEN

logger = logging.getLogger(__name__)
#logger.addHandler(logging.NullHandler())

# assemble catchall regexp
p21_real = r'(?:[+-]*[0-9][0-9]*\.[0-9]*(?:E[+-]*[0-9][0-9]*)?)'
p21_integer = r'(?:[+-]*[0-9][0-9]*)'
p21_string = r"""(?x)(?:'
    (?:
         # basic string
         [][!"*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_ ]|''|\\\\|
         # \P\A, \P\B, ... --> iso8859-1, iso8859-2,... applicable to following \S\c directives
         \\P[A-I]\\|
         # page control directive \S\c
         \\S\\[][!"'*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_\\ ]|
         # hex string encodings
         \\X2\\(?:[0-9A-F]{4})+\\X0\\|\\X4\\(?:[0-9A-F]{8})+\\X0\\|
         # hex byte encoding
         \\X\\[0-9A-F]{2}
    )*
')"""
p21_binary = r'(?:"[0-3][0-9A-F]*")'
p21_enumeration = r'(?:\.[A-Z_][A-Z0-9_]*\.)'
p21_keyword = r'(?:!|)[A-Za-z_][0-9A-Za-z_]*'
p21_eid = r'\#[0-9]+'

_catchall_types = [p21_real, p21_integer, p21_string, p21_binary, p21_enumeration,
                   p21_keyword, p21_eid]

groups_re = re.compile(r"(?P<lparens>\()|(?P<eid>\#[0-9]+)|(?P<rparens>\))|(?P<string>')")
p21_string_re = re.compile(p21_string)

def _mkgroups(s):
    """used to populate the xref database table"""
    stack_idx = 0
    stack_depth = 2
    stack = [set(), set()]
    
    cp = 0
    while True:
        m = groups_re.search(s, cp)
        if not m: break
    
        if m.group('eid'):
            stack[stack_idx].add(m.group())
        elif m.group('lparens'):
            stack_idx += 1
            if stack_idx == len(stack):
                stack_depth += 2
                stack.extend((set(), set()))
        elif m.group('rparens'):
            stack_idx -= 1
        else:
            m = p21_string_re.match(s, m.start())
            
        cp = m.end()

    groups = []
    if any(stack):
        _stack = filter(bool, stack)
        # expand the first level
        stack = tuple((x,) for x in next(_stack)) + tuple(_stack)
        groups = list(enumerate(stack, 1))
        
    return groups


base_tokens = ['PART21_START', 'PART21_END', 'HEADER', 'DATA', 'ENDSEC',
               'INTEGER', 'REAL', 'KEYWORD', 'STRING', 'BINARY', 'ENUMERATION',
               'EID', 'RAW']

####################################################################################################
# Lexer 
####################################################################################################
class Lexer(object):
    tokens = list(base_tokens)
    literals = '()=;,*$'

    states = (('slurp', 'exclusive'),
              ('header', 'exclusive'),
              ('data', 'exclusive'),
              ('raw', 'exclusive'),
              ('params', 'exclusive'))

    def __init__(self, debug=False, optimize=False, header_limit=4096):
        self.base_tokens = list(base_tokens)
        self.schema_dict = {}
        self.active_schema = {}
        self.header_limit = header_limit
        self.lexer = lex.lex(module=self, debug=debug, optimize=optimize, lextab='cl21tab',
                             debuglog=logger, errorlog=logger)
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

    def reset(self):
        self.lexer.lineno = 1
        self.lexer.lvl = 0
        self.lexer.begin('slurp')
        
    def token(self):
        return self.lexer.token()

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

    def t_slurp_error(self, t):
        m = re.search(r'(?P<comment>/\*)|(ISO-10303-21;)', t.value[:self.header_limit])
        if m:
            if m.group('comment'):
                t.lexer.skip(m.start())
            else:
                t.type = 'PART21_START'
                t.value = m.group()
                t.lexpos += m.start()
                t.lineno += t.value[:m.start()].count('\n')
                t.lexer.lexpos += m.end()
                t.lexer.begin('INITIAL')
                return t
        elif len(t.value) < self.header_limit:
            t.lexer.skip(len(t.value))
        else:
            raise LexError("Scanning error. try increasing lexer header_limit parameter",
                           "{0}...".format(t.value[0:20]))
            
    def t_error(self, t):
        raise LexError("Scanning error, invalid input", "{0}...".format(t.value[0:20]))
    
    def t_ANY_COMMENT(self, t):
        r'/\*(?:.|\n)*?\*/'
        t.lexer.lineno += t.value.count('\n')


    def t_PART21_END(self, t):
        r'END-ISO-10303-21;'
        self.lexer.lvl = 0
        self.lexer.begin('slurp')        
        return t
    def t_HEADER(self, t):
        r'HEADER;'
        t.lexer.push_state('header')
        return t
    def t_header_data_ENDSEC(self, t):
        r'ENDSEC;'
        t.lexer.pop_state()
        return t
    def t_DATA(self, t):
        r'DATA\b'
        t.lexer.in_header = True
        t.lexer.push_state('data')
        return t


    @TOKEN(p21_keyword)
    def t_header_KEYWORD(self, t):
        return t
    def t_header_lparens(self, t):
        r'\('
        t.lexer.lexpos -= 1
        t.lexer.push_state('params')
    def t_header_rparens(self, t):
        r'\)'
        t.type = ')'
        t.lexer.pop_state()
        return t


    def t_data_lparens(self, t):
        r'\('
        if t.lexer.in_header:
            t.type = '('
            t.lexer.push_state('header')
        else:
            t.type = 'RAW'
            t.lexer.push_state('raw')
        return t
    def t_data_header_end(self, t):
        r';'
        t.type = ';'
        t.lexer.in_header = False
        return t
    @TOKEN(p21_eid)
    def t_data_EID(self, t):
        return t
    @TOKEN(p21_keyword)
    def t_data_KEYWORD(self, t):
        t.lexer.push_state('raw')
        return t


    def t_params_lparens(self, t):
        r'\('
        t.type = 'RAW'
        t.lexer.lvl += 1
        return t
    def t_params_rparens(self, t):
        r'\)'
        t.type = 'RAW'
        t.lexer.lvl -= 1
        if t.lexer.lvl == 0:
            t.lexer.pop_state()
        return t
    @TOKEN('(?:' + '|'.join(_catchall_types) + r'|(?:[,*$]))+')
    def t_params_RAW(self, t):
        return t
    

    def t_raw_end(self, t):
        r';'
        t.lexer.pop_state()
        t.type = ';'
        return t
    @TOKEN('(?:' + '|'.join(_catchall_types) + r'|(?:[(),*$]))+')
    def t_raw_RAW(self, t):
        return t


    def t_ANY_newline(self, t):
        r'\n+'
        t.lexer.lineno += len(t.value)    
    t_ANY_ignore = ' \t\r'


####################################################################################################
# Parser
####################################################################################################
class Parser(object):
    tokens = list(base_tokens)
    
    def __init__(self, lexer=None, debug=False, tabmodule=None, start=None, optimize=False,
                 tempdb=False):
        # defaults
        start_tabs = {'exchange_file': 'cp21tab', 'extract_header': 'cp21hdrtab'}
        if start and tabmodule: start_tabs[start] = tabmodule
        if not start: start = 'exchange_file'
        if start not in start_tabs: raise ValueError('please pass (dedicated) tabmodule')

        self.tempdb = tempdb
        self.lexer = lexer if lexer else Lexer()
        self.parser = yacc.yacc(debug=debug, module=self, tabmodule=start_tabs[start], start=start,
                                optimize=optimize, debuglog=logger, errorlog=logger)
    
    def parse(self, p21_data, db_path=None, **kwargs):
        #TODO: will probably need to change this function if the lexer is ever to support t_eof
        self.reset(self.tempdb, db_path)
        self.lexer.input(p21_data)

        if 'debug' in kwargs:
            result = self.parser.parse(lexer=self.lexer, debug=logger,
                                       ** dict((k, v) for k, v in kwargs.items() if k != 'debug'))
        else:
            result = self.parser.parse(lexer=self.lexer, **kwargs)
        return result

    def reset(self, tempdb=None, db_path=None):
        self.lexer.reset()
        self.initdb(tempdb, db_path)

    def closedb(self):
        try:
            self.db_cxn.commit()
            self.db_cxn.close()
        except AttributeError:
            pass
        
    def initdb(self, tempdb, db_path=None):
        if tempdb and not db_path:
            tm = datetime.datetime.utcnow().isoformat(timespec='seconds').replace(':','-')
            db_path = os.path.join(tempfile.mkdtemp(), tm + '_test.db')
        elif not db_path:
            db_path = ":memory:"
        logger.info('db_path: %s', db_path)
        self.db_cxn = sqlite3.connect(db_path)
        self.db_writer = self.db_cxn.cursor()
        self.db_writer.executescript("""
            PRAGMA foreign_keys = ON;
            CREATE TABLE entity_enum (type TEXT(1) PRIMARY KEY);
            INSERT INTO entity_enum (type) VALUES ('S'), ('C');
            CREATE TABLE section_enum (type TEXT(1) PRIMARY KEY);
            INSERT INTO section_enum (type) VALUES ('D'), ('H');

            CREATE TABLE section_table (
                id INTEGER PRIMARY KEY,
                lineno INTEGER NOT NULL,
                section_type TEXT(1) NOT NULL REFERENCES section_enum(type)
            );
            
            CREATE TABLE section_headers (
                id INTEGER PRIMARY KEY,
                type_name TEXT COLLATE NOCASE,
                raw_data TEXT NOT NULL,
                lineno INTEGER NOT NULL,
                fk_section INTEGER NOT NULL REFERENCES section_table(id)
            );
            
            CREATE TABLE data_table (
                id TEXT PRIMARY KEY,
                type_name TEXT COLLATE NOCASE,
                raw_data TEXT NOT NULL,
                lineno INTEGER NOT NULL,
                entity_type TEXT(1) NOT NULL REFERENCES entity_enum(type),
                fk_section INTEGER NOT NULL REFERENCES section_table(id)
            ) WITHOUT ROWID;
            
            CREATE TABLE data_xref (
                id_from TEXT NOT NULL REFERENCES data_table(id) DEFERRABLE INITIALLY DEFERRED,
                id_to TEXT NOT NULL REFERENCES data_table(id),
                id_group INTEGER NOT NULL,
                PRIMARY KEY (id_from, id_to, id_group)
            ) WITHOUT ROWID;
            
            CREATE INDEX ix_type_name ON data_table(type_name);
            CREATE INDEX ix_entity_type ON data_table(entity_type);
            CREATE INDEX ix_fk_section ON data_table(fk_section);
            CREATE INDEX ix_id_from ON data_xref(id_from);
        """)
        self.db_cxn.commit()
        
    def p_exchange_file(self, p):
        """exchange_file : PART21_START header_section data_section_list PART21_END"""
        self.closedb()

    def p_header_section(self, p):
        """header_section : header_start header_entity header_entity header_entity ENDSEC"""

    def p_header_section_with_entity_list(self, p):
        """header_section : header_start header_entity header_entity header_entity header_entity_list ENDSEC"""
        
    def p_header_section_start(self, p):
        """header_start : HEADER"""
        tmpl = "INSERT INTO section_table(lineno, section_type) VALUES (?,?)"
        self.db_writer.execute(tmpl, (p.lineno(1), 'H'))
        self.db_writer.execute('SELECT last_insert_rowid();')
        (self.sid,) = self.db_writer.fetchone()

    def p_header_entity(self, p):
        """header_entity : KEYWORD raw_data ';'"""
        tmpl = "INSERT INTO section_headers(type_name, raw_data, lineno, fk_section) VALUES (?, ?, ?, ?)"
        self.db_writer.execute(tmpl, (p[1], p[2], p.lineno(1), self.sid))
        
    def p_header_entity_list_init(self, p):
        """header_entity_list : header_entity"""
        
    def p_header_entity_list(self, p):
        """header_entity_list : header_entity_list header_entity"""

    def p_data_section(self, p):
        """data_section : data_start entity_instance_list ENDSEC""" 

    def p_data_start(self, p):
        """data_start : DATA '(' parameter_list ')' ';'"""
        tmpl = "INSERT INTO section_table(lineno, section_type) VALUES (?,?)"
        lineno = p.lineno(1)
        self.db_writer.execute(tmpl, (lineno, 'D'))
        self.db_writer.execute('SELECT last_insert_rowid();')
        (self.sid,) = self.db_writer.fetchone()
        tmpl = "INSERT INTO section_headers(type_name, raw_data, lineno, fk_section) VALUES (?, ?, ?, ?)"
        self.db_writer.executemany(tmpl, [(t, x, lineno, self.sid) for t, x in p[3]])

    def p_data_start_empty(self, p):
        """data_start : DATA '(' ')' ';'
                      | DATA ';'"""
        tmpl = "INSERT INTO section_table(lineno, section_type) VALUES (?,?)"
        self.db_writer.execute(tmpl, (p.lineno(1), 'D'))
        self.db_writer.execute('SELECT last_insert_rowid();')
        (self.sid,) = self.db_writer.fetchone()        

    def p_data_section_list_init(self, p):
        """data_section_list : data_section"""
        
    def p_data_section_list(self, p):
        """data_section_list : data_section_list data_section"""

    def p_entity_instance_list_init(self, p):
        """entity_instance_list : entity_instance"""
        
    def p_entity_instance_list(self, p):
        """entity_instance_list : entity_instance_list entity_instance"""

    def p_entity_instance(self, p):
        """entity_instance : simple_entity_instance
                           | complex_entity_instance"""
        
    def p_entity_instance_error(self, p):
        """entity_instance  : EID '=' error ';'"""
        logger.error('resyncing parser, check input between lineno %d and %d', p.lineno(2), p.lineno(4))

    def p_simple_entity_instance(self, p):
        """simple_entity_instance : EID '=' KEYWORD raw_data ';'"""
        eid = p[1]
        tmpl = "INSERT INTO data_table VALUES (?,?,?,?,?,?)"
        self.db_writer.execute(tmpl, (p[1], p[3], p[4][1:-1], p.lineno(1), 'S', self.sid))
        tmpl = "INSERT INTO data_xref(id_from, id_to, id_group) VALUES (?, ?, ?)"
        xrefs = [(rid, eid, n) for n, x in _mkgroups(p[4]) for rid in x]
        self.db_writer.executemany(tmpl, xrefs)

    def p_complex_entity_instance(self, p):
        """complex_entity_instance : EID '=' raw_data ';'"""
        eid = p[1]
        tmpl = "INSERT INTO data_table VALUES (?,NULL,?,?,?,?)"
        self.db_writer.execute(tmpl, (p[1], p[3], p.lineno(1), 'C', self.sid))
        tmpl = "INSERT INTO data_xref(id_from, id_to, id_group) VALUES (?, ?, ?)"
        xrefs = [(rid, eid, n) for n, x in _mkgroups(p[3]) for rid in x]
        self.db_writer.executemany(tmpl, xrefs)

    def p_parameter_list_init(self, p):
        """parameter_list : parameter"""
        p[0] = [p[1],]
        
    def p_parameter_list(self, p):
        """parameter_list : parameter_list ',' parameter"""
        p[0] = p[1]
        p[0].append(p[3])
        
    def p_typed_parameter(self, p):
        """parameter : KEYWORD raw_data"""
        p[0] = (p[1], p[2])
        
    def p_other_parameter(self, p):
        """parameter : raw_data"""
        p[0] = (None, p[1])

    def p_raw_concat(self, p):
        """raw_data : raw_data RAW
                    | RAW"""
        try: p[0] = p[1] + p[2]
        except IndexError: p[0] = p[1]
            

def debug_lexer():
    import codecs
    from os.path import normpath, expanduser
    
    logging.basicConfig()
    logger.setLevel(logging.DEBUG)
    
    lexer = Lexer(debug=True)
    
    p = normpath(expanduser('~/projects/src/stepcode/data/ap214e3/s1-c5-214/s1-c5-214.stp'))
    with codecs.open(p, 'r', encoding='iso-8859-1') as f:
        s = f.read()
        lexer.input(s)
        while True:
            tok = lexer.token()
            if not tok: break
            logger.debug(tok)

def debug_parser():
    import codecs
    from os.path import normpath, expanduser

    logging.basicConfig()
    logger.setLevel(logging.DEBUG)

    parser = Parser(debug=True, tempdb=True)
    
    logger.info("***** parser debug *****")
    p = normpath(expanduser('~/projects/src/stepcode/data/ap214e3/s1-c5-214/s1-c5-214.stp'))
    with codecs.open(p, 'r', encoding='iso-8859-1') as f:
        s = f.read()
        parser.parse(s, debug=1)
        
    # test reverse lookup
    logger.info('***** testing xrefs *****')
    tm = datetime.datetime.utcnow().isoformat(timespec='seconds').replace(':', '-')
    db_path = os.path.join(tempfile.mkdtemp(), tm + '_xref_test.db')
    
    parser = Parser()

    p = normpath(expanduser('~/projects/src/stepcode/data/ap214e3/s1-c5-214/s1-c5-214.stp'))
    with codecs.open(p, 'r', encoding='iso-8859-1') as f:
        s = f.read()
        parser.parse(s, db_path=db_path)
    
    # contrived use case: we're looking for the objects referencing each of these [set] of items
    items = [('#53','#93','#133','#173','#191'), ('#174','#192'), ('#193','#196','#195'), ('#1',)]
    tmpl = "SELECT id_to FROM data_xref WHERE id_from IN (%s) GROUP BY id_group, id_to HAVING count(id_to) = ?"
    with sqlite3.connect(db_path) as db_cxn:
        for grp in items:
            db_cursor = db_cxn.execute(tmpl % ','.join('?'*len(grp)), grp + (len(grp),))
            for eid in db_cursor:
                logger.info('grp: %s, ref: %r', grp, eid)
    
    logger.info("***** finished *****")
    
def test():
    import os, codecs
    from os.path import normpath, expanduser
    
    logging.basicConfig()
    logger.setLevel(logging.INFO)

    lexer = Lexer(optimize=True)
    parser = Parser(lexer=lexer, optimize=True)

    def parse_check(p):
        logger.info("processing {0}".format(p))
        parser.reset()
        with codecs.open(p, 'r', encoding='iso-8859-1') as f:
            s = f.read()
            parser.parse(s)

    logger.info("***** standard test *****")
    stepcode_dir = normpath(os.path.expanduser('~/projects/src/stepcode'))
    for d, _, files in os.walk(stepcode_dir):
        for f in filter(lambda x: x.endswith('.stp'), files):
            p = os.path.join(d, f)
            try:
                parse_check(p)
            except LexError:
                logger.exception('Lexer failure: {0}'.format(os.path.basename(p)))

    logger.info("***** finished *****")


if __name__ == '__main__':
    #debug_lexer()
    #debug_parser()
    test()

