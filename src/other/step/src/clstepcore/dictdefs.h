#ifndef DICTDEFS_H
#define DICTDEFS_H 1

class Schema;
class AttrDescriptor;
class Inverse_attribute;
class EntityDescriptor;
class TypeDescriptor;
class EnumerationTypeDescriptor;
class AggrTypeDescriptor;
class ArrayTypeDescriptor;
class SetTypeDescriptor;
class ListTypeDescriptor;
class SelectTypeDescriptor;
class StringTypeDescriptor;
class BagTypeDescriptor;
class RealTypeDescriptor;
class EntityDescLinkNode;
class EntityDescriptorList;
class AttrDescLinkNode;
class AttrDescriptorList;
class Inverse_attributeLinkNode;
class Inverse_attributeList;
class TypeDescLinkNode;
class TypeDescriptorList;

// These all match up to the new (CD2) Part 23 spec
// Some classes don't exist in our old dictionary implementation so they are 
// currently commented out.  The underlying class names should be updated 
// when the new dictionary is integrated. DAS

//typedef class Named_type* Named_type_ptr;
//typedef Named_type_ptr Named_type_var;
//typedef class Dictionary_instance* Dictionary_instance_ptr;
//typedef Dictionary_instance_ptr Dictionary_instance_var;
typedef class Schema* Schema_ptr;
typedef Schema_ptr Schema_var;
//typedef class Defined_type* Defined_type_ptr;
//typedef Defined_type_ptr Defined_type_var;
typedef class EntityDescriptor* Entity_ptr;
typedef Entity_ptr Entity_var;
typedef class AttrDescriptor* Attribute_ptr;
typedef Attribute_ptr Attribute_var;
typedef class Derived_attribute* Derived_attribute_ptr;
typedef Derived_attribute_ptr Derived_attribute_var;
typedef class AttrDescriptor* Explicit_attribute_ptr;
typedef Explicit_attribute_ptr Explicit_attribute_var;
typedef Inverse_attribute* Inverse_attribute_ptr;
typedef Inverse_attribute_ptr Inverse_attribute_var;
typedef Inverse_attribute_ptr Inverse_attribute_var;
typedef Inverse_attribute InverseAttrDescriptor;
typedef class EnumTypeDescriptor* Enumeration_type_ptr;
typedef Enumeration_type_ptr Enumeration_type_var;
typedef class SelectTypeDescriptor* Select_type_ptr;
typedef Select_type_ptr Select_type_var;
//typedef class Global_rule* Global_rule_ptr;
//typedef Global_rule_ptr Global_rule_var;
//typedef class Simple_type* Simple_type_ptr;
//typedef Simple_type_ptr Simple_type_var;
//typedef class Number_type* Number_type_ptr;
//typedef Number_type_ptr Number_type_var;
//typedef class IntTypeDescriptor* Integer_type_ptr;
//typedef Integer_type_ptr Integer_type_var;
typedef class RealTypeDescriptor* Real_type_ptr;
typedef Real_type_ptr Real_type_var;
typedef class StringTypeDescriptor* String_type_ptr;
typedef String_type_ptr String_type_var;
//typedef class BinaryTypeDescriptor* Binary_type_ptr;
//typedef Binary_type_ptr Binary_type_var;
//typedef class LogicalTypeDescriptor* Logical_type_ptr;
//typedef Logical_type_ptr Logical_type_var;
//typedef class BooleanTypeDescriptor* Boolean_type_ptr;
//typedef Boolean_type_ptr Boolean_type_var;
typedef class AggrTypeDescriptor* Aggr_type_ptr;
typedef Aggr_type_ptr Aggr_type_var;
typedef class SetTypeDescriptor* Set_type_ptr;
typedef Set_type_ptr Set_type_var;
typedef class BagTypeDescriptor* Bag_type_ptr;
typedef Bag_type_ptr Bag_type_var;
typedef class ListTypeDescriptor* List_type_ptr;
typedef List_type_ptr List_type_var;
typedef class ArrayTypeDescriptor* Array_type_ptr;
typedef Array_type_ptr Array_type_var;

//#include <ExpDict.h>

// the following exist until everything is updated 
typedef  EntityDescriptor  Entity;

#endif
