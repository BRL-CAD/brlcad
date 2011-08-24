#ifndef OBJ_RULES_H
#define OBJ_RULES_H

#include "common.h"
#include "obj_token_type.h"

enum YYCONDTYPE {
    INITIAL,
    id_state,
    toggle_id_state,
    id_list_state
};
#define CONDTYPE enum YYCONDTYPE

#include "re2c_utils.h"

__BEGIN_DECLS

typedef scanner_t *yyscan_t;

void obj_parser_lex_destroy(yyscan_t scanner);
int obj_parser_lex(YYSTYPE *tokenValue, yyscan_t scanner);
void *obj_parser_get_extra(yyscan_t scanner);
void obj_parser_set_extra(yyscan_t scanner, void *extra);

__END_DECLS

#endif /* OBJ_RULES_H */
