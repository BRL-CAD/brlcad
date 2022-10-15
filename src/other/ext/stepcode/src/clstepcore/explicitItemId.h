#ifndef EXPLICITITEMID_H
#define EXPLICITITEMID_H

#include "interfacedItem.h"

class SC_CORE_EXPORT Explicit_item_id : public Interfaced_item {
protected:
    Explicit_item_id();
    Explicit_item_id( const Explicit_item_id & );
    Explicit_item_id( const char * foreign_schema, TypeDescriptor * ld,
                      const char * oi, const char * ni )
    : Interfaced_item( foreign_schema ), _local_definition( ld ), _original_id( oi ), _new_id( ni ) {}
    virtual ~Explicit_item_id();
public:
    // definition in the local schema. The TypeDescriptor (or subtype) is not
    // implemented quite right - the name in it is the original (foreign
    // schema) name. The USE or REFERENCED renames are listed in
    // TypeDescriptor's altNames member variable.
    // Warning: This is currently a null ptr for objects other than
    // types and entities - that is - if this is a USEd FUNCTION or PROCEDURE
    // this ptr will be null.
    const TypeDescriptor * _local_definition;

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
    // name in originating schema - only exists if it has been renamed.
    Express_id _original_id;

    Express_id _new_id; // original or renamed name via USE or REFERENCE (non-SDAI)
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    const TypeDescriptor * local_definition_() const {
        return _local_definition;
    }

    const Express_id original_id_() const {
        return _original_id;
    }

    // non-sdai, renamed name
    const Express_id new_id_() const {
        return _new_id;
    }

    // return string "USE" or "REFERENCE"
    virtual const char * EXPRESS_type() = 0;

    //  private:
    void local_definition_( const TypeDescriptor * td ) {
        _local_definition = td;
    }
    void original_id_( const Express_id & ei ) {
        _original_id = ei;
    }

    // non-sdai
    void new_id_( const Express_id & ni ) {
        _new_id = ni;
    }
};

typedef Explicit_item_id * Explicit_item_id_ptr;


class SC_CORE_EXPORT Explicit_item_id__set {
public:
    Explicit_item_id__set( int = 16 );
    ~Explicit_item_id__set();

    Explicit_item_id_ptr & operator[]( int index );
    void Insert( Explicit_item_id_ptr, int index );
    void Append( Explicit_item_id_ptr );
    void Remove( int index );
    int Index( Explicit_item_id_ptr );

    int Count();
    void Clear();
private:
    void Check( int index );
private:
    Explicit_item_id_ptr * _buf;
    int _bufsize;
    int _count;
};

typedef Explicit_item_id__set * Explicit_item_id__set_ptr;
typedef Explicit_item_id__set_ptr Explicit_item_id__set_var;

class SC_CORE_EXPORT Used_item : public Explicit_item_id {
public:
    Used_item() {}
    Used_item( const char * foreign_schema, TypeDescriptor * ld,
               const char * oi, const char * ni )
    : Explicit_item_id( foreign_schema, ld, oi, ni ) {}
    virtual ~Used_item() {}

    const char * EXPRESS_type() {
        return "USE";
    }
};

typedef Used_item * Used_item_ptr;

class SC_CORE_EXPORT Referenced_item : public Explicit_item_id {
public:
    Referenced_item() {}
    Referenced_item( const char * foreign_schema, TypeDescriptor * ld,
                     const char * oi, const char * ni )
    : Explicit_item_id( foreign_schema, ld, oi, ni ) {}
    virtual ~Referenced_item() {}

    const char * EXPRESS_type() {
        return "REFERENCE";
    }
};

typedef Referenced_item * Referenced_item_ptr;


#endif //EXPLICITITEMID_H
