use Parse::RecDescent;

local $/;
my $grammar = <DATA>;
my $parser = Parse::RecDescent->new($grammar);

my $text = <>;

my $parse_tree = $parser->translation_unit($text) or die "bad C code";

__DATA__

<autotree>

primary_expression:
          IDENTIFIER
        | CONSTANT
        | STRING_LITERAL
        | '(' expression ')'

postfix_expression:
          primary_expression
        | (primary_expression)(s) '[' expression ']'
        | (primary_expression)(s) '(' ')'
        | (primary_expression)(s) '(' argument_expression_list ')'
        | (primary_expression)(s) '.' IDENTIFIER
        | (primary_expression)(s) PTR_OP IDENTIFIER
        | (primary_expression)(s) INC_OP
        | (primary_expression)(s) DEC_OP

argument_expression_list:
          (assignment_expression ',')(s?) assignment_expression

unary_expression:
          postfix_expression
        | INC_OP unary_expression
        | DEC_OP unary_expression
        | unary_operator cast_expression
        | SIZEOF unary_expression
        | SIZEOF '(' type_name ')'

unary_operator:
          '&'
        | '*'
        | '+'
        | '-'
        | '~'
        | '!'

cast_expression:
          unary_expression
        | '(' type_name ')' cast_expression

multiplicative_expression:
          (cast_expression mul_ex_op)(s?) cast_expression

mul_ex_op : '*' | '/' | '%'

additive_expression:
          (multiplicative_expression add_op)(s?) multiplicative_expression

add_op : '+' | '-'

shift_expression:
          (additive_expression shift_op )(s?) additive_expression

shift_op : LEFT_OP | RIGHT_OP

relational_expression:
          (shift_expression rel_op)(s?) shift_expression

rel_op: '<' | '>' | LE_OP | GE_OP

equality_expression:
          (relational_expression eq_ex_op)(s?) relational_expression

eq_ex_op : EQ_OP | NE_OP

and_expression:
          (equality_expression '&')(s?) equality_expression

exclusive_or_expression:
          (and_expression '^')(s?) and_expression

inclusive_or_expression:
          (exclusive_or_expression '|')(s?) exclusive_or_expression

logical_and_expression:
          (inclusive_or_expression AND_OP)(s?) inclusive_or_expression

logical_or_expression:
          (logical_and_expression OR_OP)(s?) logical_and_expression

conditional_expression:
          logical_or_expression
        | logical_or_expression '?' expression ':' conditional_expression

assignment_expression:
          conditional_expression
        | unary_expression assignment_operator assignment_expression

assignment_operator:
          '='
        | MUL_ASSIGN
        | DIV_ASSIGN
        | MOD_ASSIGN
        | ADD_ASSIGN
        | SUB_ASSIGN
        | LEFT_ASSIGN
        | RIGHT_ASSIGN
        | AND_ASSIGN
        | XOR_ASSIGN
        | OR_ASSIGN

expression:
          (assignment_expression ',')(s?) assignment_expression

constant_expression:
          conditional_expression

declaration:
          declaration_specifiers ';'
          { print "We have a match!\n"; }
        | declaration_specifiers init_declarator_list ';'

declaration_specifiers:
          storage_class_specifier
        | storage_class_specifier declaration_specifiers
        | type_specifier
        | type_specifier declaration_specifiers
        | type_qualifier
        | type_qualifier declaration_specifiers

init_declarator_list:
          (init_declarator ',')(s?) init_declarator

init_declarator:
          declarator
        | declarator '=' initializer

storage_class_specifier:
          TYPEDEF
        | EXTERN
        | STATIC
        | AUTO
        | REGISTER

type_specifier:
          VOID
        | CHAR
        | SHORT
        | INT
        | LONG
        | FLOAT
        | DOUBLE
        | SIGNED
        | UNSIGNED
        | struct_or_union_specifier
        | enum_specifier
        | TYPE_NAME

struct_or_union_specifier:
          struct_or_union IDENTIFIER '{' struct_declaration_list '}'
        | struct_or_union '{' struct_declaration_list '}'
        | struct_or_union IDENTIFIER

struct_or_union:
          STRUCT
        | UNION

struct_declaration_list:
          struct_declaration(s)

struct_declaration:
          specifier_qualifier_list struct_declarator_list ';'

specifier_qualifier_list:
          type_specifier specifier_qualifier_list
        | type_specifier
        | type_qualifier specifier_qualifier_list
        | type_qualifier

struct_declarator_list:
          (struct_declarator ',')(s?) struct_declarator

struct_declarator:
          declarator
        | ':' constant_expression
        | declarator ':' constant_expression

enum_specifier:
          ENUM '{' enumerator_list '}'
        | ENUM IDENTIFIER '{' enumerator_list '}'
        | ENUM IDENTIFIER

enumerator_list:
          (enumerator ',')(s?) enumerator

enumerator:
          IDENTIFIER
        | IDENTIFIER '=' constant_expression

type_qualifier:
          CONST
        | VOLATILE

declarator:
          pointer direct_declarator
        | direct_declarator

direct_declarator:
          IDENTIFIER
        | '(' declarator ')'
        | (IDENTIFIER)(s?) ('(' declarator ')')(s?) '[' constant_expression ']'
        | (IDENTIFIER)(s?) ('(' declarator ')')(s?) '[' ']'
        | (IDENTIFIER)(s?) ('(' declarator ')')(s?) '(' parameter_type_list ')'
        | (IDENTIFIER)(s?) ('(' declarator ')')(s?) '(' identifier_list ')'
        | (IDENTIFIER)(s?) ('(' declarator ')')(s?) '(' ')'

pointer:
          '*'
        | '*' type_qualifier_list
        | '*' pointer
        | '*' type_qualifier_list pointer

type_qualifier_list:
          type_qualifier(s)

parameter_type_list:
          parameter_list
        | parameter_list ',' ELLIPSIS

parameter_list:
          (parameter_declaration ',')(s?) parameter_declaration

parameter_declaration:
          declaration_specifiers declarator
        | declaration_specifiers abstract_declarator
        | declaration_specifiers

identifier_list:
          (IDENTIFIER ',')(s?) IDENTIFIER

type_name:
          specifier_qualifier_list
        | specifier_qualifier_list abstract_declarator

abstract_declarator:
          pointer
        | direct_abstract_declarator
        | pointer direct_abstract_declarator

direct_abstract_declarator:
          '(' abstract_declarator ')'
        | '[' ']'
        | '[' constant_expression ']'
        | DAD '[' ']'
        | DAD '[' constant_expression ']'
        | '(' ')'
        | '(' parameter_type_list ')'
        | DAD '(' ')'
        | DAD '(' parameter_type_list ')'

DAD:    #macro for direct_abstract_declarator
          ( '(' abstract_declarator ')' )(s?)
          ( '[' ']' )(s?)
          ( '[' constant_expression ']' )(s?)
          ( '(' ')' )(s?)
          ( '(' parameter_type_list ')' )(s?)

initializer:
          assignment_expression
        | '{' initializer_list '}'
        | '{' initializer_list ',' '}'

initializer_list:
          (initializer ',')(s?) initializer

statement:
          labeled_statement
        | compound_statement
        | expression_statement
        | selection_statement
        | iteration_statement
        | jump_statement

labeled_statement:
          IDENTIFIER ':' statement
        | CASE constant_expression ':' statement
        | DEFAULT ':' statement

compound_statement:
          '{' '}'
        | '{' statement_list '}'
        | '{' declaration_list '}'
        | '{' declaration_list statement_list '}'

declaration_list:
          declaration(s)

statement_list:
          statement(s)

expression_statement:
          ';'
        | expression ';'

selection_statement:
          IF '(' expression ')' statement
        | IF '(' expression ')' statement ELSE statement
        | SWITCH '(' expression ')' statement

iteration_statement:
          WHILE '(' expression ')' statement
        | DO statement WHILE '(' expression ')' ';'
        | FOR '(' expression_statement expression_statement ')' statement
        | FOR '(' expression_statement expression_statement expression ')' statement

jump_statement:
          GOTO IDENTIFIER ';'
        | CONTINUE ';'
        | BREAK ';'
        | RETURN ';'
        | RETURN expression ';'

translation_unit:
          external_declaration(s)

external_declaration:
          function_definition
        | declaration

function_definition:
          declaration_specifiers declarator declaration_list compound_statement
        | declaration_specifiers declarator compound_statement
        | declarator declaration_list compound_statement
        | declarator compound_statement

# TERMINALS

reserved_word:
        AUTO     | BREAK   | CASE     | CHAR   | CONST    |
        CONTINUE | DEFAULT | DO       | DOUBLE | ENUM     |
        EXTERN   | FLOAT   | FOR      | GOTO   | IF       |
        INT      | LONG    | REGISTER | RETURN | SHORT    |
        SIGNED   | SIZEOF  | STATIC   | STRUCT | SWITCH   |
        TYPEDEF  | UNION   | UNSIGNED | VOID   | VOLATILE |
        WHILE


ADD_ASSIGN:     '+='
AND_ASSIGN:     '&='
AND_OP:         '&&'
AUTO:           'auto'
BREAK:          'break'
CASE:           'case'
CHAR:           'char'
CONST:          'const'
CONSTANT:       /[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/
CONTINUE:       'continue'
DEC_OP:         '--'
DEFAULT:        'default'
DIV_ASSIGN:     '/='
DO:             'do'
DOUBLE:         'double'
ELLIPSIS:       '...'
ELSE:           'else'
ENUM:           'enum'
EQ_OP:          '=='
EXTERN:         'extern'
FLOAT:          'float'
FOR:            'for'
GE_OP:          '>='
GOTO:           'goto'
IDENTIFIER:     ...!reserved_word /[a-z]\w*/i
IF:             'if'
INC_OP:         '++'
INT:            'int'
LEFT_ASSIGN:    '<<='
LEFT_OP:        '<<'
LE_OP:          '<='
LONG:           'long'
MOD_ASSIGN:     '%='
MUL_ASSIGN:     '*='
NE_OP:          '!='
OR_ASSIGN:      '|='
OR_OP:          '||'
PTR_OP:         '->'
REGISTER:       'register'
RETURN:         'return'
RIGHT_ASSIGN:   '>>='
RIGHT_OP:       '>>'
SHORT:          'short'
SIGNED:         'signed'
SIZEOF:         'sizeof'
STATIC:         'static'
STRING_LITERAL: { extract_delimited($text,'"') }
STRUCT:         'struct'
SUB_ASSIGN:     '-='
SWITCH:         'switch'
TYPEDEF:        'typedef'
TYPE_NAME:      # NONE YET
UNION:          'union'
UNSIGNED:       'unsigned'
VOID:           'void'
VOLATILE:       'volatile'
WHILE:          'while'
XOR_ASSIGN:     '^='
