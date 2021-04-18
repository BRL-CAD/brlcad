#include <assert.h>

#include "express/type.h"

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

int tag_count;

/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)


void setup()
{
    RESET_FAKE(EXPRESS_fail)
    TYPEinitialize();
}

int test_type_create_user_defined_tag()
{
    Schema scope;
    Function f;
    Type t, g, chk;
    Symbol *func_id, *tag_id;

    scope = SCHEMAcreate();

    func_id = SYMBOLcreate("func1", 1, "test_5");
    tag_id = SYMBOLcreate("item1", 1, "test_5");

    f = ALGcreate(OBJ_FUNCTION);
    f->symbol = *func_id;
    DICTdefine(scope->symbol_table, func_id->name, f, func_id, OBJ_FUNCTION);

    g = TYPEcreate(generic_);
    t = TYPEcreate_nostab(tag_id, f, OBJ_TAG);

    chk = TYPEcreate_user_defined_tag(g, f, tag_id);

    assert(chk == t);

    return 0;
}

struct test_def tests[] = {
    {"type_create_user_defined_tag", test_type_create_user_defined_tag},
    {NULL}
};
