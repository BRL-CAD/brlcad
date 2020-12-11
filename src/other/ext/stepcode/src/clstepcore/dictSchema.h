#ifndef DICTSCHEMA_H
#define DICTSCHEMA_H

#include <vector>
#include "entityDescriptorList.h"
#include "typeDescriptorList.h"
#include "interfaceSpec.h"
#include "globalRule.h"

#include "dictionaryInstance.h"

typedef  SDAI_Model_contents_ptr(* ModelContentsCreator)();

/**
 * \class Schema (was SchemaDescriptor) - a class of this type is generated and contains schema info.
 */
class SC_CORE_EXPORT Schema : public Dictionary_instance
{

    protected:
        const char   *_name;
        EntityDescriptorList _entList; // list of entities in the schema
        EntityDescriptorList _entsWithInverseAttrs;
        TypeDescriptorList _typeList; // list of types in the schema
        TypeDescriptorList _unnamed_typeList; // list of unnamed types in the schema (for cleanup)
        Interface_spec _interface; // list of USE and REF interfaces  (SDAI)

        // non-SDAI lists
        Interface_spec__set_var _use_interface_list; // list of USE interfaces
        Interface_spec__set_var _ref_interface_list; // list of REFERENCE interfaces
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::vector< std::string > _function_list; // of EXPRESS functions
        std::vector< std::string > _procedure_list; // of EXPRESS procedures
#ifdef _MSC_VER
#pragma warning( pop )
#endif

        Global_rule__set_var _global_rules;

    public:
        ModelContentsCreator CreateNewModelContents;

        Schema(const char *schemaName);
        virtual ~Schema();

        void AssignModelContentsCreator(ModelContentsCreator f = 0)
        {
            CreateNewModelContents = f;
        }

        const char *Name() const
        {
            return _name;
        }
        void Name(const char   *n)
        {
            _name = n;
        }

        Interface_spec &interface_()
        {
            return _interface;
        }

        Interface_spec__set_var use_interface_list_()
        {
            return
                _use_interface_list;
        }

        Interface_spec__set_var ref_interface_list_()
        {
            return _ref_interface_list;
        }

        std::vector< std::string > function_list_()
        {
            return _function_list;
        }

        void AddFunction(const char *f);

        Global_rule__set_var global_rules_()   // const
        {
            return _global_rules;
        }

        void AddGlobal_rule(Global_rule_ptr gr);

        void global_rules_(Global_rule__set_var &grs);    // not implemented

        std::vector< std::string > procedure_list_()
        {
            return _procedure_list;
        }

        void AddProcedure(const char *p);

        EntityDescLinkNode *AddEntity(EntityDescriptor *ed)
        {
            return _entList.AddNode(ed);
        }
        /// must be called in addition to AddEntity()
        EntityDescLinkNode *AddEntityWInverse(EntityDescriptor *ed)
        {
            return _entsWithInverseAttrs.AddNode(ed);
        }

        TypeDescLinkNode *AddType(TypeDescriptor *td)
        {
            return _typeList.AddNode(td);
        }
        TypeDescLinkNode *AddUnnamedType(TypeDescriptor *td)
        {
            return _unnamed_typeList.AddNode(td);
        }

        const EntityDescriptorList *Entities() const
        {
            return & _entList;
        }
        const EntityDescriptorList *EntsWInverse() const
        {
            return & _entsWithInverseAttrs;
        }
        const TypeDescriptorList *Types() const
        {
            return & _typeList;
        }
        const TypeDescriptorList *UnnamedTypes() const
        {
            return & _unnamed_typeList;
        }
        EntityDescriptorList *Entities()
        {
            return & _entList;
        }
        EntityDescriptorList *EntsWInverse()
        {
            return & _entsWithInverseAttrs;
        }
        TypeDescriptorList *Types()
        {
            return & _typeList;
        }
        TypeDescriptorList *UnnamedTypes()
        {
            return & _unnamed_typeList;
        }

        // the whole schema
        void GenerateExpress(ostream &out) const;

        // USE, REFERENCE definitions
        void GenerateUseRefExpress(ostream &out) const;

        // TYPE definitions
        void GenerateTypesExpress(ostream &out) const;

        // Entity definitions
        void GenerateEntitiesExpress(ostream &out) const;
};

typedef Schema SchemaDescriptor;

#endif //DICTSCHEMA_H
