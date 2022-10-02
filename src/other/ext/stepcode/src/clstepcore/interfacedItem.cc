#include "interfacedItem.h"

Interfaced_item::Interfaced_item() {
}

Interfaced_item::Interfaced_item( const Interfaced_item & ii ): Dictionary_instance() {
    _foreign_schema = ii._foreign_schema;
}

Interfaced_item::Interfaced_item( const char * foreign_schema )
: _foreign_schema( foreign_schema ) {
}

Interfaced_item::~Interfaced_item() {
}

const Express_id Interfaced_item::foreign_schema_() {
    return _foreign_schema;
}

void Interfaced_item::foreign_schema_( const Express_id & fs ) {
    _foreign_schema = fs;
}

