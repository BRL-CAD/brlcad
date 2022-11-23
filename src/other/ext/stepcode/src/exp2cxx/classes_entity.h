#ifndef CLASSES_ENTITY_H
#define CLASSES_ENTITY_H

const char * ENTITYget_classname( Entity );
Entity ENTITYget_superclass( Entity entity );
Entity ENTITYput_superclass( Entity entity );
int ENTITYhas_explicit_attributes( Entity e );
void ENTITYget_first_attribs( Entity entity, Linked_List result );
void ENTITYPrint( Entity entity, FILES * files, Schema schema, bool externMap );
void ENTITYprint_descriptors( Entity entity, FILE * createall, FILE * impl, Schema schema, bool externMap );
void ENTITYprint_classes( Entity entity, FILE * classes );
#endif
