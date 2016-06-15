==========
 Overview
==========
Perplex is a simple tool to simplify the creation of scanners using re2c. It
generates an input for the re2c tool from a perplex input file.

Main Sources
-------------
mbo_getopt.cpp and mbo_getopt.h
    Option parser, taken from the re2c project.

perplex.cpp
    main for perplex.
    
scanner.re and parser.y
    Inputs for re2c scanner-generator and lemon parser-generator respectively.
    These files implement the perplex input file parser.

perplex_template.c
    Template file with a basic re2c scanner implementation.
    Used as the basis for generated scanner sources.

Licensing and Copyrights
-------------------------
mbo_getopt.cpp and mbo_getopt.h are in the Public Domain, written by
Marcus Boerger.

scanner.re and perplex_template.c include code taken from the flex project,
and are released under a BSD License with joint copyright held by the U.S.
Government and The Regents of the University of California.

All other source files are released under a BSD License with U.S. Government
copyright.

=====
 API
=====
All scanner data is stored in a perplex_t object.

The template implements the following public functions:

perplex_t perplexFileScanner(FILE *input)
    Creates a perplex_t object initialized to scan input from the specified
    file stream. The scanner will stop scanning when EOF is encountered.

perplex_t perplexStringScanner(char *firstChar, size_t numChars)
    Creates a perplex_t object initialized to scan input from the specified
    string. The scanner will stop scanning when numChars characters have
    been scanned.

void perplexFree(perplex_t scanner)
    Frees all memory associated with the given perplex_t object, except for
    any memory referenced by scanner->extra, which must be freed by the user.

int yylex(perplex_t scanner)
    Returns the value of the next recognized token, or YYEOF when the end of
    the input has been reached. The name of this routine can be changed by
    defining the pre-processor symbol PERPLEX_LEXER in section 1 of the
    perplex input file.

void perplexSetExtra(perplex_t scanner, void *extra)
    Set scanner's application data.

void *perplexGetExtra(perplex_t scanner)
    Get scanner's application data.

void perplexUnput(peprlex_t scanner, char c)
    Inserts a character on the specified scanner's input buffer so that it
    is the next character scanned.

Configuration Macros
---------------------
These macros can be defined in section 1 of the perplex input file:

PERPLEX_LEXER
    Set this symbol to specify an alternate name for the generated lexer.
    Default is yylex.

PERPLEX_ON_ENTER
    Use this symbol to specify code to run at the beginning of each call to
    the lexer. A common use is to simplify access to application data:

    #define PERPLEX_ON_ENTER appData_t *appData = (appData_t*)yyextra;

Macros Available Inside Rule Code Blocks
-----------------------------------------
These macros can only be used inside rule code blocks:

YYGETCONDITION and YYSETCONDITION(condition)
    Get or set the current start conditions, (requires running perplex and
    re2c with '-c' option flag).

yytext
    A dynamically allocated null-terminated string holding the input which
    was matched. yytext is guaranteed to exist until the end of the code
    block it is used in, but will be automatically freed afterwards. If
    you need to store the token text, you'll need to make a copy.

yyextra
    Application data (void*).

===============
 Using Perplex
===============
1) Write a perplex input file (see Perplex Input Format).

2) Run perplex on the input to generate an re2c input file.

    perplex -t /path/to/perplex_template.c -h header.h -o output.re input.l

* Input defaults to stdin and output defaults to stdout.

* The generated header contains the perplex_t definition and public function
  prototypes. This header should be manually included in input.l. If no
  output header path is specified, the definitions/declarations will appear
  at the top of the output source.

3) Run re2c on the re2c input to generate the final scanner source.

    re2c -o /path/to/output.c /path/to/output.re

4) Scan input.

    int tokenID;
    perplex_t scanner = perplexFileScanner(inFile);

    perplexSetExtra(scanner, (void*)appData);
    
    while ((tokenID = yylex(scanner)) != YYEOF) {
	...
    }

    /* do something with appData */

    perplexFree(scanner);

======================
 Perplex Input Format
======================
Perplex takes a three-section file as input:

    /* section 1 - code copied to output */
    %%

    /* section 2 - scanner rules (see Perplex Rule Section Syntax) */
    <regular-expression> { <code> }
    <regular-expression> { <code> }

    %%
    /* section 3 (optional) - code copied to output */

Sections are separated by "%%" appearing on a line by itself, with no leading
or trailing whitespace. The second "%%" separator and following code section
are optional.

=============================
 Perplex Rule Section Syntax
=============================
The basic form of the rule section is a series of regular expressions followed
by C/C++ code to be executed when the current input string matches the
expression:

    <regular-expression> { <code> }
    <regular-expression> { <code> }

The code block spans between { and }, not including C-braces or braces
appearing within strings or comments. The code-block is optional. If missing,
the default behavior is to ignore the matched input string and continue
scanning.

* See the re2c documentation for a list of valid regular expressions.

Named Definitions
------------------
Regular expressions can be given aliases using re2c's named definition syntax.
All named definitions should appear before the first rule.

    %%
    /* named definitions */

    alpha = [A-Za-z];
    num = [0-9];

    /* rules */

    alpha { /* code */ }
    num { /* code */ }

    /* to match something like "A1" */
    alpha(num) { /* code */ }

Exiting From Code Blocks
-------------------------
When the end of a code block is reached, the default behavior is to advance
past the matched input, and continue scanning:

    [ \t]+ { /* ignore and continue scanning */ }

If you have matched a token, you will probably return a token identifier rather
than immediately resume scanning:

    [A-Za-z_]+ { return TOKEN_NAME; }

It is also possible that you will match part of a token, and need to continue
scanning to find the end of it. Use the continue keyword to avoid throwing out
the text matched so far:

    /* start of list */
    '{' { continue; }

    /* intermediate list item */
    [^,}]',' { continue; }

    /* end of list */
    [^,}]'}' { return TOKEN_LIST; }

==================
 Start Conditions
==================
Start conditions allow you to specify that certain rules only be applied in
certain states. Using start conditions requires that perplex and re2c are
run with the '-c' option flag.

Conditions are simply ints, defined in whatever manner is convenient
(e.g. in an enum).

When using conditions, all rules must either be prefixed with a
comma-separated list of conditions appearing between < and >, or must be
specified inside a condition scope (a condition list followed by rules
in between { and }):

    enum {conditionOne, conditionTwo, conditionThree};
    %%
    /* condition scope */
    <conditionOne> {
    "center" { /* code */ }
    "view" { /* code */ }
    }

    /* simple condition-prefixed rules */
    <conditionOne,conditionTwo>[a-zA-Z]+ { /* code */ }

    <conditionThree>'v' { /* code */ }

0 is the initial condition, and can be referrenced as the empty list "<>" in
the rule section. "<*>" is the shorthand for specifying all conditions.

The current condition can be determined by calling YYGETCONDITION(). A new
condition can be set by calling YYSETCONDITION(<condition>), or by using one of
the re2c transition operators: "=>" or ":=>". Take the example of changing from
a "code" condition to a "comment" condition:

    enum {INITIAL, code, comment};
    %%
    /* scanner's first order of business is to change from "INITIAL"
     * to "code" condition
     */
    <> => code

    /* this... */
    <code>"/*" => comment { /* code */ }

    /* ...is equivalent to this */
    <code>"/*" {
	YYSETCONDITION(comment);
	/* code */
    }

    /* and this... */
    <code>"/*" :=> comment

    /* ...is equivalent to this */
    <code>"/*" :=> comment {
	YYSETCONDITION(comment);
	continue;
    }
