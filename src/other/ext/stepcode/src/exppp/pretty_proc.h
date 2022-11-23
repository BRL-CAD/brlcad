#ifndef PRETTY_PROC_H
#define PRETTY_PROC_H

#include <express/alg.h>

char * PROCto_string( Procedure p );
void PROC_out( Procedure p, int level );
void PROCout( Procedure p );
int PROCto_buffer( Procedure e, char * buffer, int length );

#endif /* PRETTY_PROC_H */
