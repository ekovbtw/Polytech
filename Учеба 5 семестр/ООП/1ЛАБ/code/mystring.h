#ifndef _MYSTRING_H_
#define _MYSTRING_H_

#include <ostream>
#include <string>

class MyString {
public:
    static constexpr int cNotFound = -1;

    // Constructors and destructor
    MyString();
    MyString(const char* str);
    MyString(const std::string& str);
    MyString(const MyString& other);
    MyString(const char* str, int count);
    MyString(const std::string& str, int count);
    MyString(const MyString& other, int count);
    MyString(int count, char ch);
    ~MyString();

    // Assignment
    MyString& operator=(const char* str);
    MyString& operator=(const std::string& str);
    MyString& operator=(const MyString& other);
    MyString& operator=(char ch);

    // Getters
    const char* c_str() const;
    int size() const;
    int capacity() const;
    bool empty() const;

    // Clearing and memory
    void clear();
    void shrink_to_fit();

    // Insertion
    void insert(int index, int count, char ch);
    void insert(int index, const char* str);
    void insert(int index, const std::string& str);
    void insert(int index, const MyString& str);
    void insert(int index, const char* str, int count);
    void insert(int index, const std::string& str, int count);
    void insert(int index, const MyString& str, int count);
    void insert(int index, const char* str, int source_index, int count);
    void insert(int index, const std::string& str, int source_index, int count);
    void insert(int index, const MyString& str, int source_index, int count);

    // Appending
    void append(int count, char ch);
    void append(const char* str);
    void append(const std::string& str);
    void append(const MyString& str);
    void append(const char* str, int count);
    void append(const std::string& str, int count);
    void append(const MyString& str, int count);
    void append(const char* str, int source_index, int count);
    void append(const std::string& str, int source_index, int count);
    void append(const MyString& str, int source_index, int count);

    // Erasing
    void erase(int index, int count);

    // Replacement
    void replace(int index, int count, const char* str);
    void replace(int index, int count, const std::string& str);
    void replace(int index, int count, const MyString& str);
    void replace(int index, int count, const char* str, int source_count);
    void replace(int index, int count, const std::string& str, int source_count);
    void replace(int index, int count, const MyString& str, int source_count);
    void replace(int index, int count, const char* str, int source_index, int source_count);
    void replace(int index, int count, const std::string& str, int source_index, int source_count);
    void replace(int index, int count, const MyString& str, int source_index, int source_count);

    // Substrings
    MyString substr(int index) const;
    MyString substr(int index, int count) const;

    // Concatenation
    MyString operator+(const char* str) const;
    MyString operator+(const std::string& str) const;
    MyString operator+(const MyString& other) const;
    MyString& operator+=(const char* str);
    MyString& operator+=(const std::string& str);
    MyString& operator+=(const MyString& other);

    // Element access
    char& operator[](int index);
    const char& operator[](int index) const;

    // Comparison
    int compare(const MyString& other) const;
    bool operator==(const MyString& other) const;
    bool operator!=(const MyString& other) const;
    bool operator<(const MyString& other) const;
    bool operator<=(const MyString& other) const;
    bool operator>(const MyString& other) const;
    bool operator>=(const MyString& other) const;

    // Search
    int find(const char* str, int index = 0) const;
    int find(const std::string& str, int index = 0) const;
    int find(const MyString& str, int index = 0) const;

private:
    char* data_;     // pointer to null-terminated buffer
    int size_;       // number of characters, excluding '\0'
    int capacity_;   // allocated memory size, including '\0'

    static int checked_length(const char* str);
    void check_index(int index) const;
    void reallocate(int new_capacity);
    void ensure_capacity(int new_size);
    char* make_gap(int index, int count, int length);
    void replace_impl(int index, int count, const char* source, int source_length);
    void replace_part(int index, int count, const char* source, int source_length,
                      int source_index, int source_count);
    void replace_fill(int index, int count, int fill_count, char ch);
    int find_impl(const char* str, int length, int index) const;
};

std::ostream& operator<<(std::ostream& stream, const MyString& str);

#endif // _MYSTRING_H_