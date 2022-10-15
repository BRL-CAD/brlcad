#ifndef PRETTY_STMT_H
#define PRETTY_STMT_H

#include <express/stmt.h>
#include <express/linklist.h>

char * STMTto_string( Statement s );
void STMT_out( Statement s, int level );
void STMTlist_out( Linked_List stmts, int level );
void STMTout( Statement s );
int STMTto_buffer( Statement s, char * buffer, int length );


#endif /* PRETTY_STMT_H */
