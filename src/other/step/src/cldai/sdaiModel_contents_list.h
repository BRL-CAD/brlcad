#ifndef SDAIMODEL_CONTENTS_LIST_H
#define SDAIMODEL_CONTENTS_LIST_H 1

class SCLP23_NAME(Model_contents__list) {
public:
#ifdef __OSTORE__
    os_Collection<SCLP23_NAME(Model_contents_ptr)> &_rep;
    os_Cursor<SCLP23_NAME(Model_contents_ptr)> _cursor;
    Boolean _first;
#endif

    SCLP23_NAME(Model_contents__list)(int = 16);
    ~SCLP23_NAME(Model_contents__list)();

    SCLP23_NAME(Model_contents_ptr) retrieve(int index);
    int is_empty();

#ifndef __OSTORE__
    SCLP23_NAME(Model_contents_ptr)& operator[](int index);
#endif

    void Insert(SCLP23_NAME(Model_contents_ptr), int index);
    void Append(SCLP23_NAME(Model_contents_ptr));
    void Remove(int index);
#ifndef __OSTORE__
    int Index(SCLP23_NAME(Model_contents_ptr));

    void Clear();
#endif
    int Count();

#ifdef __OSTORE__
// cursor stuff
    SCLP23_NAME(Model_contents_ptr) first();
    SCLP23_NAME(Model_contents_ptr) next();
    SCLP23_NAME(Integer) more();
#endif

private:
    void Check(int index);
private:
    SCLP23_NAME(Model_contents_ptr)* _buf;
    int _bufsize;
    int _count;

  public:

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
};

typedef SCLP23_NAME(Model_contents__list)* 
				SCLP23_NAME(Model_contents__list_ptr);
typedef SCLP23_NAME(Model_contents__list_ptr) 
				SCLP23_NAME(Model_contents__list_var);

#endif
