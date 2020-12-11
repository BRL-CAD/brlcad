#ifndef ENTITYDESCRIPTOR_H
#define ENTITYDESCRIPTOR_H

#include "typeDescriptor.h"
#include "attrDescriptor.h"
#include "uniquenessRule.h"
#include "attrDescriptorList.h"
#include "inverseAttributeList.h"

#include "sc_export.h"

typedef  SDAI_Application_instance *(* Creator)();

class Registry;

/** EntityDescriptor
 *
 * An instance of this class will be generated for each entity type
 * found in the schema.  This should probably be derived from the
 * CreatorEntry class (see sdaiApplicaton_instance.h).  Then the binary tree
 * that the current software  builds up containing the entities in the schema
 * will be building the same thing but using the new schema info.
 * nodes (i.e. EntityDesc nodes) for each entity.
 */
class SC_CORE_EXPORT EntityDescriptor  :    public TypeDescriptor
{

    protected:

        SDAI_LOGICAL _abstractEntity;
        SDAI_LOGICAL _extMapping;  /**< does external mapping have to be used to create an instance of us? (see STEP Part 21, sect 11.2.5.1) */

        EntityDescriptorList _subtypes;   // OPTIONAL
        EntityDescriptorList _supertypes; // OPTIONAL
        AttrDescriptorList _explicitAttr; // OPTIONAL
        Inverse_attributeList _inverseAttr;  // OPTIONAL
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string _supertype_stmt;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    public:
        Uniqueness_rule__set_var _uniqueness_rules; // initially a null pointer

        // pointer to a function that will create a new instance of a SDAI_Application_instance
        Creator NewSTEPentity;

        EntityDescriptor();
        EntityDescriptor(const char *name,   // i.e. char *
                         Schema *origSchema,
                         Logical abstractEntity, // i.e. F U or T
                         Logical extMapping,
                         Creator f = 0
                        );

        virtual ~EntityDescriptor();

        void InitIAttrs(Registry &reg, const char *schNm);

        const char *GenerateExpress(std::string &buf) const;

        const char *QualifiedName(std::string &s) const;

        const SDAI_LOGICAL &AbstractEntity() const
        {
            return _abstractEntity;
        }
        const SDAI_LOGICAL &ExtMapping() const
        {
            return _extMapping;
        }
        void AbstractEntity(SDAI_LOGICAL &ae)
        {
            _abstractEntity.put(ae.asInt());
        }
        void ExtMapping(SDAI_LOGICAL &em)
        {
            _extMapping.put(em.asInt());
        }
        void AbstractEntity(Logical ae)
        {
            _abstractEntity.put(ae);
        }
        void ExtMapping(Logical em)
        {
            _extMapping.put(em);
        }
        void ExtMapping(const char *em)
        {
            _extMapping.put(em);
        }

        const EntityDescriptorList &Subtypes() const
        {
            return _subtypes;
        }

        const EntityDescriptorList &Supertypes() const
        {
            return _supertypes;
        }

        const EntityDescriptorList &GetSupertypes()  const
        {
            return _supertypes;
        }

        const AttrDescriptorList &ExplicitAttr() const
        {
            return _explicitAttr;
        }

        const Inverse_attributeList &InverseAttr() const
        {
            return _inverseAttr;
        }

        virtual const EntityDescriptor *IsA(const EntityDescriptor *) const;
        virtual const TypeDescriptor *IsA(const TypeDescriptor *td) const;
        virtual const TypeDescriptor *IsA(const char *n) const
        {
            return TypeDescriptor::IsA(n);
        }
        virtual const TypeDescriptor *CanBe(const TypeDescriptor *o) const
        {
            return o -> IsA(this);
        }

        virtual const TypeDescriptor *CanBe(const char *n) const
        {
            return TypeDescriptor::CanBe(n);
        }

        // The following will be used by schema initialization functions

        void AddSubtype(EntityDescriptor *ed)
        {
            _subtypes.AddNode(ed);
        }
        void AddSupertype_Stmt(const char *s)
        {
            _supertype_stmt = std::string(s);
        }
        const char *Supertype_Stmt()
        {
            return _supertype_stmt.c_str();
        }
        std::string &supertype_stmt_()
        {
            return _supertype_stmt;
        }

        void AddSupertype(EntityDescriptor *ed)
        {
            _supertypes.AddNode(ed);
        }

        void AddExplicitAttr(AttrDescriptor *ad)
        {
            _explicitAttr.AddNode(ad);
        }

        void AddInverseAttr(Inverse_attribute *ia)
        {
            _inverseAttr.AddNode(ia);
        }
        void uniqueness_rules_(Uniqueness_rule__set *urs)
        {
            _uniqueness_rules = urs;
        }
        Uniqueness_rule__set_var &uniqueness_rules_()
        {
            return _uniqueness_rules;
        }

};

#endif //ENTITYDESCRIPTOR_H
