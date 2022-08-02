

/**
** \file express.c
** Express package manager.
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: express.c,v $
 * Revision 1.20  1997/01/21 19:53:20  dar
 * made C++ compatible
 *
 * Revision 1.18  1995/06/08 22:59:59  clark
 * bug fixes
 *
 * Revision 1.17  1995/05/17  14:25:56  libes
 * Fixed bug in EXPRESS_PATH handling.  Improved debugging diagnostics.
 *
 * Revision 1.16  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.15  1995/03/09  18:42:21  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.14  1995/02/09  18:00:49  clark
 * update version string
 *
 * Revision 1.13  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.12  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.11  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.9  1993/02/22  21:45:06  libes
 * fix pass print bug
 *
 * Revision 1.8  1993/02/16  03:19:56  libes
 * added unwriteable error
 *
 * Revision 1.7  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.6  1992/09/16  18:19:14  libes
 * fixed bug in EXPRESS_PATH searching
 *
 * Revision 1.5  1992/08/27  23:38:35  libes
 * removed redundant call to initialize much of EXPRESS
 * rewrote schema-rename-connect to use fifo instead of dictionary
 * fixed smashing of file names by new schema files
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>

#include "sc_memmgr.h"
#include "express/memory.h"
#include "express/basic.h"
#include "express/express.h"
#include "express/resolve.h"
#include "express/factory.h"
#include "stack.h"
#include "express/scope.h"
#include "token_type.h"
#include "expparse.h"
#include "expscan.h"
#include "parse_data.h"
#include "express/lexact.h"

void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void *));
void Parse(void *parser, int tokenID, YYSTYPE data, parse_data_t parseData);
void ParseTrace(FILE *TraceFILE, char *zTracePrompt);

Linked_List EXPRESS_path;
int EXPRESSpass;

void (*EXPRESSinit_args)(int, char **) = NULL;
void (*EXPRESSinit_parse)(void) = NULL;
int (*EXPRESSfail)(Express) = NULL;
int (*EXPRESSsucceed)(Express) = NULL;
void (*EXPRESSbackend)(Express) = NULL;
char *EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];  /* initialized elsewhere */
int (*EXPRESSgetopt)(int, char *) = NULL;
bool    EXPRESSignore_duplicate_schemas      = false;

Function funcdef(char *name, int pcount, Type ret_typ);
void procdef(char *name, int pcount);
void BUILTINSinitialize();
Dictionary EXPRESSbuiltins; /* procedures/functions */


struct Scope_ *FUNC_NVL;
struct Scope_ *FUNC_USEDIN;
extern Express yyexpresult;


extern Linked_List PARSEnew_schemas;
void SCOPEinitialize(void);

static Express PARSERrun(char *, FILE *);

/** name specified on command line */
char *input_filename = 0;

int EXPRESS_fail(Express model)
{
    ERRORflush_messages();

    if(EXPRESSfail) {
        return((*EXPRESSfail)(model));
    }

    fprintf(stderr, "Errors in input\n");
    return 1;
}

int EXPRESS_succeed(Express model)
{
    if(EXPRESSsucceed) {
        return((*EXPRESSsucceed)(model));
    }

    fprintf(stderr, "No errors in input\n");
    return 0;
}

Express EXPRESScreate()
{
    Express model = SCOPEcreate(OBJ_EXPRESS);
    model->u.express = (struct Express_ *)sc_calloc(1, sizeof(struct Express_));
    return model;
}

void EXPRESSdestroy(Express model)
{
    if(model->u.express->basename) {
        sc_free(model->u.express->basename);
    }
    if(model->u.express->filename) {
        sc_free(model->u.express->filename);
    }
    sc_free(model->u.express);
    SCOPEdestroy(model);
}

#define MAX_SCHEMA_FILENAME_SIZE    256
typedef struct Dir {
    char full[MAX_SCHEMA_FILENAME_SIZE];
    char *leaf;
} Dir;

static void EXPRESS_PATHinit()
{
    char *p;
    Dir *dir;

    EXPRESS_path = LISTcreate();
    p = getenv("EXPRESS_PATH");
    if(!p) {
        /* if no EXPRESS_PATH, search current directory anyway */
        dir = (Dir *)sc_malloc(sizeof(Dir));
        dir->leaf = dir->full;
        LISTadd_last(EXPRESS_path, dir);
    } else {
        int done = 0;
        while(!done) {
            char *start;    /* start of current dir */
            int length; /* length of dir */
            char *slash;    /* last slash in dir */
            char save;  /* place to character from where we */
            /* temporarily null terminate */

            /* get next directory */
            while(isspace(*p)) {
                p++;
            }
            if(*p == '\0') {
                break;
            }
            start = p;

            /* find the end of the directory */
            while(*p != '\0' && !isspace(*p)) {
                p++;
            }
            save = *p;
            if(*p == 0) {
                done = 1;
            } else {
                *p = '\0';
            }
            p++;    /* leave p after terminating null */

            dir = (Dir *)sc_malloc(sizeof(Dir));

            /* if it's just ".", make it as if it was */
            /* just "" to make error messages cleaner */
            if(!strcmp(".", start)) {
                dir->leaf = dir->full;
                LISTadd_last(EXPRESS_path, dir);
                *(p - 1) = save;   /* put char back where */
                /* temp null was */
                continue;
            }

            length = (p - 1) - start;

            /* if slash present at end, don't add another */
            slash = strrchr(start, '/');
            if(slash && (slash[1] == '\0')) {
                strncpy(dir->full, start, MAX_SCHEMA_FILENAME_SIZE-1);
                dir->leaf = dir->full + length;
            } else {
                snprintf(dir->full, MAX_SCHEMA_FILENAME_SIZE-1, "%s/", start);
                dir->leaf = dir->full + length + 1;
            }
            LISTadd_last(EXPRESS_path, dir);

            *(p - 1) = save;   /* put char back where temp null was */
        }
    }
}

static void EXPRESS_PATHfree(void)
{
    LISTdo(EXPRESS_path, dir, Dir *)
    sc_free(dir);
    LISTod
    LISTfree(EXPRESS_path);
}

/** inform object system about bit representation for handling pass diagnostics */
void PASSinitialize()
{
}

/** Initialize the Express package. */
void EXPRESSinitialize(void)
{
    MEMORYinitialize();
    ERRORinitialize();

    HASHinitialize();   /* comes first - used by just about everything else! */
    DICTinitialize();
    LISTinitialize();   /* ditto */
    STACKinitialize();
    PASSinitialize();

    RESOLVEinitialize();

    SYMBOLinitialize();

    SCOPEinitialize();
    FACTORYinitialize();
    TYPEinitialize();   /* cannot come before SCOPEinitialize */
    VARinitialize();

    ALGinitialize();
    ENTITYinitialize();
    SCHEMAinitialize();

    CASE_ITinitialize();
    EXPinitialize();
    /*    LOOP_CTLinitialize();*/
    STMTinitialize();

    SCANinitialize();

    BUILTINSinitialize();

    EXPRESS_PATHinit(); /* note, must follow defn of errors it needs! */
}

/** Clean up the EXPRESS package. */
void EXPRESScleanup(void)
{
    EXPRESS_PATHfree();

    DICTcleanup();
    ERRORcleanup();
    RESOLVEcleanup();
    TYPEcleanup();
    EXPcleanup();
    SCANcleanup();
    LISTcleanup();
}

/**
** \param file  - Express source file to parse
** \return resulting Working Form model
** Parse an Express source file into the Working Form.
*/
void EXPRESSparse(Express model, FILE *fp, char *filename)
{
    yyexpresult = model;

    if(!fp) {
        fp = fopen(filename, "r");
    }
    if(!fp) {
        /* go down path looking for file */
        LISTdo(EXPRESS_path, dir, Dir *)
        sprintf(dir->leaf, "%s", filename);
        if(0 != (fp = fopen(dir->full, "r"))) {
            filename = dir->full;
            break;
        }
        LISTod
    }

    if(!fp) {
        ERRORreport(FILE_UNREADABLE, filename, strerror(errno));
        return;
    }

    if(filename) {
        char *dot   = strrchr(filename, '.');
        char *slash = strrchr(filename, '/');

        /* get beginning of basename */
        char *start = slash ? (slash + 1) : filename;

        int length = strlen(start);

        /* drop .exp suffix if present */
        if(dot && !strcmp(dot, ".exp")) {
            length -= 4;
        }

        model->u.express->basename = (char *)sc_malloc(length + 1);
        memcpy(model->u.express->basename, filename, length);
        model->u.express->basename[length] = '\0';

        /* get new copy of filename to avoid being smashed */
        /* by subsequent lookups on EXPRESS_path */
        model->u.express->filename = SCANstrdup(filename);
        filename = model->u.express->filename;
    }

    PARSEnew_schemas = LISTcreate();
    PARSERrun(filename, model->u.express->file = fp);
}

/* TODO LEMON ought to put this in expparse.h */
void parserInitState();

/** start parsing a new schema file */
static Express PARSERrun(char *filename, FILE *fp)
{
    extern void SCAN_lex_init(char *, FILE *);
    extern YYSTYPE yylval;
    extern int yyerrstatus;
    int tokenID;
    parse_data_t parseData;

    void *parser = ParseAlloc(malloc);
    perplex_t scanner = perplexFileScanner(fp);
    parseData.scanner = scanner;

    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "parse (pass %d)\n", EXPRESSpass);
    }

    if(print_objects_while_running & OBJ_SCHEMA_BITS) {
        fprintf(stderr, "parse: %s (schema file)\n", filename);
    }

    SCAN_lex_init(filename, fp);
    parserInitState();

    yyerrstatus = 0;
    /* NOTE uncomment the next line to enable parser tracing */
    /* ParseTrace( stderr, "- expparse - " ); */
    while((tokenID = yylex(scanner)) > 0) {
        Parse(parser, tokenID, yylval, parseData);
    }
    Parse(parser, 0, yylval, parseData);

    /* want 0 on success, 1 on invalid input, 2 on memory exhaustion */
    if(yyerrstatus != 0) {
        fprintf(stderr, ">> Bailing! (yyerrstatus = %d)\n", yyerrstatus);
        ERRORreport(BAIL_OUT);
        /* free model and model->u.express */
        return 0;
    }
    EXPRESSpass = 1;

    perplexFree(scanner);
    ParseFree(parser, free);

    return yyexpresult;
}

/**
 * find the final object to which a rename points
 * i.e., follow chain of USEs or REFs
 * sets DICT_type
 *
 * Sept 2013 - remove unused param enum rename_type type (TODO should this be used)?
 */
static void *SCOPEfind_for_rename(Scope schema, char *name)
{
    void *result;
    Rename *rename;

    /* object can only appear in top level symbol table */
    /* OR in another rename clause */

    result = DICTlookup(schema->symbol_table, name);
    if(result) {
        return result;
    }

    /* Occurs in a fully USE'd schema? */
    LISTdo(schema->u.schema->use_schemas, use_schema, Schema) {
        /* follow chain'd USEs */
        result = SCOPEfind_for_rename(use_schema, name);
        if(result) {
            return(result);
        }
    }
    LISTod;

    /* Occurs in a partially USE'd schema? */
    rename = (Rename *)DICTlookup(schema->u.schema->usedict, name);
    if(rename) {
        RENAMEresolve(rename, schema);
        DICT_type = rename->type;
        return(rename->object);
    }

    LISTdo(schema->u.schema->uselist, r, Rename *)
    if(!strcmp((r->nnew ? r->nnew : r->old)->name, name)) {
        RENAMEresolve(r, schema);
        DICT_type = r->type;
        return(r->object);
    }
    LISTod;

    /* we are searching for an object to be interfaced from this schema. */
    /* therefore, we *never* want to look at things which are REFERENCE'd */
    /* *into* the current schema.  -snc */

    return 0;
}

void RENAMEresolve(Rename *r, Schema s)
{
    void *remote;

    /*   if (is_resolved_rename_raw(r->old)) return;*/
    if(r->object) {
        return;
    }

    if(is_resolve_failed_raw(r->old)) {
        return;
    }

    if(is_resolve_in_progress_raw(r->old)) {
        ERRORreport_with_symbol(CIRCULAR_REFERENCE,
                                r->old, r->old->name);
        resolve_failed_raw(r->old);
        return;
    }
    resolve_in_progress_raw(r->old);

    remote = SCOPEfind_for_rename(r->schema, r->old->name);
    if(remote == 0) {
        ERRORreport_with_symbol(REF_NONEXISTENT, r->old,
                                r->old->name, r->schema->symbol.name);
        resolve_failed_raw(r->old);
    } else {
        r->object = remote;
        r->type = DICT_type;
        switch(r->rename_type) {
            case use:
                SCHEMAdefine_use(s, r);
                break;
            case ref:
                SCHEMAdefine_reference(s, r);
                break;
        }
        /*  resolve_rename_raw(r->old);*/
    }
    resolve_not_in_progress_raw(r->old);
}

#ifdef using_enum_items_is_a_pain
static void RENAMEresolve_enum(Type t, Schema s)
{
    DictionaryEntry de;
    Expression      x;

    DICTdo_type_init(t->symbol_table, &de, OBJ_EXPRESSION);
    while(0 != (x = (Expression)DICTdo(&de))) {
        /*      SCHEMAadd_use(s, v*/
        /*          raw(x->symbol.name);*/
    }
}
#endif

Schema EXPRESSfind_schema(Dictionary modeldict, char *name)
{
    Schema s;
    FILE *fp;
    char *src, *dest;
    char lower[MAX_SCHEMA_FILENAME_SIZE];   /* avoid lowerizing original */

    if(print_objects_while_running & OBJ_SCHEMA_BITS) {
        fprintf(stderr, "pass %d: %s (schema reference)\n",
                EXPRESSpass, name);
    }

    s = (Schema)DICTlookup(modeldict, name);
    if(s) {
        return s;
    }

    dest = lower;
    for(src = name; *src; src++) {
        *dest++ = tolower(*src);
    }
    *dest = '\0';

    /* go down path looking for file */
    LISTdo(EXPRESS_path, dir, Dir *)
    sprintf(dir->leaf, "%s.exp", lower);
    if(print_objects_while_running & OBJ_SCHEMA_BITS) {
        fprintf(stderr, "pass %d: %s (schema file?)\n",
                EXPRESSpass, dir->full);
    }
    fp = fopen(dir->full, "r");
    if(fp) {
        Express express;

        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "pass %d: %s (schema file found)\n",
                    EXPRESSpass, dir->full);
        }

        express = PARSERrun(SCANstrdup(dir->full), fp);
        if(express) {
            s = (Schema)DICTlookup(modeldict, name);
        }
        if(s) {
            return s;
        }
        ERRORreport(SCHEMA_NOT_IN_OWN_SCHEMA_FILE,
                    name, dir->full);
        return 0;
    } else {
        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "pass %d: %s (schema file not found), errno = %d\n", EXPRESSpass, dir->full, errno);
        }
    }
    LISTod
    return 0;
}


/**
 * make the initial connections from one schema to another
 * dictated by USE/REF clauses that use dictionaries, i.e.,
 * because of partial schema references
 * \sa connect_schema_lists()
 */
static void connect_lists(Dictionary modeldict, Schema schema, Linked_List list)
{
    Rename *r;

    /* translate symbols to schemas */
    LISTdo_links(list, l)
    r = (Rename *)l->data;
    r->schema = EXPRESSfind_schema(modeldict, r->schema_sym->name);
    if(!r->schema) {
        ERRORreport_with_symbol(UNDEFINED_SCHEMA,
                                r->schema_sym,
                                r->schema_sym->name);
        resolve_failed_raw(r->old);
        resolve_failed(schema);
    }
    LISTod
}

/**
 * same as `connect_lists` except for full schemas
 * \sa connect_lists()
 */
static void connect_schema_lists(Dictionary modeldict, Schema schema, Linked_List schema_list)
{
    Symbol *sym;
    Schema ref_schema;

    /* translate symbols to schemas */
    LISTdo_links(schema_list, list)
    sym = (Symbol *)list->data;
    ref_schema = EXPRESSfind_schema(modeldict, sym->name);
    if(!ref_schema) {
        ERRORreport_with_symbol(UNDEFINED_SCHEMA, sym, sym->name);
        resolve_failed(schema);
        list->data = NULL;
    } else {
        list->data = ref_schema;
    }
    LISTod
}

/**
** \param model   - Working Form model to resolve
** Perform symbol resolution on a loosely-coupled WF.
*/
void EXPRESSresolve(Express model)
{
    /* resolve multiple schemas.  Schemas will be resolved here or when */
    /* they are first encountered by a use/reference clause, whichever */
    /* comes first - DEL */

    Schema schema;
    DictionaryEntry de;

    jmp_buf env;
    if(setjmp(env)) {
        return;
    }
    ERRORsafe(env);

    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving schema references\n", EXPRESSpass);
    }

    /* connect the real schemas to all the rename clauses */

    /* we may be adding new schemas (to dictionary) as we traverse it, */
    /* so to avoid confusing dictionary, use a list as a fifo.  I.e., */
    /* add news schemas to end, drop old ones off the front as we */
    /* process them. */

    LISTdo(PARSEnew_schemas, print_schema, Schema) {
        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "pass %d: %s (schema)\n",
                    EXPRESSpass, print_schema->symbol.name);
        }

        if(print_schema->u.schema->uselist)
            connect_lists(model->symbol_table,
                          print_schema, print_schema->u.schema->uselist);
        if(print_schema->u.schema->reflist)
            connect_lists(model->symbol_table,
                          print_schema, print_schema->u.schema->reflist);

        connect_schema_lists(model->symbol_table,
                             print_schema, print_schema->u.schema->use_schemas);
        connect_schema_lists(model->symbol_table,
                             print_schema, print_schema->u.schema->ref_schemas);
    }
    LISTod;

    LISTfree(PARSEnew_schemas);
    PARSEnew_schemas = 0;   /* just in case */

    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving objects references to other schemas\n", EXPRESSpass);
    }

    /* connect the object in each rename clause to the real object */
    DICTdo_type_init(model->symbol_table, &de, OBJ_SCHEMA);
    while(0 != (schema = (Schema)DICTdo(&de))) {
        if(is_not_resolvable(schema)) {
            continue;
        }

        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "pass %d: %s (schema)\n",
                    EXPRESSpass, schema->symbol.name);
        }

        /* do USE's first because they take precedence */
        if(schema->u.schema->uselist) {
            LISTdo(schema->u.schema->uselist, r, Rename *)
            RENAMEresolve(r, schema);
            LISTod;
            LISTfree(schema->u.schema->uselist);
            schema->u.schema->uselist = 0;
        }
        if(schema->u.schema->reflist) {
            LISTdo(schema->u.schema->reflist, r, Rename *)
            RENAMEresolve(r, schema);
            LISTod;
            LISTfree(schema->u.schema->reflist);
            schema->u.schema->reflist = 0;
        }
    }

    /* resolve sub- and supertype references.  also resolve all */
    /* defined types */
    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving sub and supertypes\n", EXPRESSpass);
    }

    DICTdo_type_init(model->symbol_table, &de, OBJ_SCHEMA);
    while(0 != (schema = (Schema)DICTdo(&de))) {
        if(is_not_resolvable(schema)) {
            continue;
        }
        SCOPEresolve_subsupers(schema);
    }
    if(ERRORoccurred) {
        ERRORunsafe();
    }

    /* resolve types */
    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving types\n", EXPRESSpass);
    }

    SCOPEresolve_types(model);
    if(ERRORoccurred) {
        ERRORunsafe();
    }

#ifdef using_enum_items_is_a_pain
    /* resolve USE'd enumerations */
    /* doesn't really deserve its own pass, but needs to come after */
    /* type resolution */
    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving implied USE's\n", EXPRESSpass);
    }

    DICTdo_type_init(model->symbol_table, &de, OBJ_SCHEMA);
    while(0 != (schema = (Schema)DICTdo(&de))) {
        if(is_not_resolvable(schema)) {
            continue;
        }

        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "pass %d: %s (schema)\n",
                    EXPRESSpass, schema->symbol.name);
        }

        if(schema->u.schema->usedict) {
            DICTdo_init(schema->u.schema->usedict, &fg)
            while(0 != (r = (Rename)DICTdo(&fg))) {
                if((r->type = OBJ_TYPE) && ((Type)r->object)->body &&
                        TYPEis_enumeration((Type)r->object)) {
                    RENAMEresolve_enum((Type)r->object, schema);
                }
            }
        }
    }
    if(ERRORoccurred) {
        ERRORunsafe();
    }
#endif

    EXPRESSpass++;
    if(print_objects_while_running & OBJ_PASS_BITS) {
        fprintf(stderr, "pass %d: resolving expressions and statements\n", EXPRESSpass);
    }

    SCOPEresolve_expressions_statements(model);
    if(ERRORoccurred) {
        ERRORunsafe();
    }

    /* mark everything resolved if possible */
    DICTdo_init(model->symbol_table, &de);
    while(0 != (schema = (Schema)DICTdo(&de))) {
        if(is_resolvable(schema)) {
            resolved_all(schema);
        }
    }
}

Function funcdef(char *name, int pcount, Type ret_typ)
{
    Function f = ALGcreate(OBJ_FUNCTION);
    f->symbol.name = name;
    f->u.func->pcount = pcount;
    f->u.func->return_type = ret_typ;
    f->u.func->builtin = true;
    resolved_all(f);
    DICTdefine(EXPRESSbuiltins, name, f, 0, OBJ_FUNCTION);
    return f;
}

void procdef(char *name, int pcount)
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    p->symbol.name = name;
    p->u.proc->pcount = pcount;
    p->u.proc->builtin = true;
    resolved_all(p);
    DICTdefine(EXPRESSbuiltins, name, p, 0, OBJ_PROCEDURE);
}

void BUILTINSinitialize()
{
    EXPRESSbuiltins = DICTcreate(35);
    procdef("INSERT", 3);
    procdef("REMOVE", 2);

    funcdef("ABS",    1, Type_Number);
    funcdef("ACOS",   1, Type_Real);
    funcdef("ASIN",   1, Type_Real);
    funcdef("ATAN",   2, Type_Real);
    funcdef("BLENGTH", 1, Type_Integer);
    funcdef("COS",    1, Type_Real);
    funcdef("EXISTS", 1, Type_Boolean);
    funcdef("EXP",    1, Type_Real);
    funcdef("FORMAT", 2, Type_String);
    funcdef("HIBOUND", 1, Type_Integer);
    funcdef("HIINDEX", 1, Type_Integer);
    funcdef("LENGTH", 1, Type_Integer);
    funcdef("LOBOUND", 1, Type_Integer);
    funcdef("LOG",    1, Type_Real);
    funcdef("LOG10",  1, Type_Real);
    funcdef("LOG2",   1, Type_Real);
    funcdef("LOINDEX", 1, Type_Integer);
    funcdef("ODD",    1, Type_Logical);
    funcdef("ROLESOF", 1, Type_Set_Of_String);
    funcdef("SIN",    1, Type_Real);
    funcdef("SIZEOF", 1, Type_Integer);
    funcdef("SQRT",   1, Type_Real);
    funcdef("TAN",    1, Type_Real);
    funcdef("TYPEOF", 1, Type_Set_Of_String);
    funcdef("VALUE",   1, Type_Number);
    funcdef("VALUE_IN",    2, Type_Logical);
    funcdef("VALUE_UNIQUE", 1, Type_Logical);

    FUNC_NVL = funcdef("NVL",    2, Type_Generic);
    FUNC_USEDIN = funcdef("USEDIN",  2, Type_Bag_Of_Generic);
}
