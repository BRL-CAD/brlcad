#ifndef PRETTY_ENTITY_H
#define PRETTY_ENTITY_H

#include "../express/entity.h"
#include "../express/expbasic.h"
#include "../express/express.h"

#include "pp.h"

char * ENTITYto_string( Entity e );
void   ENTITY_out( Entity e, int level );
void   ENTITYattrs_out( Linked_List attrs, int derived, int level );
void   ENTITYinverse_out( Linked_List attrs, int level );
void   ENTITYout( Entity e );
int    ENTITYto_buffer( Entity e, char * buffer, int length );
void   ENTITYunique_out( Linked_List u, int level );


#endif /* PRETTY_ENTITY_H */
