/*                     S C A D _ P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file scad_parse.c
 *
 * Lexer and recursive-descent parser for the OpenSCAD language.  The
 * grammar implemented here covers the declarative subset that maps to
 * geometry: variable/module/function definitions, module instantiation
 * with modifiers, control flow (if/for/intersection_for/let), the full
 * expression grammar (arithmetic, comparison, logical, ternary, vector
 * literals, ranges, indexing, member/swizzle access, function calls,
 * let-expressions and list comprehensions), and include/use directives.
 *
 * Output is an array of struct scad_stmt (see scad.h); evaluation and
 * geometry generation happen in scad_eval.c / scad_geom.c.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#include "scad.h"


/* ------------------------------------------------------------------ */
/* tokens                                                             */
/* ------------------------------------------------------------------ */

enum tok_type {
    T_EOF = 0, T_NUM, T_STR, T_ID, T_PATH,
    T_LPAREN, T_RPAREN, T_LBRACK, T_RBRACK, T_LBRACE, T_RBRACE,
    T_SEMI, T_COMMA, T_COLON, T_DOT, T_QUEST,
    T_ASSIGN, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT, T_HASH,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE, T_AND, T_OR, T_NOT,
    /* keywords */
    T_MODULE, T_FUNCTION, T_IF, T_ELSE, T_FOR, T_INTFOR, T_LET, T_EACH,
    T_TRUE, T_FALSE, T_UNDEF, T_INCLUDE, T_USE
};

struct tok {
    enum tok_type type;
    double num;
    char *str;			/* identifier / string / path text */
    int line;
};


/* ------------------------------------------------------------------ */
/* small growable pointer vector                                      */
/* ------------------------------------------------------------------ */

struct ptrvec {
    void **v;
    size_t n, cap;
};

static void
pv_push(struct ptrvec *p, void *e)
{
    if (p->n == p->cap) {
	p->cap = p->cap ? p->cap * 2 : 8;
	p->v = (void **)bu_realloc(p->v, p->cap * sizeof(void *), "scad ptrvec");
    }
    p->v[p->n++] = e;
}


/* ------------------------------------------------------------------ */
/* lexer                                                              */
/* ------------------------------------------------------------------ */

struct lexer {
    const char *s;
    size_t len, pos;
    int line;
    struct ptrvec out;		/* of struct tok* */
    int error;
};

static struct tok *
tok_new(struct lexer *L, enum tok_type t)
{
    struct tok *tk;
    BU_ALLOC(tk, struct tok);
    tk->type = t;
    tk->line = L->line;
    return tk;
}

static int
kw_lookup(const char *id)
{
    if (BU_STR_EQUAL(id, "module")) return T_MODULE;
    if (BU_STR_EQUAL(id, "function")) return T_FUNCTION;
    if (BU_STR_EQUAL(id, "if")) return T_IF;
    if (BU_STR_EQUAL(id, "else")) return T_ELSE;
    if (BU_STR_EQUAL(id, "for")) return T_FOR;
    if (BU_STR_EQUAL(id, "intersection_for")) return T_INTFOR;
    if (BU_STR_EQUAL(id, "let")) return T_LET;
    if (BU_STR_EQUAL(id, "each")) return T_EACH;
    if (BU_STR_EQUAL(id, "true")) return T_TRUE;
    if (BU_STR_EQUAL(id, "false")) return T_FALSE;
    if (BU_STR_EQUAL(id, "undef")) return T_UNDEF;
    if (BU_STR_EQUAL(id, "include")) return T_INCLUDE;
    if (BU_STR_EQUAL(id, "use")) return T_USE;
    return T_ID;
}

/* read a <...> path token following include/use */
static void
lex_path(struct lexer *L)
{
    size_t start;
    struct tok *tk;
    L->pos++;			/* skip '<' */
    start = L->pos;
    while (L->pos < L->len && L->s[L->pos] != '>' && L->s[L->pos] != '\n')
	L->pos++;
    tk = tok_new(L, T_PATH);
    tk->str = (char *)bu_malloc(L->pos - start + 1, "scad path");
    memcpy(tk->str, L->s + start, L->pos - start);
    tk->str[L->pos - start] = '\0';
    if (L->pos < L->len && L->s[L->pos] == '>')
	L->pos++;
    pv_push(&L->out, tk);
}

static void
lex_string(struct lexer *L)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct tok *tk;
    L->pos++;			/* opening quote */
    while (L->pos < L->len && L->s[L->pos] != '"') {
	char c = L->s[L->pos];
	if (c == '\\' && L->pos + 1 < L->len) {
	    char n = L->s[++L->pos];
	    switch (n) {
		case 'n': bu_vls_putc(&v, '\n'); break;
		case 't': bu_vls_putc(&v, '\t'); break;
		case 'r': bu_vls_putc(&v, '\r'); break;
		case '"': bu_vls_putc(&v, '"'); break;
		case '\\': bu_vls_putc(&v, '\\'); break;
		default: bu_vls_putc(&v, n); break;
	    }
	} else {
	    if (c == '\n') L->line++;
	    bu_vls_putc(&v, c);
	}
	L->pos++;
    }
    if (L->pos < L->len) L->pos++;	/* closing quote */
    tk = tok_new(L, T_STR);
    tk->str = bu_strdup(bu_vls_cstr(&v));
    bu_vls_free(&v);
    pv_push(&L->out, tk);
}

static void
lex_number(struct lexer *L)
{
    char *end = NULL;
    struct tok *tk = tok_new(L, T_NUM);
    tk->num = strtod(L->s + L->pos, &end);
    L->pos = (size_t)(end - L->s);
    pv_push(&L->out, tk);
}

static void
lex_ident(struct lexer *L)
{
    size_t start = L->pos;
    struct tok *tk;
    int kw;
    if (L->s[L->pos] == '$')	/* special variable: $fn, $fa, $children, ... */
	L->pos++;
    while (L->pos < L->len &&
	   (isalnum((unsigned char)L->s[L->pos]) || L->s[L->pos] == '_'))
	L->pos++;
    {
	char *id = (char *)bu_malloc(L->pos - start + 1, "scad id");
	memcpy(id, L->s + start, L->pos - start);
	id[L->pos - start] = '\0';
	kw = kw_lookup(id);
	tk = tok_new(L, (enum tok_type)kw);
	if (kw == T_ID)
	    tk->str = id;
	else
	    bu_free(id, "scad id");
    }
    pv_push(&L->out, tk);
}

static void
lex_punct(struct lexer *L)
{
    char c = L->s[L->pos];
    char c2 = (L->pos + 1 < L->len) ? L->s[L->pos + 1] : '\0';
    enum tok_type t = T_EOF;
    int adv = 1;

    switch (c) {
	case '(': t = T_LPAREN; break;
	case ')': t = T_RPAREN; break;
	case '[': t = T_LBRACK; break;
	case ']': t = T_RBRACK; break;
	case '{': t = T_LBRACE; break;
	case '}': t = T_RBRACE; break;
	case ';': t = T_SEMI; break;
	case ',': t = T_COMMA; break;
	case ':': t = T_COLON; break;
	case '.': t = T_DOT; break;
	case '?': t = T_QUEST; break;
	case '+': t = T_PLUS; break;
	case '-': t = T_MINUS; break;
	case '*': t = T_STAR; break;
	case '/': t = T_SLASH; break;
	case '%': t = T_PERCENT; break;
	case '#': t = T_HASH; break;
	case '=': if (c2 == '=') { t = T_EQ; adv = 2; } else t = T_ASSIGN; break;
	case '!': if (c2 == '=') { t = T_NE; adv = 2; } else t = T_NOT; break;
	case '<': if (c2 == '=') { t = T_LE; adv = 2; } else t = T_LT; break;
	case '>': if (c2 == '=') { t = T_GE; adv = 2; } else t = T_GT; break;
	case '&': if (c2 == '&') { t = T_AND; adv = 2; } else { L->error = 1; } break;
	case '|': if (c2 == '|') { t = T_OR; adv = 2; } else { L->error = 1; } break;
	default:
	    bu_log("scad: unexpected character '%c' on line %d\n", c, L->line);
	    L->error = 1;
	    L->pos++;
	    return;
    }
    if (t != T_EOF)
	pv_push(&L->out, tok_new(L, t));
    L->pos += adv;
}

static struct tok **
lex_all(struct scad_state *st, const char *text, size_t *ntok_out, int *err)
{
    struct lexer L;
    int prev_incl = 0;		/* previous token was include/use */
    struct tok **arr;

    memset(&L, 0, sizeof(L));
    L.s = text;
    L.len = strlen(text);
    L.line = 1;

    while (L.pos < L.len) {
	char c = L.s[L.pos];

	if (c == '\n') { L.line++; L.pos++; continue; }
	if (isspace((unsigned char)c)) { L.pos++; continue; }

	/* comments */
	if (c == '/' && L.pos + 1 < L.len && L.s[L.pos + 1] == '/') {
	    while (L.pos < L.len && L.s[L.pos] != '\n') L.pos++;
	    continue;
	}
	if (c == '/' && L.pos + 1 < L.len && L.s[L.pos + 1] == '*') {
	    L.pos += 2;
	    while (L.pos + 1 < L.len && !(L.s[L.pos] == '*' && L.s[L.pos + 1] == '/')) {
		if (L.s[L.pos] == '\n') L.line++;
		L.pos++;
	    }
	    L.pos += 2;
	    continue;
	}

	/* include/use path */
	if (prev_incl && c == '<') {
	    lex_path(&L);
	    prev_incl = 0;
	    continue;
	}

	if (c == '"') { lex_string(&L); prev_incl = 0; continue; }
	if (isdigit((unsigned char)c) ||
	    (c == '.' && L.pos + 1 < L.len && isdigit((unsigned char)L.s[L.pos + 1]))) {
	    lex_number(&L);
	    prev_incl = 0;
	    continue;
	}
	if (isalpha((unsigned char)c) || c == '_' || c == '$') {
	    size_t before = L.out.n;
	    lex_ident(&L);
	    if (L.out.n > before) {
		struct tok *t = (struct tok *)L.out.v[L.out.n - 1];
		prev_incl = (t->type == T_INCLUDE || t->type == T_USE);
	    }
	    continue;
	}
	lex_punct(&L);
	prev_incl = 0;
	if (L.error) break;
    }

    pv_push(&L.out, tok_new(&L, T_EOF));

    (void)st;			/* the lexer needs no shared state */
    *err = L.error;
    *ntok_out = L.out.n;
    arr = (struct tok **)L.out.v;
    return arr;
}


/* ------------------------------------------------------------------ */
/* parser                                                             */
/* ------------------------------------------------------------------ */

struct parser {
    struct scad_state *st;
    struct tok **t;
    size_t n, pos;
    const char *fname;
    int error;
};

static struct scad_expr *parse_expr(struct parser *P);
static struct scad_stmt *parse_statement(struct parser *P);

static struct tok *
cur(struct parser *P)
{
    return P->t[P->pos < P->n ? P->pos : P->n - 1];
}

static enum tok_type
peek(struct parser *P)
{
    return cur(P)->type;
}

static struct tok *
advance(struct parser *P)
{
    struct tok *tk = cur(P);
    if (P->pos < P->n - 1) P->pos++;
    return tk;
}

static int
accept(struct parser *P, enum tok_type t)
{
    if (peek(P) == t) { advance(P); return 1; }
    return 0;
}

static int
expect(struct parser *P, enum tok_type t, const char *what)
{
    if (peek(P) == t) { advance(P); return 1; }
    if (!P->error)
	bu_log("scad: parse error in %s near line %d: expected %s\n",
	       P->fname ? P->fname : "(input)", cur(P)->line, what);
    P->error = 1;
    return 0;
}


/* expression node allocator */
static struct scad_expr *
expr_new(enum scad_expr_kind k)
{
    struct scad_expr *e;
    BU_ALLOC(e, struct scad_expr);
    e->kind = k;
    return e;
}


/* parse a comma-separated list of name=expr bindings (let/for) */
static struct scad_assign *
parse_assigns(struct parser *P)
{
    struct scad_assign *head = NULL, *tail = NULL;
    if (peek(P) == T_RPAREN)
	return NULL;
    do {
	struct scad_assign *a;
	if (peek(P) != T_ID) {
	    bu_log("scad: expected name in binding, line %d\n", cur(P)->line);
	    P->error = 1;
	    break;
	}
	BU_ALLOC(a, struct scad_assign);
	a->name = bu_strdup(advance(P)->str);
	if (accept(P, T_ASSIGN))
	    a->value = parse_expr(P);
	a->next = NULL;
	if (tail) tail->next = a; else head = a;
	tail = a;
    } while (accept(P, T_COMMA) && peek(P) != T_RPAREN);
    return head;
}


/* parse call arguments: (name=)? expr, comma separated */
static void
parse_args(struct parser *P, struct scad_arg **out, size_t *nout)
{
    struct ptrvec v;
    memset(&v, 0, sizeof(v));
    if (peek(P) != T_RPAREN) {
	do {
	    struct scad_arg *a;
	    if (peek(P) == T_RPAREN) break;	/* trailing comma */
	    BU_ALLOC(a, struct scad_arg);
	    if (peek(P) == T_ID && P->t[P->pos + 1]->type == T_ASSIGN) {
		a->name = bu_strdup(advance(P)->str);
		advance(P);	/* '=' */
	    }
	    a->value = parse_expr(P);
	    pv_push(&v, a);
	} while (accept(P, T_COMMA));
    }
    *out = (struct scad_arg *)v.v;	/* array of scad_arg* ; see note below */
    *nout = v.n;
}

/* NOTE: parse_args stores an array of (struct scad_arg *).  We expose
 * struct scad_arg* (array of structs) in the AST for compactness, so
 * convert the pointer array into a packed struct array. */
static void
pack_args(struct scad_arg **pp, size_t n)
{
    struct scad_arg *packed;
    struct scad_arg **ptrs = (struct scad_arg **)*pp;
    size_t i;
    if (!n) { *pp = NULL; if (ptrs) bu_free(ptrs, "scad args"); return; }
    packed = (struct scad_arg *)bu_calloc(n, sizeof(struct scad_arg), "scad args");
    for (i = 0; i < n; i++) {
	packed[i] = *ptrs[i];
	bu_free(ptrs[i], "scad arg");
    }
    bu_free(ptrs, "scad argp");
    *pp = packed;
}


/* parse one element of a vector / list-comprehension */
static struct scad_expr *
parse_vec_element(struct parser *P)
{
    if (peek(P) == T_FOR) {
	struct scad_expr *e = expr_new(EX_LC_FOR);
	advance(P);
	expect(P, T_LPAREN, "'('");
	/* reject unsupported C-style generator early */
	e->assigns = parse_assigns(P);
	expect(P, T_RPAREN, "')'");
	e->a = parse_vec_element(P);
	return e;
    }
    if (peek(P) == T_IF) {
	struct scad_expr *e = expr_new(EX_LC_IF);
	advance(P);
	expect(P, T_LPAREN, "'('");
	e->a = parse_expr(P);
	expect(P, T_RPAREN, "')'");
	e->b = parse_vec_element(P);
	if (accept(P, T_ELSE))
	    e->c = parse_vec_element(P);
	return e;
    }
    if (peek(P) == T_LET) {
	struct scad_expr *e = expr_new(EX_LC_LET);
	advance(P);
	expect(P, T_LPAREN, "'('");
	e->assigns = parse_assigns(P);
	expect(P, T_RPAREN, "')'");
	e->a = parse_vec_element(P);
	return e;
    }
    if (peek(P) == T_EACH) {
	struct scad_expr *e = expr_new(EX_EACH);
	advance(P);
	e->a = parse_vec_element(P);
	return e;
    }
    return parse_expr(P);
}


/* parse the contents after '[' : vector, range, or comprehension */
static struct scad_expr *
parse_bracket(struct parser *P)
{
    struct scad_expr *e0;
    struct ptrvec items;

    if (accept(P, T_RBRACK)) {	/* [] empty vector */
	struct scad_expr *e = expr_new(EX_VECTOR);
	return e;
    }

    if (peek(P) == T_FOR || peek(P) == T_IF || peek(P) == T_LET || peek(P) == T_EACH) {
	struct scad_expr *e = expr_new(EX_VECTOR);
	memset(&items, 0, sizeof(items));
	do {
	    if (peek(P) == T_RBRACK) break;
	    pv_push(&items, parse_vec_element(P));
	} while (accept(P, T_COMMA));
	expect(P, T_RBRACK, "']'");
	e->items = (struct scad_expr **)items.v;
	e->nitems = items.n;
	return e;
    }

    e0 = parse_expr(P);

    if (accept(P, T_COLON)) {	/* range */
	struct scad_expr *e = expr_new(EX_RANGE);
	struct scad_expr *m = parse_expr(P);
	e->a = e0;
	if (accept(P, T_COLON)) {
	    e->b = m;
	    e->c = parse_expr(P);
	} else {
	    e->b = NULL;
	    e->c = m;
	}
	expect(P, T_RBRACK, "']'");
	return e;
    }

    /* vector literal */
    {
	struct scad_expr *e = expr_new(EX_VECTOR);
	memset(&items, 0, sizeof(items));
	pv_push(&items, e0);
	while (accept(P, T_COMMA)) {
	    if (peek(P) == T_RBRACK) break;	/* trailing comma */
	    pv_push(&items, parse_vec_element(P));
	}
	expect(P, T_RBRACK, "']'");
	e->items = (struct scad_expr **)items.v;
	e->nitems = items.n;
	return e;
    }
}


static struct scad_expr *
parse_primary(struct parser *P)
{
    struct tok *tk = cur(P);

    switch (peek(P)) {
	case T_NUM: {
	    struct scad_expr *e = expr_new(EX_NUM);
	    e->num = advance(P)->num;
	    return e;
	}
	case T_STR: {
	    struct scad_expr *e = expr_new(EX_STR);
	    e->str = bu_strdup(advance(P)->str);
	    return e;
	}
	case T_TRUE: advance(P); { struct scad_expr *e = expr_new(EX_BOOL); e->num = 1; return e; }
	case T_FALSE: advance(P); { struct scad_expr *e = expr_new(EX_BOOL); e->num = 0; return e; }
	case T_UNDEF: advance(P); return expr_new(EX_UNDEF);
	case T_LPAREN: {
	    struct scad_expr *e;
	    advance(P);
	    e = parse_expr(P);
	    expect(P, T_RPAREN, "')'");
	    return e;
	}
	case T_LBRACK:
	    advance(P);
	    return parse_bracket(P);
	case T_LET: {
	    struct scad_expr *e = expr_new(EX_LET);
	    advance(P);
	    expect(P, T_LPAREN, "'('");
	    e->assigns = parse_assigns(P);
	    expect(P, T_RPAREN, "')'");
	    e->a = parse_expr(P);
	    return e;
	}
	case T_ID: {
	    char *name = bu_strdup(tk->str);
	    advance(P);
	    if (peek(P) == T_LPAREN) {	/* function call */
		struct scad_expr *e = expr_new(EX_CALL);
		advance(P);
		e->name = name;
		parse_args(P, &e->args, &e->nargs);
		pack_args(&e->args, e->nargs);
		expect(P, T_RPAREN, "')'");
		return e;
	    } else {
		struct scad_expr *e = expr_new(EX_IDENT);
		e->name = name;
		return e;
	    }
	}
	default:
	    if (!P->error)
		bu_log("scad: parse error near line %d: unexpected token in expression\n",
		       tk->line);
	    P->error = 1;
	    return expr_new(EX_UNDEF);
    }
}


static struct scad_expr *
parse_postfix(struct parser *P)
{
    struct scad_expr *e = parse_primary(P);
    for (;;) {
	if (accept(P, T_LBRACK)) {
	    struct scad_expr *idx = expr_new(EX_INDEX);
	    idx->a = e;
	    idx->b = parse_expr(P);
	    expect(P, T_RBRACK, "']'");
	    e = idx;
	} else if (accept(P, T_DOT)) {
	    struct scad_expr *m = expr_new(EX_MEMBER);
	    m->a = e;
	    if (peek(P) == T_ID)
		m->name = bu_strdup(advance(P)->str);
	    else { expect(P, T_ID, "member name"); }
	    e = m;
	} else {
	    break;
	}
    }
    return e;
}


static struct scad_expr *
parse_unary(struct parser *P)
{
    if (peek(P) == T_NOT || peek(P) == T_MINUS || peek(P) == T_PLUS) {
	int op = (peek(P) == T_NOT) ? SCAD_OP_NOT :
		 (peek(P) == T_MINUS) ? SCAD_OP_NEG : 0;
	advance(P);
	if (op == 0)	/* unary plus is a no-op */
	    return parse_unary(P);
	{
	    struct scad_expr *e = expr_new(EX_UNARY);
	    e->op = op;
	    e->a = parse_unary(P);
	    return e;
	}
    }
    return parse_postfix(P);
}


/* binary operator precedence helper */
static struct scad_expr *
mk_binary(int op, struct scad_expr *a, struct scad_expr *b)
{
    struct scad_expr *e = expr_new(EX_BINARY);
    e->op = op;
    e->a = a;
    e->b = b;
    return e;
}

static struct scad_expr *
parse_mul(struct parser *P)
{
    struct scad_expr *e = parse_unary(P);
    for (;;) {
	int op;
	if (peek(P) == T_STAR) op = SCAD_OP_MUL;
	else if (peek(P) == T_SLASH) op = SCAD_OP_DIV;
	else if (peek(P) == T_PERCENT) op = SCAD_OP_MOD;
	else break;
	advance(P);
	e = mk_binary(op, e, parse_unary(P));
    }
    return e;
}

static struct scad_expr *
parse_add(struct parser *P)
{
    struct scad_expr *e = parse_mul(P);
    for (;;) {
	int op;
	if (peek(P) == T_PLUS) op = SCAD_OP_ADD;
	else if (peek(P) == T_MINUS) op = SCAD_OP_SUB;
	else break;
	advance(P);
	e = mk_binary(op, e, parse_mul(P));
    }
    return e;
}

static struct scad_expr *
parse_rel(struct parser *P)
{
    struct scad_expr *e = parse_add(P);
    for (;;) {
	int op;
	if (peek(P) == T_LT) op = SCAD_OP_LT;
	else if (peek(P) == T_LE) op = SCAD_OP_LE;
	else if (peek(P) == T_GT) op = SCAD_OP_GT;
	else if (peek(P) == T_GE) op = SCAD_OP_GE;
	else break;
	advance(P);
	e = mk_binary(op, e, parse_add(P));
    }
    return e;
}

static struct scad_expr *
parse_eq(struct parser *P)
{
    struct scad_expr *e = parse_rel(P);
    for (;;) {
	int op;
	if (peek(P) == T_EQ) op = SCAD_OP_EQ;
	else if (peek(P) == T_NE) op = SCAD_OP_NE;
	else break;
	advance(P);
	e = mk_binary(op, e, parse_rel(P));
    }
    return e;
}

static struct scad_expr *
parse_and(struct parser *P)
{
    struct scad_expr *e = parse_eq(P);
    while (peek(P) == T_AND) {
	advance(P);
	e = mk_binary(SCAD_OP_AND, e, parse_eq(P));
    }
    return e;
}

static struct scad_expr *
parse_or(struct parser *P)
{
    struct scad_expr *e = parse_and(P);
    while (peek(P) == T_OR) {
	advance(P);
	e = mk_binary(SCAD_OP_OR, e, parse_and(P));
    }
    return e;
}

static struct scad_expr *
parse_expr(struct parser *P)
{
    struct scad_expr *e = parse_or(P);
    if (accept(P, T_QUEST)) {
	struct scad_expr *t = expr_new(EX_TERNARY);
	t->a = e;
	t->b = parse_expr(P);
	expect(P, T_COLON, "':'");
	t->c = parse_expr(P);
	return t;
    }
    return e;
}


/* ------------------------------------------------------------------ */
/* statements                                                         */
/* ------------------------------------------------------------------ */

static struct scad_param *
parse_params(struct parser *P)
{
    struct scad_param *head = NULL, *tail = NULL;
    if (peek(P) == T_RPAREN)
	return NULL;
    do {
	struct scad_param *p;
	if (peek(P) == T_RPAREN) break;
	if (peek(P) != T_ID) {
	    bu_log("scad: expected parameter name, line %d\n", cur(P)->line);
	    P->error = 1;
	    break;
	}
	BU_ALLOC(p, struct scad_param);
	p->name = bu_strdup(advance(P)->str);
	if (accept(P, T_ASSIGN))
	    p->def = parse_expr(P);
	p->next = NULL;
	if (tail) tail->next = p; else head = p;
	tail = p;
    } while (accept(P, T_COMMA));
    return head;
}


/* parse a brace-delimited or single statement into a body array */
static void
parse_body(struct parser *P, struct scad_stmt ***out, size_t *nout)
{
    struct ptrvec v;
    memset(&v, 0, sizeof(v));
    if (accept(P, T_LBRACE)) {
	while (peek(P) != T_RBRACE && peek(P) != T_EOF && !P->error) {
	    struct scad_stmt *s = parse_statement(P);
	    if (s) pv_push(&v, s);
	}
	expect(P, T_RBRACE, "'}'");
    } else {
	struct scad_stmt *s = parse_statement(P);
	if (s) pv_push(&v, s);
    }
    *out = (struct scad_stmt **)v.v;
    *nout = v.n;
}


static struct scad_stmt *
stmt_new(enum scad_stmt_kind k)
{
    struct scad_stmt *s;
    BU_ALLOC(s, struct scad_stmt);
    s->kind = k;
    return s;
}


static struct scad_stmt *
parse_statement(struct parser *P)
{
    int modifier = 0;

    /* leading modifier characters */
    while (peek(P) == T_STAR || peek(P) == T_NOT ||
	   peek(P) == T_HASH || peek(P) == T_PERCENT) {
	switch (peek(P)) {
	    case T_STAR: modifier = '*'; break;
	    case T_NOT: modifier = '!'; break;
	    case T_HASH: modifier = '#'; break;
	    case T_PERCENT: modifier = '%'; break;
	    default: break;
	}
	advance(P);
    }

    if (accept(P, T_SEMI))
	return NULL;		/* stray empty statement */

    switch (peek(P)) {
	case T_LBRACE: {
	    struct scad_stmt *s = stmt_new(ST_BLOCK);
	    s->modifier = modifier;
	    parse_body(P, &s->body, &s->nbody);
	    return s;
	}
	case T_INCLUDE:
	case T_USE: {
	    int use = (peek(P) == T_USE);
	    struct scad_stmt *s;
	    advance(P);
	    s = stmt_new(use ? ST_USE : ST_INCLUDE);
	    if (peek(P) == T_PATH)
		s->name = bu_strdup(advance(P)->str);
	    else
		expect(P, T_PATH, "<path>");
	    return s;
	}
	case T_MODULE: {
	    struct scad_stmt *s = stmt_new(ST_MODULE);
	    advance(P);
	    if (peek(P) == T_ID) s->name = bu_strdup(advance(P)->str);
	    else expect(P, T_ID, "module name");
	    expect(P, T_LPAREN, "'('");
	    s->params = parse_params(P);
	    expect(P, T_RPAREN, "')'");
	    parse_body(P, &s->body, &s->nbody);
	    return s;
	}
	case T_FUNCTION: {
	    struct scad_stmt *s = stmt_new(ST_FUNCTION);
	    advance(P);
	    if (peek(P) == T_ID) s->name = bu_strdup(advance(P)->str);
	    else expect(P, T_ID, "function name");
	    expect(P, T_LPAREN, "'('");
	    s->params = parse_params(P);
	    expect(P, T_RPAREN, "')'");
	    expect(P, T_ASSIGN, "'='");
	    s->expr = parse_expr(P);
	    expect(P, T_SEMI, "';'");
	    return s;
	}
	case T_IF: {
	    struct scad_stmt *s = stmt_new(ST_IF);
	    s->modifier = modifier;
	    advance(P);
	    expect(P, T_LPAREN, "'('");
	    s->expr = parse_expr(P);
	    expect(P, T_RPAREN, "')'");
	    parse_body(P, &s->body, &s->nbody);
	    if (accept(P, T_ELSE))
		parse_body(P, &s->elsebody, &s->nelse);
	    return s;
	}
	case T_FOR:
	case T_INTFOR: {
	    int isf = (peek(P) == T_INTFOR);
	    struct scad_stmt *s = stmt_new(isf ? ST_INT_FOR : ST_FOR);
	    s->modifier = modifier;
	    advance(P);
	    expect(P, T_LPAREN, "'('");
	    s->assigns = parse_assigns(P);
	    expect(P, T_RPAREN, "')'");
	    parse_body(P, &s->body, &s->nbody);
	    return s;
	}
	case T_LET: {
	    struct scad_stmt *s = stmt_new(ST_LET);
	    s->modifier = modifier;
	    advance(P);
	    expect(P, T_LPAREN, "'('");
	    s->assigns = parse_assigns(P);
	    expect(P, T_RPAREN, "')'");
	    parse_body(P, &s->body, &s->nbody);
	    return s;
	}
	case T_ID: {
	    char *name = bu_strdup(cur(P)->str);
	    advance(P);
	    if (accept(P, T_ASSIGN)) {	/* assignment */
		struct scad_stmt *s = stmt_new(ST_ASSIGN);
		s->name = name;
		s->expr = parse_expr(P);
		expect(P, T_SEMI, "';'");
		return s;
	    }
	    if (accept(P, T_LPAREN)) {	/* module instantiation */
		struct scad_stmt *s = stmt_new(ST_INSTANCE);
		s->modifier = modifier;
		s->name = name;
		parse_args(P, &s->args, &s->nargs);
		pack_args(&s->args, s->nargs);
		expect(P, T_RPAREN, "')'");
		if (!accept(P, T_SEMI))
		    parse_body(P, &s->body, &s->nbody);
		return s;
	    }
	    bu_log("scad: parse error near line %d: expected '=' or '(' after '%s'\n",
		   cur(P)->line, name);
	    bu_free(name, "scad name");
	    P->error = 1;
	    return NULL;
	}
	default:
	    bu_log("scad: parse error near line %d: unexpected token\n", cur(P)->line);
	    P->error = 1;
	    /* skip to next semicolon to attempt recovery */
	    while (peek(P) != T_SEMI && peek(P) != T_EOF) advance(P);
	    accept(P, T_SEMI);
	    return NULL;
    }
}


/* ------------------------------------------------------------------ */
/* public entry + AST teardown                                        */
/* ------------------------------------------------------------------ */

static void expr_free(struct scad_expr *e);

static void
assigns_free(struct scad_assign *a)
{
    while (a) {
	struct scad_assign *nx = a->next;
	if (a->name) bu_free(a->name, "scad assign name");
	expr_free(a->value);
	bu_free(a, "scad assign");
	a = nx;
    }
}

static void
expr_free(struct scad_expr *e)
{
    size_t i;
    if (!e) return;
    if (e->str) bu_free(e->str, "scad expr str");
    if (e->name) bu_free(e->name, "scad expr name");
    expr_free(e->a);
    expr_free(e->b);
    expr_free(e->c);
    for (i = 0; i < e->nitems; i++) expr_free(e->items[i]);
    if (e->items) bu_free(e->items, "scad expr items");
    for (i = 0; i < e->nargs; i++) {
	if (e->args[i].name) bu_free(e->args[i].name, "scad arg name");
	expr_free(e->args[i].value);
    }
    if (e->args) bu_free(e->args, "scad expr args");
    assigns_free(e->assigns);
    bu_free(e, "scad expr");
}

static void
params_free(struct scad_param *p)
{
    while (p) {
	struct scad_param *nx = p->next;
	if (p->name) bu_free(p->name, "scad param name");
	expr_free(p->def);
	bu_free(p, "scad param");
	p = nx;
    }
}

static void
stmt_free(struct scad_stmt *s)
{
    size_t i;
    if (!s) return;
    if (s->name) bu_free(s->name, "scad stmt name");
    expr_free(s->expr);
    for (i = 0; i < s->nargs; i++) {
	if (s->args[i].name) bu_free(s->args[i].name, "scad arg name");
	expr_free(s->args[i].value);
    }
    if (s->args) bu_free(s->args, "scad stmt args");
    params_free(s->params);
    assigns_free(s->assigns);
    for (i = 0; i < s->nbody; i++) stmt_free(s->body[i]);
    if (s->body) bu_free(s->body, "scad stmt body");
    for (i = 0; i < s->nelse; i++) stmt_free(s->elsebody[i]);
    if (s->elsebody) bu_free(s->elsebody, "scad stmt else");
    bu_free(s, "scad stmt");
}

void
scad_stmts_free(struct scad_stmt **stmts, size_t n)
{
    size_t i;
    if (!stmts) return;
    for (i = 0; i < n; i++) stmt_free(stmts[i]);
    bu_free(stmts, "scad stmts");
}


struct scad_stmt **
scad_parse(struct scad_state *st, const char *text, const char *fname, size_t *nout)
{
    struct tok **toks;
    size_t ntok = 0;
    int lex_err = 0;
    struct parser P;
    struct ptrvec prog;
    size_t i;

    *nout = 0;
    toks = lex_all(st, text, &ntok, &lex_err);

    memset(&P, 0, sizeof(P));
    P.st = st;
    P.t = toks;
    P.n = ntok;
    P.fname = fname;
    P.error = lex_err;

    memset(&prog, 0, sizeof(prog));
    while (peek(&P) != T_EOF && !P.error) {
	struct scad_stmt *s = parse_statement(&P);
	if (s) pv_push(&prog, s);
    }

    /* free token storage */
    for (i = 0; i < ntok; i++) {
	if (toks[i]->str) bu_free(toks[i]->str, "scad tok str");
	bu_free(toks[i], "scad tok");
    }
    bu_free(toks, "scad toks");

    if (P.error) {
	scad_stmts_free((struct scad_stmt **)prog.v, prog.n);
	return NULL;
    }

    *nout = prog.n;
    return (struct scad_stmt **)prog.v;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
