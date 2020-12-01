#include <assert.h>

#include "express/resolve.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"

/* non-core */
#include "express/symbol.h"
#include "express/schema.h"
#include "express/expr.h"
#include "express/type.h"

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */

char * EXPRESSprogram_name;
int yylineno;
int __SCOPE_search_id;
int EXPRESSpass;
struct Scope_ * FUNC_NVL;
struct Scope_ * FUNC_USEDIN;

struct EXPop_entry EXPop_table[OP_LAST];

int tag_count;

/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VALUE_FUNC(void *, SCOPEfind, Scope, char *, int)
FAKE_VALUE_FUNC(Variable, VARfind, Scope, char *, int)
FAKE_VALUE_FUNC(char *, VARget_simple_name, Variable)
FAKE_VALUE_FUNC(struct Scope_ *, ENTITYfind_inherited_entity, struct Scope_ *, char *, int)
FAKE_VALUE_FUNC(Variable, ENTITYget_named_attribute, Entity, char *)
FAKE_VALUE_FUNC(Variable, ENTITYresolve_attr_ref, Entity, Symbol *, Symbol *)
FAKE_VALUE_FUNC(int, ENTITYdeclares_variable, Entity, Variable)
FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)

void setup() {
    RESOLVEinitialize();
    
    RESET_FAKE(SCOPEfind);
    RESET_FAKE(VARfind);
    RESET_FAKE(VARget_simple_name);
    RESET_FAKE(ENTITYfind_inherited_entity);
    RESET_FAKE(ENTITYresolve_attr_ref);
    RESET_FAKE(ENTITYget_named_attribute);
    RESET_FAKE(ENTITYdeclares_variable);
    RESET_FAKE(EXPRESS_fail);
}

void * SCOPEfind_handler(Scope scope, char * name, int type) {
    (void) type;
    return DICTlookup(scope->symbol_table, name);
}

int test_exp_resolve_bad_func_call() {
    Schema scope;
    Symbol *func_id;
    Expression func_call;
    
    scope = SCHEMAcreate();
    
    func_id = SYMBOLcreate("func1", 1, "test1");
    func_call = EXPcreate_from_symbol(Type_Funcall, func_id);
    
    SCOPEfind_fake.custom_fake = SCOPEfind_handler;    

    EXP_resolve(func_call, scope, Type_Dont_Care);
    
    assert(func_call->symbol.resolved != RESOLVED);
    
    return 0;
}

int test_exp_resolve_func_call() {
    Schema scope;
    Symbol *func_id;
    Expression func_call;
    Function func_def;
    
    scope = SCHEMAcreate();
    
    func_id = SYMBOLcreate("func1", 1, "test1");
    func_call = EXPcreate_from_symbol(Type_Funcall, func_id);
    
    func_def = TYPEcreate_nostab(func_id, scope, OBJ_FUNCTION);
    SCOPEfind_fake.custom_fake = SCOPEfind_handler;    
    
    EXP_resolve(func_call, scope, Type_Dont_Care);
    
    assert(func_call->symbol.resolved == RESOLVED);
    assert(func_call->u.funcall.function == func_def);
    
    return 0;
}

Variable VARfind_handler(Scope scope, char *name, int strict) {
    (void) strict;
    return DICTlookup(scope->symbol_table, name);
}

int test_exp_resolve_local_identifier() {
    Schema scope;
    Entity ent;
    Expression ent_attr, ent_attr_ref;
    Symbol *attr_id, *attr_ref, *ent_id;
    Variable v_attr;
    Type attr_typ;
    
    scope = SCHEMAcreate();
    
    ent_id = SYMBOLcreate("entity1", 1, "test_2");
    ent = ENTITYcreate(ent_id);        
    DICT_define(scope->symbol_table, ent_id->name, ent, ent_id, OBJ_ENTITY);

    attr_id = SYMBOLcreate("attr1", 1, "test_2");
    ent_attr = EXPcreate_from_symbol(Type_Attribute, attr_id);
    attr_typ = TYPEcreate(real_);
    v_attr = VARcreate(ent_attr, attr_typ);
    v_attr->flags.attribute = true;
    DICT_define(ent->symbol_table, attr_id->name, v_attr, attr_id, OBJ_VARIABLE);
    
    attr_ref = SYMBOLcreate("attr1", 1, "test_2");
    ent_attr_ref = EXPcreate_from_symbol(Type_Identifier, attr_ref);
    
    VARfind_fake.custom_fake = VARfind_handler;
    
    EXP_resolve(ent_attr_ref, ent, Type_Dont_Care);
    
    assert(ent_attr_ref->u.variable == v_attr);
    assert(ent_attr_ref->symbol.resolved == RESOLVED);
    
    return 0;
}

int test_entity_resolve_subtype_expr_entity() {
    Schema scope;
    Entity ent1, ent2;
    Expression subtype_exp;
    Symbol *ent1_id, *ent2_id, *ent2_ref;
    int chk;
    
    scope = SCHEMAcreate();
    ent1_id = SYMBOLcreate("ent1", 1, "test_3");
    ent2_id = SYMBOLcreate("ent2", 1, "test_3");
    ent2_ref = SYMBOLcreate("ent2", 1, "test_3");
    ent1 = ENTITYcreate(ent1_id);
    ent2 = ENTITYcreate(ent2_id);
    
    DICTdefine(scope->symbol_table, ent1_id->name, ent1, ent1_id, OBJ_ENTITY);
    DICTdefine(scope->symbol_table, ent2_id->name, ent2, ent2_id, OBJ_ENTITY);

    subtype_exp = EXPcreate_from_symbol(Type_Identifier, ent2_ref);
    ent1->superscope = scope;
    ent1->u.entity->subtypes = LISTcreate();
    ent1->u.entity->subtype_expression = subtype_exp;
    
    SCOPEfind_fake.custom_fake = SCOPEfind_handler;
    chk = ENTITYresolve_subtype_expression(subtype_exp, ent1, &ent1->u.entity->subtypes);
    
    assert(chk == RESOLVED);
    
    return 0;
}

int test_type_resolve_entity() {
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

    SCOPEfind_fake.custom_fake = SCOPEfind_handler;

    TYPE_resolve(&sel);
    
    assert(sel->symbol.resolved == RESOLVED);
    
    return 0;
}

int test_stmt_resolve_pcall_proc() {
    Schema scope;
    Function f;
    Procedure p;
    Statement s;
    Symbol *func_id, *proc_id, *proc_ref;
    
    scope = SCHEMAcreate();
    
    func_id = SYMBOLcreate("func1", 1, "test_5");
    proc_id = SYMBOLcreate("proc1", 1, "test_5");
    proc_ref = SYMBOLcreate("proc1", 1, "test_5");

    f = ALGcreate(OBJ_FUNCTION);
    DICTdefine(scope->symbol_table, func_id->name, f, func_id, OBJ_FUNCTION);
    
    p = ALGcreate(OBJ_PROCEDURE);
    DICTdefine(f->symbol_table, proc_id->name, p, proc_id, OBJ_PROCEDURE);
    
    s = PCALLcreate(NULL);
    s->symbol = *proc_ref;
    
    SCOPEfind_fake.custom_fake = SCOPEfind_handler;
    
    STMTresolve(s, f);
    
    assert(s->u.proc->procedure == p);
    
    return 0;
}

int test_scope_resolve_named_types() {
    Schema scope;
    Type sel, ent_base;
    Entity ent;
    Symbol *ent_id, *sel_id;
    
    scope = SCHEMAcreate();
    sel_id = SYMBOLcreate("sel_typ", 1, "test_4");
    ent_id = SYMBOLcreate("ent", 1, "test_4");
    
    ent_base = TYPEcreate(entity_);
    ent_base->symbol = *ent_id;
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

    SCOPEfind_fake.custom_fake = SCOPEfind_handler;
    
    SCOPEresolve_types(scope);
    
    assert(!(ent->symbol.resolved & RESOLVE_FAILED));
    assert(!(sel->symbol.resolved & RESOLVE_FAILED));
    assert(!(scope->symbol.resolved & RESOLVE_FAILED));
    
    return 0;
}

int test_entity_resolve_supertypes() {
    Schema scope;
    Entity ent1, ent2;
    Symbol *ent1_id, *ent2_id, *ent1_ref;
    
    scope = SCHEMAcreate();
    ent1_id = SYMBOLcreate("ent1", 1, "test_3");
    ent2_id = SYMBOLcreate("ent2", 1, "test_3");
    ent1_ref = SYMBOLcreate("ent1", 1, "test_3");
    ent1 = ENTITYcreate(ent1_id);
    ent2 = ENTITYcreate(ent2_id);
    ent1->superscope = scope;
    ent2->superscope = scope;
    
    DICTdefine(scope->symbol_table, ent1_id->name, ent1, ent1_id, OBJ_ENTITY);
    DICTdefine(scope->symbol_table, ent2_id->name, ent2, ent2_id, OBJ_ENTITY);
    
    ent2->u.entity->supertype_symbols = LISTcreate();
    LISTadd_last(ent2->u.entity->supertype_symbols, ent1_ref);
    
    SCOPEfind_fake.custom_fake = SCOPEfind_handler;
    
    ENTITYresolve_supertypes(ent2);

    assert(!(ent2->symbol.resolved & RESOLVE_FAILED));
    
    return 0;
}

struct test_def tests[] = {
    {"exp_resolve_bad_func_call", test_exp_resolve_bad_func_call},
    {"exp_resolve_func_call", test_exp_resolve_func_call},
    {"exp_resolve_local_identifier", test_exp_resolve_local_identifier},
    {"entity_resolve_subtype_expr_entity", test_entity_resolve_subtype_expr_entity},
    {"type_resolve_entity", test_type_resolve_entity},
    {"stmt_resolve_pcall_proc", test_stmt_resolve_pcall_proc},
    {"scope_resolve_named_types", test_scope_resolve_named_types},
    {"entity_resolve_supertypes_entity", test_entity_resolve_supertypes},
    {NULL}
};

