#ifndef GLOBALRULE_H
#define GLOBALRULE_H

#include "dictionaryInstance.h"
#include "whereRule.h"
#include "entityDescriptorList.h"

#include "sc_export.h"

class SC_CORE_EXPORT Global_rule : public Dictionary_instance {
public:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
    Express_id _name;
    std::string _rule_text; // non-SDAI
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    Entity__set_var _entities; // not implemented
    Where_rule__list_var _where_rules;
    Schema_ptr _parent_schema;

    Global_rule();
    Global_rule( const char * n, Schema_ptr parent_sch, const std::string & rt );
    Global_rule( Global_rule & ); // not fully implemented
    virtual ~Global_rule();

    Express_id name_() const {
        return _name;
    }
    Entity__set_var entities_() const {
        return _entities;
    }
    Where_rule__list_var where_rules_() const {
        return _where_rules;
    }
    Schema_ptr parent_schema_() const {
        return _parent_schema;
    }
    const char * rule_text_() {
        return _rule_text.c_str();
    }

    void name_( Express_id & n ) {
        _name = n;
    }
    void entities_( const Entity__set_var & e ); // not implemented
    void where_rules_( const Where_rule__list_var & wrl ); // not implemented
    void parent_schema_( const Schema_ptr & s ) {
        _parent_schema = s;
    }
    void rule_text_( const char * rt ) {
        _rule_text = rt;
    }

};

typedef Global_rule * Global_rule_ptr;

class SC_CORE_EXPORT Global_rule__set {
public:
    Global_rule__set( int = 16 );
    ~Global_rule__set();

    Global_rule_ptr & operator[]( int index );
    void Insert( Global_rule_ptr, int index );
    void Append( Global_rule_ptr );
    void Remove( int index );
    int Index( Global_rule_ptr );

    int Count();
    void Clear();
private:
    void Check( int index );
private:
    Global_rule_ptr * _buf;
    int _bufsize;
    int _count;
};

typedef Global_rule__set * Global_rule__set_ptr;
typedef Global_rule__set_ptr Global_rule__set_var;


#endif //GLOBALRULE_H
