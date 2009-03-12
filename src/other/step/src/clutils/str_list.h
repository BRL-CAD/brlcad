
typedef char * scl_char_str;
typedef scl_char_str * scl_char_str_ptr;

class scl_char_str__list {
public:
    scl_char_str__list(int = 16);
    ~scl_char_str__list();

    scl_char_str_ptr& operator[](int index);
    void Insert(scl_char_str_ptr, int index);
    void Append(scl_char_str_ptr);
    void Remove(int index);
    int Index(scl_char_str_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    scl_char_str_ptr* _buf;
    int _bufsize;
    int _count;
};
