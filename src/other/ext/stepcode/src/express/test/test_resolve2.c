
#include <assert.h>

#include "express/resolve.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"

/* non-core */
#include "express/type.h"

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */

char * EXPRESSprogram_name;
int EXPRESSpass;
int yylineno;
int print_objects_while_running;

/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VOID_FUNC(ENTITYresolve_supertypes, Entity)
FAKE_VOID_FUNC(ENTITYresolve_subtypes, Schema)
FAKE_VOID_FUNC(TYPE_resolve, Type *)
FAKE_VOID_FUNC(TYPEresolve_expressions, Type, Scope)
FAKE_VOID_FUNC(EXP_resolve, Expression, Scope, Type)
FAKE_VOID_FUNC(STMTlist_resolve, Linked_List, Scope)
FAKE_VOID_FUNC(ENTITYresolve_expressions, Entity)

FAKE_VALUE_FUNC(int, WHEREresolve, Linked_List, Scope, int)
FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)

void setup() {
    RESET_FAKE(ENTITYresolve_supertypes);
    RESET_FAKE(ENTITYresolve_subtypes);
    RESET_FAKE(TYPE_resolve);
    RESET_FAKE(TYPEresolve_expressions);
    RESET_FAKE(EXP_resolve);
    RESET_FAKE(STMTlist_resolve);
    RESET_FAKE(ENTITYresolve_expressions);
    RESET_FAKE(WHEREresolve);
    RESET_FAKE(EXPRESS_fail);
}

int test_scope_resolve_expr_stmt() {
    Schema scope;
    Type sel, ent_base;
    Entity ent;
    Symbol *ent_id, *sel_id;
    
    scope = SCHEMAcreate();
    ent_id = SYMBOLcreate("ent", 1, "test_4");
    sel_id = SYMBOLcreate("sel_typ", 1, "test_4");
    
    ent_base = TYPEcreate_name(ent_id);
    ent_base->superscope = scope;
    ent = ENTITYcreate(ent_id);
    ent->superscope = scope;
    sel = TYPEcreate(select_);
    sel->symbol = *sel_id;
    sel->u.type->body->list = LISTcreate();
    sel->superscope = scope;
    LISTadd_last(sel->u.type->body->list, ent_base);
    
    DICTdefine(scope->symbol_table, ent_id->name, ent, ent_id, OBJ_ENTITY);
    DICTdefine(scope->symbol_table, sel_id->name, sel, sel_id, OBJ_TYPE);

    SCOPEresolve_expressions_statements(scope);
    
    assert(ENTITYresolve_expressions_fake.call_count == 1);
    assert(TYPEresolve_expressions_fake.call_count == 1);
    
    return 0;
}

int test_scope_resolve_subsupers() {
    Schema scope;
    Type sel, ent_base;
    Entity ent;
    Symbol *ent_id, *sel_id;
    
    scope = SCHEMAcreate();
    ent_id = SYMBOLcreate("ent", 1, "test_4");
    sel_id = SYMBOLcreate("sel_typ", 1, "test_4");
    
    ent_base = TYPEcreate_name(ent_id);
    ent_base->superscope = scope;
    ent = ENTITYcreate(ent_id);
    ent->superscope = scope;
    sel = TYPEcreate(select_);
    sel->symbol = *sel_id;
    sel->u.type->body->list = LISTcreate();
    sel->superscope = scope;
    LISTadd_last(sel->u.type->body->list, ent_base);
    
    DICTdefine(scope->symbol_table, ent_id->name, ent, ent_id, OBJ_ENTITY);
    DICTdefine(scope->symbol_table, sel_id->name, sel, sel_id, OBJ_TYPE);

    SCOPEresolve_subsupers(scope);
    
    assert(TYPE_resolve_fake.call_count == 1);
    assert(ENTITYresolve_supertypes_fake.call_count == 1);
    assert(ENTITYresolve_subtypes_fake.call_count == 1);
    
    return 0;
}


struct test_def tests[] = {
    {"scope_resolve_expr_stmt", test_scope_resolve_expr_stmt},
    {"scope_resolve_subsupers", test_scope_resolve_subsupers},
    {NULL}
};
