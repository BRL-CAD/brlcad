#ifndef INTERFACEDITEM_H
#define INTERFACEDITEM_H

#include "dictionaryInstance.h"

#include "sdai.h"

#include "sc_export.h"

class SC_CORE_EXPORT Interfaced_item : public Dictionary_instance {
protected:
    Interfaced_item();
    Interfaced_item( const Interfaced_item & );
    Interfaced_item( const char * foreign_schema );
    virtual ~Interfaced_item();
public:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
    Express_id _foreign_schema;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    const Express_id foreign_schema_();
    //  private:
    void foreign_schema_( const Express_id & );
};

#endif //INTERFACEDITEM_H
