#ifndef SDAIMODEL_CONTENTS_LIST_H
#define SDAIMODEL_CONTENTS_LIST_H 1

class SCLP23_NAME(Model_contents__list) {
public:
    SCLP23_NAME(Model_contents__list)(int = 16);
    ~SCLP23_NAME(Model_contents__list)();

    SCLP23_NAME(Model_contents_ptr) retrieve(int index);
    int is_empty();

    SCLP23_NAME(Model_contents_ptr)& operator[](int index);

    void Insert(SCLP23_NAME(Model_contents_ptr), int index);
    void Append(SCLP23_NAME(Model_contents_ptr));
    void Remove(int index);
    int Index(SCLP23_NAME(Model_contents_ptr));

    void Clear();
    int Count();

private:
    void Check(int index);
private:
    SCLP23_NAME(Model_contents_ptr)* _buf;
    int _bufsize;
    int _count;
};

typedef SCLP23_NAME(Model_contents__list)* 
				SCLP23_NAME(Model_contents__list_ptr);
typedef SCLP23_NAME(Model_contents__list_ptr) 
				SCLP23_NAME(Model_contents__list_var);

#endif
