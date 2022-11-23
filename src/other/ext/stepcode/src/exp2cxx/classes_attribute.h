#ifndef CLASSES_ATTRIBUTE_H
#define CLASSES_ATTRIBUTE_H

char * generate_attribute_name( Variable a, char * out );
char * generate_attribute_func_name( Variable a, char * out );

void DataMemberPrintAttr( Entity entity, Variable a, FILE * file );
void ATTRnames_print( Entity entity, FILE * file );
void ATTRprint_access_methods_get_head( const char * classnm, Variable a, FILE * file, bool returnsConst );
void ATTRprint_access_methods_put_head( const char * entnm, Variable a, FILE * file );
void ATTRsign_access_methods( Variable a, FILE* file );
void ATTRprint_access_methods( const char * entnm, Variable a, FILE * file, Schema schema );

/** return true if attr needs const and non-const getters */
bool attrIsObj( Type t );

#endif
