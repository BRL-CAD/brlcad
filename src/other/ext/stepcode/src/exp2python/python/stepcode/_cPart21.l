/*
 * STEP Part 21 Parser
 *
 * Copyright (c) 2020, Christopher HORLER (cshorler@googlemail.com)
 *
 * All rights reserved.
 *
 * This file is part of the STEPCODE project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 *   Neither the name of the <ORGANIZATION> nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#include <sqlite3.h>

#define YYCTYPE unsigned char
#define YYCURSOR in->cur
#define YYLIMIT in->lim
#define YYMARKER in->mrk
#define YYCTXMARKER in->ctxmrk
#define YYFILL(n) do { \
        if (fill(in, n) != 0) { \
            fprintf(stderr, "lexer fill(...) failed, exiting\n"); \
            exit(1); \
        } \
    } while (0)

/*!max:re2c*/
#define INIT_BUF_SZ 4096
#define INIT_STACK_SZ 64

/* reserved literals '(' ')' ';' '=' */
#define T_P21_START     'S'
#define T_P21_END       'X'
#define T_HEADER        'H'
#define T_DATA          'D'
#define T_ENDSEC        'E'
#define T_EID           'I'
#define T_KEYWORD       'K'
#define T_VARIANT       'V'
#define T_EOF           '\x00'
#define T_ERROR         '\x01'

#define V_REAL          'r'
#define V_INTEGER       'i'
#define V_STRING        's'
#define V_BINARY        'b'
#define V_ENUMERATION   'e'
#define V_EID           T_EID
#define V_DERIVED       '*'
#define V_EMPTY         '$'

#define P_FILE          'f'
#define P_HEADERSECTION 'h'
#define P_DATASECTION   'd'
#define P_HEADERENTITY  'x'
#define P_SIMPLEENTITY  's'
#define P_COMPLEXENTITY 'c'
#define P_SIMPLERECORD  'u'
#define P_LIST          'l'
#define P_PARAMETER     'p'

int debug = 1;
#define dprintf(fmt, ...) \
    do { if (debug) fprintf(stderr, "%s:%3d " fmt, __FILE__, __LINE__, ##__VA_ARGS__); } while (0)
        
/* ppfu https://stackoverflow.com/a/11763277/1162349 */
#define GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define _EXPAND(x) x

/* for lookahead */
#define PUSH_SYMBOL(...) _EXPAND(GET_MACRO(__VA_ARGS__, _4, _3, _PUSH_SYMBOL2, _PUSH_SYMBOL1)(__VA_ARGS__))
#define _PUSH_SYMBOL1(token) in->sym[in->nsym++] = (Symbol){(token), 0, n, in->lineno, in->sp - in->basemrk}
#define _PUSH_SYMBOL2(token, vtype) in->sym[in->nsym++] = (Symbol){(token), (vtype), n, in->lineno, in->sp - in->basemrk}

/* for parse stack */
#define PUSH_TERMINAL(stack, sym) do { \
        Symbol it = (sym); \
        push((stack), it); \
        if (it.token == T_ERROR) goto err; \
    } while (0)
    
#define PUSH_TERMINAL_EXT(cxt, stack, sym) do { \
        Symbol it = (sym); \
        push((stack), it); \
        if (it.token == T_ERROR) goto err; \
        else if (it.token == '(') (cxt) = (stack)->idx_top - 1; \
        else if (it.token == ')') (stack)->items[(cxt)].n = (stack)->idx_top - (cxt) - 1; \
    } while (0)

/* test for one in a set of 1 to 4 e.g. {t0, t1, t2, t3} */
#define LOOKAHEAD(x, ...) _EXPAND(GET_MACRO(__VA_ARGS__, _LOOKAHEAD4, _LOOKAHEAD3, _LOOKAHEAD2, _LOOKAHEAD1)(x, __VA_ARGS__))
#define _LOOKAHEAD1(x, t0)             ((t0) == (x).token)
#define _LOOKAHEAD2(x, t0, t1)         ((t0) == (x).token || (t1) == (x).token)
#define _LOOKAHEAD3(x, t0, t1, t2)     (_LOOKAHEAD2(x, t0, t1) || (t2) == (x).token)
#define _LOOKAHEAD4(x, t0, t1, t2, t3) (_LOOKAHEAD2(x, t0, t1) || _LOOKAHEAD2(x, t2, t3))


/*!rules:re2c
 ascii_encoding = [][!"*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_ ] | "''" | "\\\\" ;
 page_encoding  = "\\" [A-I] "\\" | "\\S\\" [][!"'*$%&.#+,\-()?/:;<=>@{}|^`~0-9a-zA-Z_\\ ] ;
 hex_encoding   = "\\X2\\" ([0-9A-F]{4})+ "\\X0\\" | "\\X4\\" ([0-9A-F]{8})+ "\\X0\\" ;
 byte_encoding  = "\\X\\" [0-9A-F]{2} ;

 NL = ("\n" | "\r\n") ;
 PUNCTUATION = [ ;()/] ;
 
 P21_START = "ISO-10303-21;" ;
 P21_END = "END-ISO-10303-21;" ;
 DATA = "DATA" ;
 HEADER = "HEADER;" ;
 ENDSEC = "ENDSEC;" ;

 WS = " " ;
 KEYWORD = "!"? [A-Za-z_] [0-9A-Za-z_]* ;
 REAL = [+-]* [0-9] [0-9]* "." [0-9]* ("E" [+-]* [0-9] [0-9]*)? ;
 INTEGER = [+-]* [0-9] [0-9]* ;
 STRING = "'" (ascii_encoding | page_encoding | hex_encoding | byte_encoding )* "'" ;
 BINARY = '"' [0-3] [0-9A-F]* '"' ;
 ENUMERATION = "." [A-Z_] [A-Z0-9_]* "." ;
 EID = "#" [0-9]+ ;

 <!*> { n = in->cur - in->sp; }
 WS+ { continue; }
 NL { in->lineno++; continue; }
 "/*" { lex_comment(in); continue; }
 * { YYCURSOR--; break; }

 */


/* lexeme */
typedef struct {
    uint8_t token;
    union {
        uint8_t vtype;
        uint8_t errtoken;
    };
    uint16_t n;
    uint32_t lineno;
    union {
        ptrdiff_t offset; /* from basemrk */
        void *data; /* production allocation if applicable */
    };
} Symbol;

typedef struct SimpleRecord_ {
    Symbol *kw;  /* 'KEYWORD' */
    Symbol *args;/* '(' */
} SimpleRecord;

typedef struct SimpleRecord_ HeaderEntity;

typedef struct {
    Symbol *eid; /* '#<idN>' */
    Symbol *eq;   /* '=' */
    Symbol *kw;  /* 'KEYWORD' */
    Symbol *args;/* '(' */
} SimpleEntity;

typedef struct {
    Symbol *eid; /* '#<idN>' */
    Symbol *eq;   /* '=' */
    Symbol *subsupers;/* '(' */
} ComplexEntity;


typedef struct {
    FILE *file;
    size_t bufsz;
    unsigned char *cur, *mrk, *ctxmrk, *lim;
    unsigned char *sp, *basemrk;
    int eof;
    uint32_t lineno;    
    int nsym;
    Symbol sym[3];
    unsigned char *buf;
} Input;

typedef struct {
    int idx_top;
    int idx_lim;
    Symbol *items;
} Stack;

/* LL(3) parser */
typedef struct {
    bool error;
    bool hold;
    Input *in;
    Stack *stack;
} P21Parser;

typedef void (p21_action_cb_t) (P21Parser *, int, void *);
typedef void (p21_error_cb_t) (P21Parser *, int, uint8_t);
typedef void (p21_ud_cb_t) (void *);

typedef struct {
    void            *userdata;
    p21_error_cb_t  *error_cb;
    p21_ud_cb_t     *ud_init_cb;
    p21_ud_cb_t     *ud_exit_cb;
    p21_action_cb_t *exchange_file_cb;
    p21_action_cb_t *header_start_cb;
    p21_action_cb_t *header_entity_list_cb;
    p21_action_cb_t *data_section_list_cb;
    p21_action_cb_t *data_start_cb;
    p21_action_cb_t *header_entity_cb;
    p21_action_cb_t *simple_entity_instance_cb;
    p21_action_cb_t *complex_entity_instance_cb;
    p21_action_cb_t *parameter_list_cb;
    p21_action_cb_t *parameter_cb;
    p21_action_cb_t *entity_instance_list_cb;
    p21_action_cb_t *entity_instance_cb;
    p21_action_cb_t *simple_record_list_cb;
    p21_action_cb_t *simple_record_cb;
} P21ParserActions;


void report_error(P21Parser *, const char *);
void _recover(Input *, uint8_t, uint8_t, uint8_t);
Symbol lpop(Input *, uint8_t);

void p21_parse(P21Parser *, P21ParserActions *);
void p21_exchange_file(P21Parser *, P21ParserActions *);
void p21_header_section(P21Parser *, P21ParserActions *);
void p21_header_entity_list(P21Parser *, P21ParserActions *);
void p21_header_entity(P21Parser *, P21ParserActions *);
void p21_data_section_list(P21Parser *, P21ParserActions *);
void p21_data_section(P21Parser *, P21ParserActions *);
void p21_entity_instance(P21Parser *, P21ParserActions *);
void p21_simple_entity_instance(P21Parser *, P21ParserActions *);
void p21_complex_entity_instance(P21Parser *, P21ParserActions *);
void p21_entity_instance_list(P21Parser *, P21ParserActions *);
void p21_parameter(P21Parser *, P21ParserActions *);
void p21_parameter_list(P21Parser *, P21ParserActions *);
void p21_simple_record(P21Parser *, P21ParserActions *);
void p21_simple_record_list(P21Parser *, P21ParserActions *);


void push(Stack *stack, Symbol it) {
    if (stack->idx_top == stack->idx_lim) {
        Symbol *nitems = realloc(stack->items, 2 * stack->idx_lim * sizeof stack->items[0]);
        if (!nitems) {
            fprintf(stderr, "failed to grow parser stack, memory exhausted\n");
            exit(1);
        }
        stack->items = nitems;
        stack->idx_lim *= 2;
    }
    
    stack->items[stack->idx_top++] = it;
}

/* mock implementations */
void drop(Stack *stack, uint32_t n) {
    assert(stack->idx_top >= n);
    stack->idx_top -= n;
}

void unwind(Stack *stack, int bsp) {
    stack->idx_top = bsp;
}

Symbol *pop(Stack *stack) {
    assert(stack->idx_top >= 1);
    stack->idx_top--;
    return stack->items + stack->idx_top;
}

Symbol *peek(Stack *stack) {
    assert(stack->idx_top >= 1);
    return stack->items + stack->idx_top - 1;
}

Symbol lpop(Input *in, uint8_t token) {
    Symbol *stack = in->sym;
    Symbol sym = stack[0];
    
    /* missing input or unexpected lookahead token */
    if (in->nsym == 0)
        return (Symbol){T_ERROR, token, 0, in->lineno};
    else if (sym.token != token)
        return (Symbol){T_ERROR, token, 0, sym.lineno};
    
    if (!--in->nsym) {
        memset(&in->sym[0], 0, sizeof in->sym[0]);
    } else {
        memmove(&in->sym[0], &in->sym[1], in->nsym * sizeof in->sym[0]);
        memset(&in->sym[in->nsym], 0, sizeof in->sym[0]);
    }
    
    return sym;
}

static int fill(Input *in, size_t need)
{
    size_t free;
    unsigned char *newbuf;
    
    if (in->eof) {
        return 1;
    }
    free = in->basemrk - in->buf;
    if (free < need) {
        newbuf = realloc(in->buf, 2 * in->bufsz + YYMAXFILL);
        if (!newbuf) {
            fprintf(stderr, "fatal - buffer memory exhausted, exiting\n");
            return 2;
        }
        in->bufsz *= 2;
        in->lim = newbuf + (in->lim - in->buf);
        in->cur = newbuf + (in->cur - in->buf);
        in->mrk = newbuf + (in->mrk - in->buf);
        in->ctxmrk = newbuf + (in->ctxmrk - in->buf);
        in->basemrk = newbuf + (in->basemrk - in->buf);
        in->sp = newbuf + (in->sp - in->buf);
        in->buf = newbuf;
        
        /* don't memmove() here! */
        free = (in->buf + in->bufsz) - in->lim;
    } else {
        memmove(in->buf, in->basemrk, in->lim - in->basemrk);
        in->lim -= free;
        in->cur -= free;
        in->mrk -= free;
        in->ctxmrk -= free;
        in->basemrk -= free;
        in->sp -= free;
    }
    
    in->lim += fread(in->lim, 1, free, in->file);
    if (in->lim < in->buf + in->bufsz) {
        in->eof = 1;
        memset(in->lim, 0, YYMAXFILL);
        in->lim += YYMAXFILL;
    }
    return 0;
}

static void p21_init(P21Parser *p, FILE *file)
{
    Stack *stack;
    Input *in; 

    in = malloc(sizeof *in);
    if (!in)
        goto err;
    memset(in, 0, sizeof *in);
    in->bufsz = INIT_BUF_SZ;
    in->buf = malloc(INIT_BUF_SZ + YYMAXFILL);
    if (!in->buf)
        goto err;
    in->file = file;
    in->cur = in->basemrk = in->sp = in->lim = in->buf + INIT_BUF_SZ;
    in->lineno = 1;
    fill(in, 1);

    stack = malloc(sizeof *stack);
    if (!stack)
        goto err;
    memset(stack, 0, sizeof *stack);
    stack->idx_lim = 16;
    stack->idx_top = 0;
    stack->items = malloc(stack->idx_lim * sizeof stack->items[0]);
    if (!stack->items)
        goto err;

    p->in = in;
    p->stack = stack;
    p->error = false;
    
    return;
    
err:
    fprintf(stderr, "failed to initialise parser\n");
    exit(1);
}

/* noop error handler */
void default_error_handler(P21Parser *p, int bsp, uint8_t t) {
    Symbol *sym = peek(p->stack);
    if (sym->token == T_ERROR)
        pop(p->stack);
    push(p->stack, (Symbol){t});
}

/* TODO: this needs to be reworked */
void report_error(P21Parser *p, const char *cxt) {
    Input *in = p->in;
    Symbol *it = peek(p->stack);
    int lineno;
    unsigned char *cur;
    
    fprintf(stderr, cxt);
    
    if (it->token == T_ERROR) {
        fprintf(stderr, "  syntax error - line: %d\n", it->lineno);
        fprintf(stderr, "  expected '%c' (token type) ", it->errtoken);
    } else {
        cur = in->cur;
        lineno = in->lineno;
        while (1) {
            if (*(cur - 2) == '\r' && *(cur - 1) == '\n') { cur -= 2; --lineno; }
            else if (*(cur - 1) == '\n') { --cur; --lineno; }
            else { break; }
        }
        fprintf(stderr, "  syntax error - line: %d\n", lineno);
    }
    
    if (!in->nsym) {
        cur = in->cur;
        lineno = in->lineno;
        while (1) {
            if (*cur == '\r' && *(cur + 1) == '\n') { cur -= 2; --lineno; }
            else if (*cur == '\n') { --cur; --lineno; }
            else { break; }
        }
        fprintf(stderr, "  unexpected character '%c' (line: %d)\n", *cur, lineno);        
    } else {
        fprintf(stderr, "  got '%c' (token type)\n", in->sym[0].token);
    }
}


void lex_comment(Input *in) {
    size_t n;
    int comment_lvl = 1;
    
    while (1) {
        in->sp = in->cur;
        /*!use:re2c
        NOT_SLASH_STAR = [][!"#$%&'()+,\-.:;<=>?@\\^`{|}~0-9A-Z_a-z ] ;     

        "*"+ "/" {
            if (!--comment_lvl) { break; }
            else { continue; }
        }
        "*"+ { continue; }
        NOT_SLASH_STAR+ { continue; }
        "/" { continue; }
        "/*" { ++comment_lvl; continue; } */
    }
    
    return;
    
err:
    fprintf(stderr, "invalid character in comment, exiting\n");
    exit(1);
}

#define recover(in, ...) GET_MACRO(__VA_ARGS__, _4, _RECOVER3, _RECOVER2, _RECOVER1)(in, __VA_ARGS__)
#define _RECOVER1(in, u0) _recover((in), (u0), 0U, 0U)
#define _RECOVER2(in, u0, u1) _recover((in), (u0), (u1), 0U)
#define _RECOVER3(in, u0, u1, u2) _recover((in), (u0), (u1), (u2))

void _recover(Input *in, uint8_t u0, uint8_t u1, uint8_t u2) {
    size_t n;
    Symbol sym;
    
    while (in->nsym) {
        if (LOOKAHEAD(in->sym[0], u0, u1, u2, T_EOF))
            break;
        --in->nsym;
        memmove(&in->sym[0], &in->sym[1], in->nsym * sizeof in->sym[0]);
        memset(&in->sym[in->nsym], 0, sizeof in->sym[0]);
    }
    
    if (in->nsym)
        return;
    
    while (1) {
        in->sp = in->cur;
        /*!use:re2c
         P21_START      { sym = (Symbol){T_P21_START}; goto check; }
         P21_END        { sym = (Symbol){T_P21_END}; goto check; }
         HEADER         { sym = (Symbol){T_HEADER}; goto check; }
         DATA / PUNCTUATION { sym = (Symbol){T_DATA}; goto check; }
         ENDSEC         { sym = (Symbol){T_ENDSEC}; goto check; }
         EID            { sym = (Symbol){T_EID}; goto check; }
         KEYWORD        { sym = (Symbol){T_KEYWORD}; goto check; }
         REAL           { sym = (Symbol){T_VARIANT, V_REAL}; goto check; }
         INTEGER        { sym = (Symbol){T_VARIANT, V_INTEGER}; goto check; }
         STRING         { sym = (Symbol){T_VARIANT, V_STRING}; goto check; }
         BINARY         { sym = (Symbol){T_VARIANT, V_BINARY}; goto check; }
         ENUMERATION    { sym = (Symbol){T_VARIANT, V_ENUMERATION}; goto check; }
         "*"            { sym = (Symbol){T_VARIANT, V_DERIVED}; goto check; }
         "$"            { sym = (Symbol){T_VARIANT, V_EMPTY}; goto check; }
         [();]           { sym = (Symbol){*in->sp}; goto check; }
        */
check:
        if (LOOKAHEAD(sym, u0, u1, u2, T_EOF)) {
            PUSH_SYMBOL(sym.token, sym.vtype);
            break;
        }
    }
    
    return;
    
err:
    fprintf(stderr, "fatal, failed to resolve follow set (%c, %c, %c)\n", u0, u1, u2);
    exit(1);
}


/*
 * P21Parser
 */
void p21_parse(P21Parser *p, P21ParserActions *act) {
    if (act->ud_init_cb)
        act->ud_init_cb(act->userdata);
    
    p21_exchange_file(p, act);
    
    if (act->ud_exit_cb)
        act->ud_exit_cb(act->userdata);

    assert(p->stack->idx_top == 1);    
    return;

err:    
    report_error(p, "exchange_file' << 0 >>\n");
}

void p21_exchange_file(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;
    
    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c
        P21_START { PUSH_SYMBOL(T_P21_START); continue;} */
    }
    PUSH_TERMINAL(p->stack, lpop(in, T_P21_START));

    p21_header_section(p, act);
    p21_data_section_list(p, act);

    PUSH_TERMINAL(p->stack, lpop(in, T_P21_END));
    
    if (p->error)
        goto err;
    
    /* user action */
    if (act->exchange_file_cb)
        act->exchange_file_cb(p, bsp, act->userdata);
    
    /* default reduction */
    p->stack->items[bsp] = (Symbol){P_FILE};
    drop(p->stack, p->stack->idx_top - bsp - 1);
    
    return;

err:
    report_error(p, "exchange_file << 1 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_FILE);
    else default_error_handler(p, bsp, P_FILE);
    recover(in, T_EOF);
}

void p21_header_section(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;
    
    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c re2c:labelprefix = 'yya';
        HEADER { PUSH_SYMBOL(T_HEADER); continue; } */
    }
    PUSH_TERMINAL(p->stack, lpop(in, T_HEADER));

    /* section callback */
    if (act->header_start_cb)
        act->header_start_cb(p, bsp, act->userdata);
    
    /* mandatory headers */
    p21_header_entity(p, act);
    p21_header_entity(p, act);
    p21_header_entity(p, act);
   
    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c re2c:labelprefix = 'yyb';
         ENDSEC { PUSH_SYMBOL(T_ENDSEC); continue; }
         KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; } */
    }
    
    /* optional headers */
    if (LOOKAHEAD(in->sym[0], T_KEYWORD))
        p21_header_entity_list(p, act);

    PUSH_TERMINAL(p->stack, lpop(in, T_ENDSEC));

    if (p->error)
        goto err;
    
    /* default reduction */
    p->stack->items[bsp] = (Symbol){P_HEADERSECTION};
    drop(p->stack, p->stack->idx_top - bsp - 1);
    
    return;
    
err:
    report_error(p, "header_section << 2 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_HEADERSECTION);
    else default_error_handler(p, bsp, P_HEADERSECTION);
    recover(in, T_DATA);
}

void p21_data_section_list(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    uint32_t len = 0;
    Input *in = p->in;

    do {
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c
             DATA / PUNCTUATION { PUSH_SYMBOL(T_DATA); continue; }
             P21_END { PUSH_SYMBOL(T_P21_END); continue; } */
        }
        if (!LOOKAHEAD(in->sym[0], T_DATA))
            break;
        p21_data_section(p, act);
    } while (++len);

    /* one or more */
    if (!len) {
        push(p->stack, (Symbol){T_ERROR, T_DATA, 0, in->sym[0].lineno});
        p->error = true;
    }
    
    if(p->error)
        goto err;
    
    /* user action */
    if (act->data_section_list_cb)
        act->data_section_list_cb(p, bsp, act->userdata);

    /* default reduction */
    p->stack->items[bsp] = (Symbol){P_LIST};
    drop(p->stack, p->stack->idx_top - bsp - 1);
    
    return;
    
err:
    report_error(p, "data_section_list << 3 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, T_P21_END);
}

void p21_data_section(P21Parser *p, P21ParserActions *act) {
    size_t n, cxt;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    while (in->nsym < 2) {
        in->sp = in->cur;
        /*!use:re2c re2c:labelprefix = 'yya';
         [(;] { PUSH_SYMBOL(*in->sp); continue; } */
    }
    PUSH_TERMINAL(p->stack, lpop(in, T_DATA));
    
    if (LOOKAHEAD(in->sym[0], '(')) {
        PUSH_TERMINAL(p->stack, lpop(in, '('));
        
        p21_parameter_list(p, act);
        while (in->nsym < 2) {
            in->sp = in->cur;
            /*!use:re2c re2c:labelprefix = 'yyb';
            ";" { PUSH_SYMBOL(';'); continue; } */
        }
        
        PUSH_TERMINAL(p->stack, lpop(in, ')'));
    }
    PUSH_TERMINAL(p->stack, lpop(in, ';'));
    
    if (act->data_start_cb)
        act->data_start_cb(p, bsp, act->userdata);

    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c re2c:labelprefix = 'yyc';
        ENDSEC { PUSH_SYMBOL(T_ENDSEC); continue; }
        EID { PUSH_SYMBOL(T_EID); continue; } */
    }
    if (LOOKAHEAD(in->sym[0], T_EID))
        p21_entity_instance_list(p, act);
    
    PUSH_TERMINAL(p->stack, lpop(in, T_ENDSEC));
    
    if (p->error)
        goto err;
    
    /* default reduction */
    p->stack->items[bsp] = (Symbol){P_DATASECTION};
    drop(p->stack, p->stack->idx_top - bsp - 1);
    
    return;

err:
    report_error(p, "data_section << 4 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_DATASECTION);
    else default_error_handler(p, bsp, P_DATASECTION);
    recover(in, T_P21_END, T_DATA);
}

void p21_header_entity(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;
    
    /* set KEYWORD as basemrk to prevent fill() recycling the buffer before user action */
    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c
         KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; } */
    }
    
    /* set KEYWORD as basemrk to prevent fill() recycling the buffer before user action */
    assert(in->nsym == 1);
    in->basemrk += in->sym[0].offset;
    in->sym[0].offset = 0;

    p21_simple_record(p, act);
    
    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c
         ";" { PUSH_SYMBOL(';'); continue; } */
    }
    PUSH_TERMINAL(p->stack, lpop(in, ';'));

    if (p->error)
        goto err;
    
    /* user action */
    if (act->header_entity_cb)
        act->header_entity_cb(p, bsp, act->userdata);

    /* reduction */
    assert(!p->hold);
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_HEADERENTITY};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "header_entity << 5 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_HEADERENTITY);
    else default_error_handler(p, bsp, P_HEADERENTITY);
    recover(in, T_ENDSEC, T_KEYWORD);
}

void p21_header_entity_list(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    p21_header_entity(p, act);
    
    do {
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c
             KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; }
             ENDSEC { PUSH_SYMBOL(T_ENDSEC); continue; } */
        }
        if (!LOOKAHEAD(in->sym[0], T_KEYWORD))
            break;
        p21_header_entity(p, act);
    } while (1);
        
    if (p->error)
        goto err;
    
    
    /* user action */
    if (act->header_entity_list_cb)
        act->header_entity_list_cb(p, bsp, act->userdata);

    /* reduction */
    assert(!p->hold);
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_LIST};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "header_entity_list << 6 >>");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, T_ENDSEC);
}

void p21_entity_instance_list(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    p21_entity_instance(p, act);
    
    do {
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c
             EID { PUSH_SYMBOL(T_EID); continue; }
             ENDSEC { PUSH_SYMBOL(T_ENDSEC); continue; } */
        }
        if (!LOOKAHEAD(in->sym[0], T_EID))
            break;
        p21_entity_instance(p, act);
    } while (1);
    
    if (p->error)
        goto err;
    
    /* user action */
    if (act->entity_instance_list_cb)
        act->entity_instance_list_cb(p, bsp, act->userdata);

    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_LIST};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "entity_instance_list << 7 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, T_ENDSEC);
}

void p21_parameter_list(P21Parser *p, P21ParserActions *act) {    
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    p21_parameter(p, act);

    do {
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c
             [,)] { PUSH_SYMBOL(*in->sp); continue; } */
        }
        if (LOOKAHEAD(in->sym[0], ')'))
            break;

        PUSH_TERMINAL(p->stack, lpop(in, ','));
        p21_parameter(p, act);
    } while (1);

    if (p->error)
        goto err;
    
    /* user action */
    if (act->parameter_list_cb)
        act->parameter_list_cb(p, bsp, act->userdata);
    
    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_LIST};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }

    return;
    
err:
    report_error(p, "parameter_list << 8 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, ')', ';');
}

void p21_entity_instance(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    /* set EID as basemrk to prevent fill() recycling the buffer before user action */
    assert(in->nsym == 1);
    in->basemrk += in->sym[0].offset;
    in->sym[0].offset = 0;

    while (in->nsym < 3) {
        in->sp = in->cur;
        /*!use:re2c
         KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; }
         [(=] { PUSH_SYMBOL(*in->sp); continue; } */
    }
    if (!LOOKAHEAD(in->sym[0], T_EID) || !LOOKAHEAD(in->sym[1], '='))
        goto err;
        
    if (LOOKAHEAD(in->sym[2], T_KEYWORD)) {
        p21_simple_entity_instance(p, act);
    } else if (LOOKAHEAD(in->sym[2], '(')) {
        p21_complex_entity_instance(p, act);
    }

    if (p->error)
        goto err;
    
    /* user action */
    if (act->entity_instance_cb)
        act->entity_instance_cb(p, bsp, act->userdata);
    
    /* no default reduction */
    
    return;
    
err:
    report_error(p, "entity_instance << 9 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, T_ENDSEC, T_EID);
}

void p21_simple_entity_instance(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    PUSH_TERMINAL(p->stack, lpop(in, T_EID));
    PUSH_TERMINAL(p->stack, lpop(in, '='));
    
    p21_simple_record(p, act);

    while (in->nsym < 1) {
        in->sp = in->cur;
        /*!use:re2c
         ";" { PUSH_SYMBOL(';'); continue; } */
    }
    PUSH_TERMINAL(p->stack, lpop(in, ';'));

    if (p->error)
        goto err;
    
    /* user action */
    if (act->simple_entity_instance_cb)
        act->simple_entity_instance_cb(p, bsp, act->userdata);

    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_SIMPLEENTITY};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }

    return;
    
err:
    report_error(p, "simple_entity_instance << 10 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_SIMPLEENTITY);
    else default_error_handler(p, bsp, P_SIMPLEENTITY);
    recover(in, T_ENDSEC, T_EID);
}


void p21_complex_entity_instance(P21Parser *p, P21ParserActions *act) {
    size_t n, c;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    PUSH_TERMINAL(p->stack, lpop(in, T_EID));
    PUSH_TERMINAL(p->stack, lpop(in, '='));
    PUSH_TERMINAL_EXT(c, p->stack, lpop(in, '('));
    
    p21_simple_record_list(p, act);

    while (in->nsym < 2) {
        in->sp = in->cur;
        /*!use:re2c
         ";" { PUSH_SYMBOL(';'); continue; } */
    }
    
    PUSH_TERMINAL_EXT(c, p->stack, lpop(in, ')'));
    PUSH_TERMINAL(p->stack, lpop(in, ';'));

    if (p->error) 
        goto err;
    
    /* user action */
    if (act->complex_entity_instance_cb)
        act->complex_entity_instance_cb(p, bsp, act->userdata);

    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_COMPLEXENTITY};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "complex_entity_instance << 11 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_COMPLEXENTITY);
    else default_error_handler(p, bsp, P_COMPLEXENTITY);
    recover(in, T_ENDSEC, T_EID);
}

void p21_simple_record(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    while (in->nsym < 3) {
        in->sp = in->cur;
        /*!use:re2c
         KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; }
         REAL { PUSH_SYMBOL(T_VARIANT, V_REAL); continue; }
         INTEGER { PUSH_SYMBOL(T_VARIANT, V_INTEGER); continue; }
         STRING { PUSH_SYMBOL(T_VARIANT, V_STRING); continue; }
         BINARY { PUSH_SYMBOL(T_VARIANT, V_BINARY); continue; }
         ENUMERATION { PUSH_SYMBOL(T_VARIANT, V_ENUMERATION); continue; }
         EID { PUSH_SYMBOL(T_VARIANT, V_EID); continue; }
         "*" { PUSH_SYMBOL(T_VARIANT, V_DERIVED); continue; }
         "$" { PUSH_SYMBOL(T_VARIANT, V_EMPTY); continue; }
         [()] { PUSH_SYMBOL(*in->sp); continue; } */
    }

    PUSH_TERMINAL(p->stack, lpop(in, T_KEYWORD));
    PUSH_TERMINAL_EXT(n, p->stack, lpop(in, '('));
    
    if (LOOKAHEAD(in->sym[0], '(', T_KEYWORD, T_VARIANT))
        p21_parameter_list(p, act);

    PUSH_TERMINAL_EXT(n, p->stack, lpop(in, ')'));

    if (p->error)
        goto err;
    
    /* user action */
    if (act->simple_record_cb)
        act->simple_record_cb(p, bsp, act->userdata);
    
    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_SIMPLERECORD};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "simple_record << 12 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_SIMPLERECORD);
    else default_error_handler(p, bsp, P_SIMPLERECORD);
    recover(in, ';', ')', T_KEYWORD);
}

void p21_simple_record_list(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    p21_simple_record(p, act);
    
    do {
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c
             KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; }
             ")" { PUSH_SYMBOL(')'); continue; } */
        }
        if (!LOOKAHEAD(in->sym[0], T_KEYWORD))
            break;
        p21_simple_record(p, act);
    } while (1);

    if (p->error)
        goto err;
    
    /* user action */
    if (act->simple_record_list_cb)
        act->simple_record_list_cb(p, bsp, act->userdata);

    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_LIST};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;
    
err:
    report_error(p, "simple_record_list << 13 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_LIST);
    else default_error_handler(p, bsp, P_LIST);
    recover(in, ')', ';');
}

void p21_parameter(P21Parser *p, P21ParserActions *act) {
    size_t n;
    uint32_t bsp = p->stack->idx_top;
    Input *in = p->in;

    while (in->nsym < 2) {
        in->sp = in->cur;
        /*!use:re2c re2c:labelprefix='yya';
         KEYWORD { PUSH_SYMBOL(T_KEYWORD); continue; }
         REAL { PUSH_SYMBOL(T_VARIANT, V_REAL); continue; }
         INTEGER { PUSH_SYMBOL(T_VARIANT, V_INTEGER); continue; }
         STRING { PUSH_SYMBOL(T_VARIANT, V_STRING); continue; }
         BINARY { PUSH_SYMBOL(T_VARIANT, V_BINARY); continue; }
         ENUMERATION { PUSH_SYMBOL(T_VARIANT, V_ENUMERATION); continue; }
         EID { PUSH_SYMBOL(T_VARIANT, V_EID); continue; }
         "*" { PUSH_SYMBOL(T_VARIANT, V_DERIVED); continue; }
         "$" { PUSH_SYMBOL(T_VARIANT, V_EMPTY); continue; }
         [(),] { PUSH_SYMBOL(*in->sp); continue; } */
    }
    
    if (LOOKAHEAD(in->sym[0], T_VARIANT)) {
        PUSH_TERMINAL(p->stack, lpop(in, T_VARIANT));
    } else {
        if (LOOKAHEAD(in->sym[0], T_KEYWORD)) {
            PUSH_TERMINAL(p->stack, lpop(in, T_KEYWORD));
            PUSH_TERMINAL(p->stack, lpop(in, '('));
            p21_parameter(p, act);
        } else {
            PUSH_TERMINAL(p->stack, lpop(in, '('));
            if (LOOKAHEAD(in->sym[0], '(', T_KEYWORD, T_VARIANT)) {
                p21_parameter_list(p, act);
            }
        }
        while (in->nsym < 1) {
            in->sp = in->cur;
            /*!use:re2c re2c:labelprefix='yyb';
             ")" { PUSH_SYMBOL(')'); continue; } */
        }
        PUSH_TERMINAL(p->stack, lpop(in, ')'));
    }
    
    if (p->error)
        goto err;
    
    /* user action */
    if (act->parameter_cb)
        act->parameter_cb(p, bsp, act->userdata);

    /* reduction */
    if (!p->hold) {
        p->stack->items[bsp] = (Symbol){P_PARAMETER};
        drop(p->stack, p->stack->idx_top - bsp - 1);
    }
    
    return;

err:
    report_error(p, "parameter << 14 >>\n");
    if (act->error_cb) act->error_cb(p, bsp, P_PARAMETER);
    else default_error_handler(p, bsp, P_PARAMETER);
    recover(in, ')', ',', ';');
}

void mock_error(P21Parser *, int, uint8_t);
void mock_ud_init(void *);
void mock_ud_exit(void *);
void mock_exchange_file(P21Parser *, int, void *);
void mock_header_start(P21Parser *, int, void *);
void mock_header_entity_list(P21Parser *, int, void *);
void mock_data_section_list(P21Parser *, int, void *);
void mock_data_start(P21Parser *, int, void *);
void mock_header_entity(P21Parser *, int, void *);
void mock_simple_entity_instance(P21Parser *, int, void *);
void mock_complex_entity_instance(P21Parser *, int, void *);
void mock_parameter_list(P21Parser *, int, void *);
void mock_parameter(P21Parser *, int, void *);
void mock_entity_instance_list(P21Parser *, int, void *);
void mock_entity_instance(P21Parser *, int, void *);
void mock_simple_record_list(P21Parser *, int, void *);
void mock_simple_record(P21Parser *, int, void *);
void mock_noop(P21Parser *, int, void *);

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *sec_stmt;
    sqlite3_stmt *sei_stmt;
    sqlite3_stmt *cei_stmt;
    sqlite3_stmt *hei_stmt;
    int section_idx;
} P21UserData;

P21UserData mockdata = {0};

P21ParserActions mockact = {
    .userdata = &mockdata,
    .error_cb = mock_error,
    .ud_init_cb = mock_ud_init,
    .ud_exit_cb = mock_ud_exit,
    .header_start_cb = mock_header_start,
    .data_start_cb = mock_data_start,
    .exchange_file_cb = NULL,
    .header_entity_list_cb = NULL,
    .data_section_list_cb = NULL,
    .header_entity_cb = mock_header_entity,
    .simple_entity_instance_cb = mock_simple_entity_instance,
    .complex_entity_instance_cb = mock_complex_entity_instance,
    .parameter_list_cb = mock_noop,
    .parameter_cb = mock_noop,
    .entity_instance_list_cb = NULL,
    .entity_instance_cb = NULL,
    .simple_record_list_cb = mock_noop,
    .simple_record_cb = mock_noop
};

void mock_error(P21Parser *p, int bsp, uint8_t cxt) { 
    switch (cxt) {
        case P_SIMPLEENTITY:
        case P_COMPLEXENTITY:
        case P_HEADERENTITY:
            dprintf("caught error: '%c'\n", cxt);
            p->error = false;
            drop(p->stack, p->stack->idx_top - bsp - 1);
            break;
        default:
            p->error = true;
            break;
    }
}


void mock_ud_init(void *d) {
    P21UserData *ud = d;
    char ddl_sql[] = 
        "PRAGMA foreign_keys = ON;\n"
        "CREATE TABLE entity_enum (type TEXT(1) PRIMARY KEY);\n"
        "INSERT INTO entity_enum (type) VALUES ('S'), ('C');\n"
        "CREATE TABLE section_enum (type TEXT(1) PRIMARY KEY);\n"
        "INSERT INTO section_enum (type) VALUES ('D'), ('H');\n"

        "CREATE TABLE section_table (\n"
        "    id INTEGER PRIMARY KEY,\n"
        "    lineno INTEGER NOT NULL,\n"
        "    section_type TEXT(1) NOT NULL REFERENCES section_enum(type)\n"
        ");\n"

        "CREATE TABLE section_headers (\n"
        "    id INTEGER PRIMARY KEY,\n"
        "    type_name TEXT COLLATE NOCASE,\n"
        "    raw_data TEXT NOT NULL,\n"
        "    lineno INTEGER NOT NULL,\n"
        "    fk_section INTEGER NOT NULL REFERENCES section_table(id)\n"
        ");\n"

        "CREATE TABLE data_table (\n"
        "    id TEXT PRIMARY KEY,\n"
        "    type_name TEXT COLLATE NOCASE,\n"
        "    raw_data TEXT NOT NULL,\n"
        "    lineno INTEGER NOT NULL,\n"
        "    entity_type TEXT(1) NOT NULL REFERENCES entity_enum(type),\n"
        "    fk_section INTEGER NOT NULL REFERENCES section_table(id)\n"
        ") WITHOUT ROWID;\n"
        
        "BEGIN DEFERRED TRANSACTION;";

    char sei_sql[] = "INSERT INTO data_table VALUES (?,?,?,?,'S',?)";
    char cei_sql[] = "INSERT INTO data_table VALUES (?,NULL,?,?,'C',?)";
    char hei_sql[] = "INSERT INTO section_headers(type_name, raw_data, lineno, fk_section) VALUES (?, ?, ?, ?)";
    int rc;
    
    rc = sqlite3_open_v2(":memory:", &ud->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    
    /* TODO: read ddl sql from external file */
    rc = sqlite3_exec(ud->db, ddl_sql, NULL, NULL, NULL);
    
    rc |= sqlite3_prepare_v3(ud->db, sei_sql, sizeof sei_sql, SQLITE_PREPARE_PERSISTENT, &ud->sei_stmt, NULL);
    rc |= sqlite3_prepare_v3(ud->db, cei_sql, sizeof cei_sql, SQLITE_PREPARE_PERSISTENT, &ud->cei_stmt, NULL);
    rc |= sqlite3_prepare_v3(ud->db, hei_sql, sizeof hei_sql, SQLITE_PREPARE_PERSISTENT, &ud->hei_stmt, NULL);
    
    if (rc != SQLITE_OK)
        exit(1);
    
    ud->section_idx = 0;
}

void mock_ud_exit(void *d) {
    P21UserData *ud = d;
    int rc;
    char ddl_sql[] = 
        "CREATE INDEX ix_type_name ON data_table(type_name);\n"
        "CREATE INDEX ix_entity_type ON data_table(entity_type);\n"
        "CREATE INDEX ix_fk_section ON data_table(fk_section);";

    rc = sqlite3_finalize(ud->sei_stmt);
    rc |= sqlite3_finalize(ud->cei_stmt);
    rc |= sqlite3_finalize(ud->hei_stmt);
    if (rc != SQLITE_OK) goto err;

    rc = sqlite3_exec(ud->db, "COMMIT TRANSACTION", NULL, NULL, NULL);
    if (rc != SQLITE_OK) goto err;
    
    /* TODO: benchmark index creation here vs on db init */
    rc = sqlite3_exec(ud->db, ddl_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) goto err;
    
    rc = sqlite3_close(ud->db);
    if (rc != SQLITE_OK) goto err;
    
    return;
err:
    dprintf("db error\n");
    exit(1);
}

void mock_exchange_file(P21Parser *p, int bsp, void *d) { Stack *s = p->stack; }
void mock_header_start(P21Parser *p, int bsp, void *d) {
    char sec_sql[] = "INSERT INTO section_table VALUES(?,?,'H')";
    Stack *s = p->stack;
    P21UserData *ud = d;
    Symbol *t = s->items + bsp;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(ud->db, sec_sql, sizeof sec_sql, &stmt, NULL);
    if (rc != SQLITE_OK) goto err;
    
    rc |= sqlite3_bind_int(stmt, 1, ++ud->section_idx);
    rc |= sqlite3_bind_int(stmt, 2, t->lineno);
    if (rc != SQLITE_OK) goto err;
    
    rc |= sqlite3_step(stmt);
    if (rc != SQLITE_DONE) goto err;
    
    sqlite3_finalize(stmt);

    /*
    s->items[bsp] = (Symbol){P_HEADERSECTION};
    drop(s, s->idx_top - bsp - 1);
    */
    
    return;
    
err:
    dprintf("db error\n");
    exit(1);
}
void mock_header_entity_list(P21Parser *p, int bsp, void *d) { Stack *s = p->stack; }
void mock_data_section_list(P21Parser *p, int bsp, void *d) { Stack *s = p->stack; }
void mock_data_start(P21Parser *p, int bsp, void *d) {
    char sec_sql[] = "INSERT INTO section_table VALUES(?,?,'D')";
    Stack *s = p->stack;
    P21UserData *ud = d;
    Symbol *t = s->items + bsp;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(ud->db, sec_sql, sizeof sec_sql, &stmt, NULL);
    if (rc != SQLITE_OK) goto err;
    
    rc |= sqlite3_bind_int(stmt, 1, ++ud->section_idx);
    rc |= sqlite3_bind_int(stmt, 2, t->lineno);
    if (rc != SQLITE_OK) goto err;
    
    rc |= sqlite3_step(stmt);
    if (rc!= SQLITE_DONE) goto err;
    
    sqlite3_finalize(stmt);
        
    return;
    
err:
    dprintf("db error\n");
    exit(1);
}

void mock_header_entity(P21Parser *p, int bsp, void *d) {
    Stack *s = p->stack;
    P21UserData *ud = d;
    HeaderEntity e = {s->items + bsp, s->items + bsp + 1};
    size_t i, nargs = e.args->n;
    unsigned char *basemrk = p->in->basemrk;
    ptrdiff_t ep;
    int rc;
    
    /* rewrite (normalise) args member before bind */
    e.args->offset = (e.args + 1)->offset;
    e.args->n = (e.args + 1)->n;
    for (i = 2, ep = e.args->offset + 1; i < nargs; i++) {
        Symbol *t = e.args + i;
        if (t->token == '(') t->n = 1;
        if (ep != t->offset) memmove(basemrk + ep, basemrk + t->offset, t->n);
        ep += t->n;
    }
    e.args->n = ep - e.args->offset;
    
    rc = sqlite3_reset(ud->hei_stmt);
    if (rc != SQLITE_OK) goto err;
    
    rc =  sqlite3_bind_text(ud->hei_stmt, 1, basemrk + e.kw->offset, e.kw->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(ud->hei_stmt, 2, basemrk + e.args->offset, e.args->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(ud->hei_stmt, 3, e.kw->lineno);
    rc |= sqlite3_bind_int(ud->hei_stmt, 4, ud->section_idx);
    if (rc != SQLITE_OK) goto err;
    
    rc = sqlite3_step(ud->hei_stmt);
    if (rc != SQLITE_DONE) goto err;
    
    p->hold = false;
    return;
    
err:
    mock_error(p, bsp, P_HEADERENTITY);
    dprintf("db error\n");
}

void mock_simple_entity_instance(P21Parser *p, int bsp, void *d) {
    Stack *s = p->stack;
    P21UserData *ud = d;
    SimpleEntity e = {s->items + bsp, s->items + bsp + 1, s->items + bsp + 2, s->items + bsp + 3};
    size_t i, nargs = e.args->n;
    unsigned char *basemrk = p->in->basemrk;
    ptrdiff_t ep;
    int rc;
    
    /* rewrite (normalise) args before bind */
    e.args->offset = (e.args + 1)->offset;
    e.args->n = (e.args + 1)->n;
    for (i = 2, ep = e.args->offset + e.args->n; i < nargs; i++) {
        Symbol *t = e.args + i;
        if (t->token == '(') t->n = 1;
        if (ep != t->offset) memmove(basemrk + ep, basemrk + t->offset, t->n);
        ep += t->n;
    }
    e.args->n = ep - e.args->offset;

    /* */
    rc = sqlite3_reset(ud->sei_stmt);
    if (rc != SQLITE_OK) goto err;

    rc =  sqlite3_bind_text(ud->sei_stmt, 1, basemrk + e.eid->offset, e.eid->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(ud->sei_stmt, 2, basemrk + e.kw->offset, e.kw->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(ud->sei_stmt, 3, basemrk + e.args->offset, e.args->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(ud->sei_stmt, 4, e.eid->lineno);
    rc |= sqlite3_bind_int(ud->sei_stmt, 5, ud->section_idx);
    if (rc != SQLITE_OK) goto err;
    
    rc = sqlite3_step(ud->sei_stmt);
    if (rc != SQLITE_DONE) goto err;
    
    p->hold = false;    
    
    return;
    
err:
    mock_error(p, bsp, P_SIMPLEENTITY);
    dprintf("db error\n");
}


void mock_complex_entity_instance(P21Parser *p, int bsp, void *d) {
    Stack *s = p->stack;
    P21UserData *ud = d;
    ComplexEntity e = {s->items + bsp, s->items + bsp + 1, s->items + bsp + 2};
    size_t i, nsubsupers = e.subsupers->n;
    unsigned char *basemrk = p->in->basemrk;
    ptrdiff_t ep;
    int rc;
    
    /* rewrite (normalise) list before bind */
    for (i = 1, ep = e.subsupers->offset + 1; i < nsubsupers; i++) {
        Symbol *t = e.subsupers + i;
        if (t->token == '(') t->n = 1;
        if (ep != t->offset) memmove(basemrk + ep, basemrk + t->offset, t->n);
        ep += t->n;
    }
    e.subsupers->n = ep - e.subsupers->offset;
    
    rc = sqlite3_reset(ud->cei_stmt);
    if (rc != SQLITE_OK) goto err;
    
    rc = sqlite3_bind_text(ud->cei_stmt, 1, basemrk + e.eid->offset, e.eid->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(ud->cei_stmt, 2, basemrk + e.subsupers->offset, e.subsupers->n, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(ud->cei_stmt, 3, e.eid->lineno);
    rc |= sqlite3_bind_int(ud->cei_stmt, 4, ud->section_idx);
    if (rc != SQLITE_OK) goto err;
    
    rc = sqlite3_step(ud->cei_stmt);
    if (rc != SQLITE_DONE) goto err;

    p->hold = false;
    return;
    
err:
    mock_error(p, bsp, P_COMPLEXENTITY);
    dprintf("db error \n");
}

void mock_parameter_list(P21Parser *p, int bsp, void *d) { }
void mock_parameter(P21Parser *p, int bsp, void *d) { }
void mock_entity_instance_list(P21Parser *p, int bsp, void *d) { }
void mock_entity_instance(P21Parser *p, int bsp, void *d) { }
void mock_simple_record_list(P21Parser *p, int bsp, void *d) { }
void mock_simple_record(P21Parser *p, int bsp, void *d) {}

void mock_noop(P21Parser *p, int bsp, void *d) {
    p->hold = true;
}

int main(char *argv[], int argc) {
    const char *paths[] = {
        "/home/chorler/projects/src/stepcode/test/p21/test_array_bounds_FAIL1.p21",
        "/home/chorler/projects/src/stepcode/test/p21/comments.p21",
        "/home/chorler/projects/src/stepcode/test/p21/test_inverse_attr.p21",
        "/home/chorler/projects/src/stepcode/test/p21/missing_and_required.p21",
        "/home/chorler/projects/src/stepcode/test/p21/test_array_bounds.p21",
        "/home/chorler/projects/src/stepcode/test/p21/test_inherit_inverse.p21",
        "/home/chorler/projects/src/stepcode/data/ap214e3/as1-oc-214.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/dm1-id-214.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/MAINBODY.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/HEAD_BACK.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/HEAD_FRONT.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/TAIL.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/MAINBODY_FRONT.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/FOOT_BACK_000.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/FOOT_FRONT_000.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/s1-c5-214.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/MAINBODY_BACK.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/HEAD.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/TAIL_TURBINE.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/TAIL_MIDDLE_PART.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/s1-c5-214/FOOT.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/sg1-c5-214.stp",
        "/home/chorler/projects/src/stepcode/data/ap214e3/io1-cm-214.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS7-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS1Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS2-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS1Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS3-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS10Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS2Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS8-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS3Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS4Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS7Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS4Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS10Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS3Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS8Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS4-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS7Mod0-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS2Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS8Mod0-outresult.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS10-out.stp",
        "/home/chorler/projects/src/stepcode/data/ap209/ATS1-out.stp"
    };
    
    P21Parser myp;
    P21UserData mydata;
    FILE *fp;
    memset(&mydata, 0, sizeof mydata);
    
    for (unsigned int i = 0; i < (sizeof paths / sizeof paths[0]); i++) {
        fp = fopen(paths[i], "rb");
        if (!fp) { fprintf(stderr, "failed to read input: %s\n", paths[i]); continue; }
        else { fprintf(stderr, "processing: %s\n", paths[i]); }
        p21_init(&myp, fp);
        p21_parse(&myp, &mockact);
    }
}
