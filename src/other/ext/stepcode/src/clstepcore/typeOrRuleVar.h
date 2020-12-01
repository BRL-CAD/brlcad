#ifndef TYPEORRULEVAR_H
#define TYPEORRULEVAR_H

#include "dictionaryInstance.h"

#include "sc_export.h"

class SC_CORE_EXPORT Type_or_rule : public Dictionary_instance {
public:
    Type_or_rule();
    Type_or_rule( const Type_or_rule & );
    virtual ~Type_or_rule();
};

typedef Type_or_rule * Type_or_rule_ptr;
typedef Type_or_rule_ptr Type_or_rule_var;


#endif //TYPEORRULEVAR_H
