#ifndef WHERERULE_H
#define WHERERULE_H

#include <string>
#include "sdai.h"
#include "dictionaryInstance.h"
#include "typeOrRuleVar.h"

#include "sc_export.h"

class SC_CORE_EXPORT Where_rule : public Dictionary_instance {
public:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
    Express_id _label;

    // non-SDAI
    std::string _comment; // Comment contained in the EXPRESS.
    // Should be properly formatted to include (* *)
    // Will be written to EXPRESS as-is (w/out formatting)
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    Type_or_rule_var _type_or_rule;

    Where_rule();
    Where_rule( const Where_rule & );
    Where_rule( const char * label, Type_or_rule_var tor = 0 )
    : _label( label ), _type_or_rule( tor ) { }
    virtual ~Where_rule();

    Express_id label_() const {
        return _label;
    }
    Type_or_rule_var parent_item() const {
        return _type_or_rule;
    }
    std::string comment_() const {
        return _comment;
    }

    void label_( const Express_id & ei ) {
        _label = ei;
    }
    void parent_item( const Type_or_rule_var & tor ) {
        _type_or_rule = tor;
    }
    void comment_( const char * c ) {
        _comment = c;
    }
};

typedef Where_rule * Where_rule_ptr;

class SC_CORE_EXPORT Where_rule__list {
public:
    Where_rule__list( int = 16 );
    ~Where_rule__list();

    Where_rule_ptr & operator[]( int index );
    void Insert( Where_rule_ptr, int index );
    void Append( Where_rule_ptr );
    void Remove( int index );
    int Index( Where_rule_ptr );

    int Count();
    void Clear();
private:
    void Check( int index );
private:
    Where_rule_ptr * _buf;
    int _bufsize;
    int _count;
};

typedef Where_rule__list * Where_rule__list_ptr;
typedef Where_rule__list_ptr Where_rule__list_var;

#endif //WHERERULE_H
