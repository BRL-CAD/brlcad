#include <assert.h>

#include "express/scope.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"

/* non-core */

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */

char *EXPRESSprogram_name;
int yylineno;
int __SCOPE_search_id;

int ENTITY_MARK;

Dictionary EXPRESSbuiltins;


/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)


void setup()
{
    RESET_FAKE(EXPRESS_fail);
}

int test_scope_find()
{
    Schema scope;
    Type sel, ent_base, chk_sel;
    Entity ent, chk_ent;
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

    scope->search_id = -1;
    chk_sel = SCOPE_find(scope, "sel_typ", SCOPE_FIND_ENTITY | SCOPE_FIND_TYPE);

    scope->search_id = -1;
    chk_ent = SCOPE_find(scope, "ent", SCOPE_FIND_ENTITY | SCOPE_FIND_TYPE);

    assert(chk_sel == sel);
    assert(chk_ent == ent);

    return 0;
}

struct test_def tests[] = {
    {"scope_find", test_scope_find},
    {NULL}
};
