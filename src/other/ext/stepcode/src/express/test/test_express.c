#include <assert.h>

#include "express/express.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"

/* non-core */
#include "express/resolve.h"
#include "express/schema.h"

#include "../token_type.h"
#include "../parse_data.h"
#include "expscan.h"

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */

Error ERROR_circular_reference;
Error ERROR_undefined_schema;

int yylineno;
Express yyexpresult;
Linked_List PARSEnew_schemas;
int print_objects_while_running;
YYSTYPE yylval;
int yyerrstatus;

/*
 * mock functions
 */
typedef void * (*malloc_func_t) (size_t);
typedef void   (*free_func_t)   (void *);

DEFINE_FFF_GLOBALS

FAKE_VOID_FUNC0(RESOLVEinitialize)
FAKE_VOID_FUNC0(SYMBOLinitialize)
FAKE_VOID_FUNC0(SCOPEinitialize)
FAKE_VOID_FUNC0(TYPEinitialize)
FAKE_VOID_FUNC0(VARinitialize)
FAKE_VOID_FUNC0(ENTITYinitialize)
FAKE_VOID_FUNC0(SCHEMAinitialize)
FAKE_VOID_FUNC0(CASE_ITinitialize)
FAKE_VOID_FUNC0(EXPinitialize)
FAKE_VOID_FUNC0(SCANinitialize)
FAKE_VOID_FUNC0(EXPKWinitialize)
FAKE_VOID_FUNC0(RESOLVEcleanup)
FAKE_VOID_FUNC0(TYPEcleanup)
FAKE_VOID_FUNC0(EXPcleanup)
FAKE_VOID_FUNC0(SCANcleanup)
FAKE_VOID_FUNC0(parserInitState)
FAKE_VOID_FUNC(SCAN_lex_init, char *, FILE *)
FAKE_VOID_FUNC(ParseTrace, FILE *, char *)
FAKE_VOID_FUNC(Parse, void *, int, YYSTYPE, parse_data_t)
FAKE_VOID_FUNC(perplexFree, perplex_t)
FAKE_VOID_FUNC(ParseFree, void *, free_func_t)
FAKE_VOID_FUNC(SCHEMAdefine_use, Schema, Rename *)
FAKE_VOID_FUNC(SCHEMAdefine_reference, Schema, Rename *)
FAKE_VOID_FUNC(SCOPEresolve_subsupers, Scope)
FAKE_VOID_FUNC(SCOPEresolve_types, Scope)
FAKE_VOID_FUNC(SCOPEresolve_expressions_statements, Scope)

FAKE_VALUE_FUNC(void *, ParseAlloc, malloc_func_t)
FAKE_VALUE_FUNC(char *, SCANstrdup, const char *)
FAKE_VALUE_FUNC(perplex_t, perplexFileScanner, FILE *)
FAKE_VALUE_FUNC(int, yylex, perplex_t)

void setup() {
    RESET_FAKE(RESOLVEinitialize);
    RESET_FAKE(SYMBOLinitialize);
    RESET_FAKE(SCOPEinitialize);
    RESET_FAKE(TYPEinitialize);
    RESET_FAKE(VARinitialize);
    RESET_FAKE(ENTITYinitialize);
    RESET_FAKE(SCHEMAinitialize);
    RESET_FAKE(CASE_ITinitialize);
    RESET_FAKE(EXPinitialize);
    RESET_FAKE(SCANinitialize);
    RESET_FAKE(EXPKWinitialize);
    RESET_FAKE(RESOLVEcleanup);
    RESET_FAKE(TYPEcleanup);
    RESET_FAKE(EXPcleanup);
    RESET_FAKE(SCANcleanup);
    RESET_FAKE(parserInitState);
    RESET_FAKE(SCAN_lex_init);
    RESET_FAKE(ParseTrace);
    RESET_FAKE(Parse);
    RESET_FAKE(perplexFree);
    RESET_FAKE(ParseFree);
    RESET_FAKE(SCHEMAdefine_use);
    RESET_FAKE(SCHEMAdefine_reference);
    RESET_FAKE(SCOPEresolve_subsupers);
    RESET_FAKE(SCOPEresolve_types);
    RESET_FAKE(SCOPEresolve_expressions_statements);
    RESET_FAKE(ParseAlloc);
    RESET_FAKE(SCANstrdup);
    RESET_FAKE(perplexFileScanner);
    RESET_FAKE(yylex);
}

int test_express_rename_resolve() {
    Schema cur_schema, ref_schema;
    Rename *use_ref;
    Entity ent;
    Symbol *cur_schema_id, *ent_id, *ent_ref, *ref_schema_id;

    cur_schema_id = SYMBOLcreate("cur_schema", 1, "cur.exp");
    ent_id = SYMBOLcreate("line", 1, "cur.exp");
    ent_ref = SYMBOLcreate("line", 1, "ref.exp");
    ref_schema_id = SYMBOLcreate("ref_schema", 1, "ref.exp");
    
    cur_schema = SCHEMAcreate();
    cur_schema->symbol = *cur_schema_id;
    cur_schema->u.schema->uselist = LISTcreate();
    
    ref_schema = SCHEMAcreate();
    ref_schema->symbol = *ref_schema_id;
    
    ent = ENTITYcreate(ent_id);
    DICTdefine(ref_schema->symbol_table, ent_id->name, ent, ent_id, OBJ_ENTITY);
    
    /* TODO: create RENcreate(...), refactor SCHEMAadd_use() */
    use_ref = REN_new();
    use_ref->schema_sym = ref_schema_id;
    use_ref->old = ent_ref;
    use_ref->nnew = ent_ref;
    use_ref->rename_type = use;
    LISTadd_last(cur_schema->u.schema->uselist, use_ref);
    use_ref->schema = ref_schema;
    
    RENAMEresolve(use_ref, cur_schema);
    
    assert(use_ref->type == OBJ_ENTITY);
    return 0;
}

struct test_def tests[] = {
    {"express_rename_resolve", test_express_rename_resolve},
    {NULL}
};
