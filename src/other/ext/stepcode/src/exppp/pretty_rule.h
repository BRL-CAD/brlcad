#ifndef PRETTY_RULE_H
#define PRETTY_RULE_H

#include "../express/alg.h"

char * RULEto_string( Rule r );
void RULE_out( Rule r, int level );
void RULEout( Rule r );
int RULEto_buffer( Rule e, char * buffer, int length );

#endif /* PRETTY_RULE_H */
