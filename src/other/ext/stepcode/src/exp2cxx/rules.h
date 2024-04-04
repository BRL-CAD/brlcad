#ifndef RULES_H
#define RULES_H

#include <express/entity.h>

/** print Where_rule's. for types, schema should be null - tename will include schema name + type prefix */
void WHEREprint( const char * tename, Linked_List wheres, FILE * impl, Schema schema, bool needWR );

/** print Uniqueness_rule's. only Entity type has them? */
void UNIQUEprint( Entity entity, FILE * impl, Schema schema );


#endif /* RULES_H */
