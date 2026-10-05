#include "mystring.h"

#include <cstring>
#include <stdexcept>

// ---------- Constructors and destructor ----------

MyString::MyString() : data_(nullptr), size_(0), capacity_(0) {
}

MyString::MyString(const char* str) : MyString() {
    append(str);
}

MyString::MyString(const std::string& str) : MyString() {
    append(str);
}

MyString::MyString(const MyString& other) : MyString() {
    append(other);
}

MyString::MyString(const char* str, int count) : MyString() {
    append(str, count);
}

MyString::MyString(const std::string& str, int count) : MyString() {
    append(str, count);
}

MyString::MyString(const MyString& other, int count) : MyString() {
    append(other, count);
}

MyString::MyString(int count, char ch) : MyString() {
    append(count, ch);
}

MyString::~MyString() {
    delete[] data_;
}

// ---------- Assignment ----------

MyString& MyString::operator=(const char* str) {
    replace(0, size_, str);
    return *this;
}

MyString& MyString::operator=(const std::string& str) {
    replace(0, size_, str);
    return *this;
}

MyString& MyString::operator=(const MyString& other) {
    if (this != &other) {
        replace(0, size_, other);
    }
    return *this;
}

MyString& MyString::operator=(char ch) {
    replace_fill(0, size_, 1, ch);
    return *this;
}

// ---------- Getters ----------

const char* MyString::c_str() const {
    if (data_ == nullptr) {
        return "";
    }
    return data_;
}

int MyString::size() const {
    return size_;
}

int MyString::capacity() const {
    return capacity_;
}

bool MyString::empty() const {
    return size_ == 0;
}

// ---------- Clearing and memory ----------

void MyString::clear() {
    erase(0, size_);
}

void MyString::shrink_to_fit() {
    if (size_ == 0) {
        delete[] data_;
        data_ = nullptr;
        capacity_ = 0;
        return;
    }
    if (capacity_ > size_ + 1) {
        reallocate(size_ + 1);
    }
}

// ---------- Insertion ----------

void MyString::insert(int index, int count, char ch) {
    replace_fill(index, 0, count, ch);
}

void MyString::insert(int index, const char* str) {
    replace(index, 0, str);
}

void MyString::insert(int index, const std::string& str) {
    replace(index, 0, str);
}

void MyString::insert(int index, const MyString& str) {
    replace(index, 0, str);
}

void MyString::insert(int index, const char* str, int count) {
    replace(index, 0, str, count);
}

void MyString::insert(int index, const std::string& str, int count) {
    replace(index, 0, str, count);
}

void MyString::insert(int index, const MyString& str, int count) {
    replace(index, 0, str, count);
}

void MyString::insert(int index, const char* str, int source_index, int count) {
    replace(index, 0, str, source_index, count);
}

void MyString::insert(int index, const std::string& str, int source_index, int count) {
    replace(index, 0, str, source_index, count);
}

void MyString::insert(int index, const MyString& str, int source_index, int count) {
    replace(index, 0, str, source_index, count);
}

// ---------- Appending ----------

void MyString::append(int count, char ch) {
    insert(size_, count, ch);
}

void MyString::append(const char* str) {
    insert(size_, str);
}

void MyString::append(const std::string& str) {
    insert(size_, str);
}

void MyString::append(const MyString& str) {
    insert(size_, str);
}

void MyString::append(const char* str, int count) {
    insert(size_, str, count);
}

void MyString::append(const std::string& str, int count) {
    insert(size_, str, count);
}

void MyString::append(const MyString& str, int count) {
    insert(size_, str, count);
}

void MyString::append(const char* str, int source_index, int count) {
    insert(size_, str, source_index, count);
}

void MyString::append(const std::string& str, int source_index, int count) {
    insert(size_, str, source_index, count);
}

void MyString::append(const MyString& str, int source_index, int count) {
    insert(size_, str, source_index, count);
}

// ---------- Erasing ----------

void MyString::erase(int index, int count) {
    make_gap(index, count, 0);
}

// ---------- Replacement ----------

void MyString::replace(int index, int count, const char* str) {
    replace_impl(index, count, str, checked_length(str));
}

void MyString::replace(int index, int count, const std::string& str) {
    replace_impl(index, count, str.c_str(), static_cast<int>(str.size()));
}

void MyString::replace(int index, int count, const MyString& str) {
    replace_impl(index, count, str.c_str(), str.size());
}

void MyString::replace(int index, int count, const char* str, int source_count) {
    replace_part(index, count, str, checked_length(str), 0, source_count);
}

void MyString::replace(int index, int count, const std::string& str, int source_count) {
    replace_part(index, count, str.c_str(), static_cast<int>(str.size()), 0, source_count);
}

void MyString::replace(int index, int count, const MyString& str, int source_count) {
    replace_part(index, count, str.c_str(), str.size(), 0, source_count);
}

void MyString::replace(int index, int count, const char* str, int source_index, int source_count) {
    replace_part(index, count, str, checked_length(str), source_index, source_count);
}

void MyString::replace(int index, int count, const std::string& str, int source_index,
                       int source_count) {
    replace_part(index, count, str.c_str(), static_cast<int>(str.size()), source_index,
                 source_count);
}

void MyString::replace(int index, int count, const MyString& str, int source_index,
                       int source_count) {
    replace_part(index, count, str.c_str(), str.size(), source_index, source_count);
}

// ---------- Substrings ----------

MyString MyString::substr(int index) const {
    return substr(index, size_ - index);
}

MyString MyString::substr(int index, int count) const {
    MyString result;
    result.replace_part(0, 0, c_str(), size_, index, count);
    return result;
}

// ---------- Concatenation ----------

MyString MyString::operator+(const char* str) const {
    MyString result(*this);
    result += str;
    return result;
}

MyString MyString::operator+(const std::string& str) const {
    MyString result(*this);
    result += str;
    return result;
}

MyString MyString::operator+(const MyString& other) const {
    MyString result(*this);
    result += other;
    return result;
}

MyString& MyString::operator+=(const char* str) {
    append(str);
    return *this;
}

MyString& MyString::operator+=(const std::string& str) {
    append(str);
    return *this;
}

MyString& MyString::operator+=(const MyString& other) {
    append(other);
    return *this;
}

// ---------- Element access ----------

char& MyString::operator[](int index) {
    check_index(index);
    return data_[index];
}

const char& MyString::operator[](int index) const {
    check_index(index);
    return data_[index];
}

// ---------- Comparison ----------

int MyString::compare(const MyString& other) const {
    int common_length = size_ < other.size_ ? size_ : other.size_;
    int result = std::memcmp(c_str(), other.c_str(), common_length);
    if (result == 0) {
        result = size_ - other.size_;
    }
    if (result < 0) {
        return -1;
    }
    if (result > 0) {
        return 1;
    }
    return 0;
}

bool MyString::operator==(const MyString& other) const {
    return compare(other) == 0;
}

bool MyString::operator!=(const MyString& other) const {
    return compare(other) != 0;
}

bool MyString::operator<(const MyString& other) const {
    return compare(other) < 0;
}

bool MyString::operator<=(const MyString& other) const {
    return compare(other) <= 0;
}

bool MyString::operator>(const MyString& other) const {
    return compare(other) > 0;
}

bool MyString::operator>=(const MyString& other) const {
    return compare(other) >= 0;
}

// ---------- Search ----------

int MyString::find(const char* str, int index) const {
    return find_impl(str, checked_length(str), index);
}

int MyString::find(const std::string& str, int index) const {
    return find_impl(str.c_str(), static_cast<int>(str.size()), index);
}

int MyString::find(const MyString& str, int index) const {
    return find_impl(str.c_str(), str.size(), index);
}

// ---------- Private helpers ----------

int MyString::checked_length(const char* str) {
    if (str == nullptr) {
        throw std::invalid_argument("MyString: null pointer");
    }
    return static_cast<int>(std::strlen(str));
}

void MyString::check_index(int index) const {
    if (index < 0 || index >= size_) {
        throw std::out_of_range("MyString: index out of range");
    }
}

void MyString::reallocate(int new_capacity) {
    char* new_data = new char[new_capacity];
    if (data_ != nullptr) {
        std::memcpy(new_data, data_, size_ + 1);
    } else {
        new_data[0] = '\0';
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

void MyString::ensure_capacity(int new_size) {
    int required_capacity = new_size + 1;
    if (required_capacity > capacity_) {
        reallocate(required_capacity);
    }
}

char* MyString::make_gap(int index, int count, int length) {
    if (index < 0 || index > size_) {
        throw std::out_of_range("MyString: index out of range");
    }
    if (count < 0 || length < 0) {
        throw std::invalid_argument("MyString: negative count");
    }
    if (count > size_ - index) {
        count = size_ - index;
    }
    if (count == 0 && length == 0) {
        return data_ + index;
    }

    int new_size = size_ - count + length;
    ensure_capacity(new_size);

    int tail_length = size_ - index - count;
    std::memmove(data_ + index + length, data_ + index + count, tail_length + 1);
    size_ = new_size;
    return data_ + index;
}

void MyString::replace_impl(int index, int count, const char* source, int source_length) {
    // The source may point into our own buffer (e.g. str.append(str)),
    // and make_gap can reallocate or shift it, so copy it first.
    bool source_is_inside = data_ != nullptr && source >= data_ && source < data_ + capacity_;
    if (source_is_inside) {
        MyString source_copy(source, source_length);
        replace_impl(index, count, source_copy.c_str(), source_copy.size());
        return;
    }
    char* gap = make_gap(index, count, source_length);
    std::memcpy(gap, source, source_length);
}

void MyString::replace_part(int index, int count, const char* source, int source_length,
                            int source_index, int source_count) {
    if (source_index < 0 || source_index > source_length) {
        throw std::out_of_range("MyString: source index out of range");
    }
    if (source_count < 0) {
        throw std::invalid_argument("MyString: negative count");
    }
    int available = source_length - source_index;
    if (source_count > available) {
        source_count = available;
    }
    replace_impl(index, count, source + source_index, source_count);
}

void MyString::replace_fill(int index, int count, int fill_count, char ch) {
    char* gap = make_gap(index, count, fill_count);
    std::memset(gap, ch, fill_count);
}

int MyString::find_impl(const char* str, int length, int index) const {
    if (index < 0) {
        throw std::out_of_range("MyString: index out of range");
    }
    const char* text = c_str();
    for (int i = index; i + length <= size_; ++i) {
        if (std::memcmp(text + i, str, length) == 0) {
            return i;
        }
    }
    return cNotFound;
}

// ---------- Output ----------

std::ostream& operator<<(std::ostream& stream, const MyString& str) {
    stream << str.c_str();
    return stream;
}