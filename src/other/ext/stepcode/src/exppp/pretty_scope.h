#ifndef PRETTY_SCOPE_H
#define PRETTY_SCOPE_H

#include <express/linklist.h>
#include <express/scope.h>

#include "pp.h"

void SCOPEadd_inorder( Linked_List list, Scope s );
void SCOPEalgs_out( Scope s, int level );
void SCOPEconsts_out( Scope s, int level );
void SCOPEentities_out( Scope s, int level );
void SCOPEfuncs_out( Scope s, int level );
void SCOPElocals_out( Scope s, int level );
void SCOPEprocs_out( Scope s, int level );
void SCOPErules_out( Scope s, int level );
void SCOPEtypes_out( Scope s, int level );


#endif /* PRETTY_SCOPE_H */
