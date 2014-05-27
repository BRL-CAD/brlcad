# This improved version of the C grammar was provided by Joe Buehler

use Parse::RecDescent;

local $/;
my $grammar = <DATA>;
my $parser = Parse::RecDescent->new($grammar);

my $text = <>;

my $parse_tree = $parser->translation_unit($text) or die "bad C code";

use Data::Dumper 'Dumper';
warn Dumper [ $parse_tree ];

__DATA__
<autotree>

primary_expression:
	IDENTIFIER
	| CONSTANT
	| STRING_LITERAL
	| '(' expression ')'

postfix_expression_post:
	'[' expression ']'
	| '(' argument_expression_list(?) ')'
	| '.' IDENTIFIER
	| '->' IDENTIFIER
	| '++'
	| '--'

postfix_expression:
	primary_expression postfix_expression_post(s?)

argument_expression_list:
	assignment_expression (',' assignment_expression)(s?)

unary_expression:
	postfix_expression
	| '++' unary_expression
	| '--' unary_expression
	| unary_operator cast_expression
	| SIZEOF unary_expression
	| SIZEOF '(' type_name ')'

unary_operator:
	/[-&*+!~]/

cast_expression:
	'(' type_name ')' cast_expression
	| unary_expression

multiplicative_expression_op:
	/[*\/%]/

multiplicative_expression:
	cast_expression (multiplicative_expression_op cast_expression)(s?)

additive_expression_op:
	/[-+]/

additive_expression:
	multiplicative_expression (additive_expression_op multiplicative_expression)(s?)

shift_expression_pre_op:
	/(<<|>>)(?!=)/

shift_expression:
	additive_expression (shift_expression_pre_op additive_expression)(s?)

relational_expression_op:
	/(<=|>=|<|>)/

relational_expression:
	shift_expression (relational_expression_op shift_expression)(s?)

equality_expression_pre_op:
	/==|!=/

equality_expression:
	relational_expression (equality_expression_pre_op relational_expression)(s?)

and_expression:
	equality_expression ('&' equality_expression)(s?)

exclusive_or_expression:
	and_expression ('^' and_expression)(s?)

inclusive_or_expression:
	exclusive_or_expression ('|' exclusive_or_expression)(s?)

logical_and_expression:
	inclusive_or_expression ('&&' inclusive_or_expression)(s?)

logical_or_expression:
	logical_and_expression ('||' logical_and_expression)(s?)

conditional_expression:
	logical_or_expression ('?' expression ':' conditional_expression)(?)

assignment_expression:
	conditional_expression
	| unary_expression assignment_operator assignment_expression

assignment_operator:
	/=(?!=)|\+=|\&=|\/=|<<=|\%=|\*=|\|=|>>=|-=|\^=/

expression:
	assignment_expression (',' assignment_expression)(s?)

constant_expression:
	conditional_expression

declaration:
	declaration_specifiers init_declarator_list ';'
	| declaration_specifiers ';'

declaration_specifiers:
	( storage_class_specifier | type_specifier | type_qualifier )(s)

init_declarator_list:
	init_declarator (',' init_declarator)(s?)

init_declarator:
	declarator '=' initializer
	| declarator

storage_class_specifier:
	/(typedef|extern|static|auto|register)(?![a-zA-Z0-9_])/

type_specifier:
	/(void|char|short|int|long|float|double|signed|unsigned)(?![a-zA-Z0-9_])/
	| struct_or_union_specifier
	| enum_specifier
	| TYPE_NAME

struct_or_union_specifier:
	struct_or_union IDENTIFIER '{' struct_declaration_list '}'
	| struct_or_union '{' struct_declaration_list '}'
	| struct_or_union IDENTIFIER

struct_or_union:
	/(struct|union)(?![a-zA-Z0-9_])/

struct_declaration_list:
	struct_declaration(s)

struct_declaration:
	specifier_qualifier_list struct_declarator_list ';'

specifier_qualifier_list:
	( type_specifier | type_qualifier )(s)

struct_declarator_list:
	struct_declarator (',' struct_declarator)(s?)

struct_declarator:
	declarator ':' constant_expression
	| declarator
	| ':' constant_expression

enum_specifier:
	ENUM '{' enumerator_list '}'
	| ENUM IDENTIFIER '{' enumerator_list '}'
	| ENUM IDENTIFIER

enumerator_list:
	enumerator (',' enumerator)(s?)

enumerator:
	IDENTIFIER '=' constant_expression
	| IDENTIFIER

type_qualifier:
	/(const|volatile)(?![a-zA-Z0-9_])/

declarator:
	pointer(?) direct_declarator

direct_declarator_pre:
	IDENTIFIER
	| '(' declarator ')'

direct_declarator_post:
	'[' constant_expression ']'
	| '[' ']'
	| '(' parameter_type_list ')'
	| '(' identifier_list ')'
	| '(' ')'

direct_declarator:
	direct_declarator_pre direct_declarator_post(s?)

pointer:
	'*' type_qualifier_list(?) pointer(?)

type_qualifier_list:
	type_qualifier(s)

parameter_type_list:
	parameter_list ',' '...'
	| parameter_list

parameter_list:
	parameter_declaration (',' parameter_declaration)(s?)

parameter_declaration:
	declaration_specifiers declarator
	| declaration_specifiers abstract_declarator(?)

identifier_list:
	IDENTIFIER (',' IDENTIFIER)(s?)

type_name:
	specifier_qualifier_list abstract_declarator(?)

abstract_declarator:
	pointer direct_abstract_declarator(?)
	| direct_abstract_declarator

direct_abstract_declarator_pre:
	'(' abstract_declarator ')'
	| direct_abstract_declarator_post

direct_abstract_declarator_post:
	'[' ']'
	| '[' constant_expression ']'
	| '(' ')'
	| '(' parameter_type_list ')'

direct_abstract_declarator:
	direct_abstract_declarator_pre direct_abstract_declarator_post(s?)

initializer:
	assignment_expression
	| '{' initializer_list '}'
	| '{' initializer_list ',' '}'

initializer_list:
	initializer (',' initializer)(s?)

statement:
	labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement

labeled_statement:
	IDENTIFIER ':' statement
	| /case(?![a-zA-Z0-9_])/ constant_expression ':' statement
	| /default(?![a-zA-Z0-9_])/ ':' statement

compound_statement:
	'{' declaration_list statement_list(?) '}'
	| '{' statement_list(?) '}'

declaration_list:
	declaration(s)

statement_list:
	statement(s)

expression_statement:
	expression(?) ';'

selection_statement:
	IF '(' expression ')' statement ELSE statement
	| IF '(' expression ')' statement
	| SWITCH '(' expression ')' statement

iteration_statement:
	WHILE '(' expression ')' statement
	| DO statement WHILE '(' expression ')' ';'
	| FOR '(' expression_statement expression_statement expression(?) ')' statement

jump_statement:
	/goto(?![a-zA-Z0-9_])/ IDENTIFIER ';'
	| /continue(?![a-zA-Z0-9_])/ ';'
	| /break(?![a-zA-Z0-9_])/ ';'
	| /return(?![a-zA-Z0-9_])/ expression(?) ';'

translation_unit:
	external_declaration(s)

external_declaration:
	function_definition { print "*** function\n"; }
	| declaration { print "*** declaration\n"; }

function_definition:
	declaration_specifiers declarator(?) declaration_list(?) compound_statement

reserved_word:
	/(auto|break|case|char|const|continue|default|do|double|enum|extern|float|for|goto|if|int|long|register|return|short|signed|sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while)(?![a-zA-Z0-9_])/

CONSTANT:       /[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/
DO:             /do(?![a-zA-Z0-9_])/
ELSE:           /else(?![a-zA-Z0-9_])/
ENUM:           /enum(?![a-zA-Z0-9_])/
FOR:            /for(?![a-zA-Z0-9_])/
IDENTIFIER:     ...!reserved_word /[a-zA-Z_][a-zA-Z_0-9]*/
IF:             /if(?![a-zA-Z0-9_])/
SIZEOF:         /sizeof(?![a-zA-Z0-9_])/
STRING_LITERAL: { extract_delimited($text,'"') }
SWITCH:         /switch(?![a-zA-Z0-9_])/
TYPE_NAME:      ...!reserved_word /[a-zA-Z_][a-zA-Z_0-9]*/ ...IDENTIFIER
				| ...!reserved_word /[a-zA-Z_][a-zA-Z_0-9]*/ .../[:*)]/
WHILE:          /while(?![a-zA-Z0-9_])/

