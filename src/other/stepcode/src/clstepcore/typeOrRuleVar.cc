#include "typeOrRuleVar.h"

#include <iostream>

Type_or_rule::Type_or_rule() {
    std::cerr << "WARNING - Type_or_rule class doesn't seem to be complete - it has no members!" << std::endl;
}

Type_or_rule::Type_or_rule( const Type_or_rule & tor ): Dictionary_instance() {
    ( void ) tor; //TODO once this class has some members, we'll actually have something to copy
}

Type_or_rule::~Type_or_rule() {
}
