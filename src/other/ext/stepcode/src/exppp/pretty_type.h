#ifndef PRETTY_TYPE_H
#define PRETTY_TYPE_H

#include "../express/type.h"

char * TYPEbody_to_string( Type t );
char * TYPEhead_to_string( Type t );
char * TYPEto_string( Type t );
void TYPE_body_out( Type t, int level );
void TYPE_head_out( Type t, int level );
void TYPE_out( Type t, int level );
void TYPEbody_out( Type t );
int TYPEbody_to_buffer( Type t, char * buffer, int length );
void TYPEhead_out( Type t );
int TYPEhead_to_buffer( Type t, char * buffer, int length );
void TYPEout( Type t );
int TYPEto_buffer( Type t, char * buffer, int length );
void TYPEunique_or_optional_out( TypeBody tb );


#endif /* PRETTY_TYPE_H */
