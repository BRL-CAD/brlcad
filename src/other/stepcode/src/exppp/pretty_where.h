#ifndef PRETTY_WHERE_H
#define PRETTY_WHERE_H

#include <express/linklist.h>

void WHERE_out( Linked_List wheres, int level );
void WHEREout( Linked_List w );
int WHEREto_buffer( Linked_List w, char * buffer, int length );
char * WHEREto_string( Linked_List w );


#endif /* PRETTY_WHERE_H */
