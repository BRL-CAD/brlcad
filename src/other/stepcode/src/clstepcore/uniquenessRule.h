#ifndef UNIQUENESSRULE_H
#define UNIQUENESSRULE_H

#include "dictionaryInstance.h"

#include "sdai.h"

#include "sc_export.h"

class EntityDescriptor;

class SC_CORE_EXPORT Uniqueness_rule : public Dictionary_instance
{
    public:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        Express_id _label;

        // non-SDAI
        std::string _comment; /** Comment contained in the EXPRESS.
                           * Should be properly formatted to include (* *)
                           * Will be written to EXPRESS as-is (w/out formatting) */
#ifdef _MSC_VER
#pragma warning( pop )
#endif

        const EntityDescriptor *_parent_entity;

        Uniqueness_rule();
        Uniqueness_rule(const Uniqueness_rule &);
        Uniqueness_rule(const char *label, EntityDescriptor *pe = 0)
            : _label(label), _parent_entity(pe) { }
        virtual ~Uniqueness_rule();

        Express_id label_() const
        {
            return _label;
        }
        const EntityDescriptor *parent_() const
        {
            return _parent_entity;
        }
        std::string &comment_()
        {
            return _comment;
        }

        void label_(const Express_id &ei)
        {
            _label = ei;
        }
        void parent_(const EntityDescriptor *pe)
        {
            _parent_entity = pe;
        }
        void comment_(const char *c)
        {
            _comment = c;
        }

};


typedef Uniqueness_rule *Uniqueness_rule_ptr;

class SC_CORE_EXPORT Uniqueness_rule__set
{
    public:
        Uniqueness_rule__set(int = 16);
        ~Uniqueness_rule__set();

        Uniqueness_rule_ptr &operator[](int index);
        void Insert(Uniqueness_rule_ptr, int index);
        void Append(Uniqueness_rule_ptr);
        void Remove(int index);
        int Index(Uniqueness_rule_ptr);

        int Count();
        void Clear();
    private:
        void Check(int index);
    private:
        Uniqueness_rule_ptr *_buf;
        int _bufsize;
        int _count;
};

typedef Uniqueness_rule__set *Uniqueness_rule__set_ptr;
typedef Uniqueness_rule__set_ptr Uniqueness_rule__set_var;

#endif //UNIQUENESSRULE_H
