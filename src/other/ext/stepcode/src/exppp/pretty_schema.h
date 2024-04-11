#ifndef PRETTY_SCHEMA_H
#define PRETTY_SCHEMA_H

#include <express/schema.h>

char * SCHEMAout( Schema s );
char * SCHEMAref_to_string( Schema s );
void SCHEMAref_out( Schema s );
int SCHEMAref_to_buffer( Schema s, char * buffer, int length );

#endif /* PRETTY_SCHEMA_H */
