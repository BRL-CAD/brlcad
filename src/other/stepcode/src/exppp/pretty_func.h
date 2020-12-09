#ifndef PRETTY_FUNC_H
#define PRETTY_FUNC_H

#include <express/alg.h>

char * FUNCto_string( Function f );
void FUNC_out( Function fn, int level );
void FUNCout( Function f );
int FUNCto_buffer( Function e, char * buffer, int length );


#endif /* PRETTY_FUNC_H */
