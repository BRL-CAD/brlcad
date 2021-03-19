#ifndef INTERFACESPEC_H
#define INTERFACESPEC_H

#include "dictionaryInstance.h"
#include "explicitItemId.h"
#include "implicitItemId.h"

#include "sc_export.h"

class SC_CORE_EXPORT Interface_spec : public Dictionary_instance
{
    public:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        Express_id _current_schema_id; // schema containing the USE/REF stmt
#ifdef _MSC_VER
#pragma warning( pop )
#endif
        // set of objects from USE/REFERENCE stmt(s)
        Explicit_item_id__set_var _explicit_items;
        Implicit_item_id__set_var _implicit_items; //not yet initialized for schema

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        // non-SDAI, not useful for SDAI use of Interface_spec (it would need to
        // be a list).
        // schema that defined the USE/REFd objects
        Express_id _foreign_schema_id;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

        // non-SDAI, not useful for SDAI use of Interface_spec (it would need to
        // be a list of ints).
        // schema USEs or REFERENCEs all objects from foreign schema
        int _all_objects;

        Interface_spec();
        Interface_spec(Interface_spec &);   // not tested
        Interface_spec(const char *cur_sch_id, const char *foreign_sch_id,
                       int all_objects = 0);
        virtual ~Interface_spec();

        Express_id current_schema_id_()
        {
            return _current_schema_id;
        }
        Express_id foreign_schema_id_()
        {
            return _foreign_schema_id;
        }

        Explicit_item_id__set_var explicit_items_()
        {
            return _explicit_items;
        }

        // this is not yet initialized for the schema
        Implicit_item_id__set_var implicit_items_()
        {
            return _implicit_items;
        }

        //  private:
        void current_schema_id_(const Express_id &ei)
        {
            _current_schema_id = ei;
        }
        void foreign_schema_id_(const Express_id &fi)
        {
            _foreign_schema_id = fi;
        }

        int all_objects_()
        {
            return _all_objects;
        }
        void all_objects_(int ao)
        {
            _all_objects = ao;
        }
};

typedef Interface_spec *Interface_spec_ptr;

class SC_CORE_EXPORT Interface_spec__set
{
    public:
        Interface_spec__set(int = 16);
        ~Interface_spec__set();

        Interface_spec_ptr &operator[](int index);
        void Insert(Interface_spec_ptr, int index);
        void Append(Interface_spec_ptr);
        void Remove(int index);
        int Index(Interface_spec_ptr);

        int Count();
        void Clear();
    private:
        void Check(int index);
    private:
        Interface_spec_ptr *_buf;
        int _bufsize;
        int _count;
};

typedef Interface_spec__set *Interface_spec__set_ptr;
typedef Interface_spec__set_ptr Interface_spec__set_var;


#endif //INTERFACESPEC_H
