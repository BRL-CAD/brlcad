
#include <string>

class Select {
  public:
    std::string _underlying_typename;

    Select();
    //Select(const Select&);
    virtual ~Select();

    virtual char * Underlying_typeName const 
    { return _underlying_typename.chars(); }

//    const Named_type_ptr Underlying_type() const;

    // return 0 if unset, otherwise return 1
    virtual int exists() const=0;

    // change the receiver to an unset state.
    virtual void nullify() =0;

    // Set the type of the select's immediate underlying type. This function 
    // shall only be used for the special case of a complex select which
    // contains at least two underlying types which ultimately resolve to
    // the same EXPRESS type.
//    void SetUnderlying_type(const char * typename);
};
