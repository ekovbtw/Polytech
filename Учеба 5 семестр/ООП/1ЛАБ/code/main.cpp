#include <iostream>
#include "mystring.h"

void PrintString(const MyString& str) {
    std::cout << "\"" << str.c_str() << "\" ("
              << str.size() << ", " << str.capacity() << ")" << std::endl;
}

int main() {
    MyString empty_str;
    PrintString(empty_str);

    MyString str("Hello world!");
    PrintString(str);

    std::string std_str = "hello";
    MyString from_std(std_str);
    PrintString(from_std);

    MyString copy(from_std);
    PrintString(copy);

    MyString empty_copy(empty_str);
    PrintString(empty_copy);

    MyString part1("hello", 4);
    PrintString(part1);

    MyString part2(std::string("hello"), 4);
    PrintString(part2);

    MyString part3(MyString("hello"), 4);
    PrintString(part3);

    MyString too_long("hello", 10);
    PrintString(too_long);

    MyString exclamations(5, '!');
    PrintString(exclamations);

    MyString assign_str;
    assign_str = "hello";
    PrintString(assign_str);

    MyString assign_ch;
    assign_ch = '!';
    PrintString(assign_ch);

    MyString assign_copy;
    assign_copy = assign_str;
    PrintString(assign_copy);

    MyString& alias = assign_copy;
    assign_copy = alias;
    PrintString(assign_copy);

        // insert
    MyString insert_ch("aaaaa");
    insert_ch.insert(0, 1, '!');
    PrintString(insert_ch);
    insert_ch.insert(3, 2, '@');
    PrintString(insert_ch);

    MyString insert_full("aaaaa");
    insert_full.insert(1, "@@@@@");
    PrintString(insert_full);

    MyString insert_count("aaaaa");
    insert_count.insert(1, "@@@@@", 2);
    PrintString(insert_count);

    MyString insert_part("aaaaa");
    insert_part.insert(1, "abcde", 1, 2);
    PrintString(insert_part);

    // append
    MyString append_ch;
    append_ch.append(3, '!');
    PrintString(append_ch);
    append_ch.append(3, '@');
    PrintString(append_ch);

    MyString append_full;
    append_full.append("Hello ");
    PrintString(append_full);
    append_full.append("world");
    PrintString(append_full);

    MyString append_count;
    append_count.append("Hello world", 6);
    PrintString(append_count);
    append_count.append("world");
    PrintString(append_count);

    MyString append_part;
    append_part.append("Hello world", 0, 6);
    PrintString(append_part);
    append_part.append("Hello world", 6, 5);
    PrintString(append_part);

    // replace
    MyString replace_full("hello amazing world");
    replace_full.replace(6, 7, "wonderful");
    PrintString(replace_full);

    MyString replace_count("hello amazing world");
    replace_count.replace(6, 7, "wonderful", 6);
    PrintString(replace_count);

    MyString replace_part("hello amazing world");
    replace_part.replace(6, 7, "wonderful", 1, 2);
    PrintString(replace_part);

    // substr
    MyString source("hello amazing world");
    MyString sub_tail;
    sub_tail = source.substr(6);
    PrintString(sub_tail);
    MyString sub_part;
    sub_part = source.substr(6, 7);
    PrintString(sub_part);

    // operator+ and operator+=
    MyString left("hel");
    MyString right("lo");
    MyString sum;
    sum = left + right;
    PrintString(left);
    PrintString(right);
    PrintString(sum);

    MyString add_to("hel");
    MyString to_add("lo");
    add_to += to_add;
    PrintString(add_to);
    PrintString(to_add);

    // operator[]
    MyString index_str("hello");
    std::cout << index_str[2] << std::endl;
    index_str[2] = 'L';
    PrintString(index_str);

    // compare and comparison operators, checked against std::string
    MyString a("abcd");
    MyString b("abce");
    std::cout << a.compare(b) << b.compare(a) << std::endl;
    std::cout << (a == b) << (a != b) << (a > b) << (a >= b) << (a < b) << (a <= b) << std::endl;
    std::string std_a = "abcd";
    std::string std_b = "abce";
    std::cout << (std_a == std_b) << (std_a != std_b) << (std_a > std_b)
              << (std_a >= std_b) << (std_a < std_b) << (std_a <= std_b) << std::endl;

    // find
    MyString find_str = "hello amazing world amazing";
    std::cout << find_str.find("amazing") << std::endl;
    std::cout << find_str.find("amazing", 7) << std::endl;

    // operator<<
    MyString out_str("str");
    std::cout << out_str << std::endl;

    // inserting a string into itself
    MyString self("ab");
    self.append(self);
    PrintString(self);
    self.insert(1, self);
    PrintString(self);

    MyString clear_str("Hello world!");
    clear_str.clear();
    PrintString(clear_str);

    MyString shrink_str("Hello world!");
    shrink_str.erase(5, 6);
    PrintString(shrink_str);
    shrink_str.shrink_to_fit();
    PrintString(shrink_str);

        // exceptions
    MyString error_str("hello");

    try {
        error_str.insert(10, "abc");
        std::cout << "NOT THROWN" << std::endl;
    } catch (const std::out_of_range& error) {
        std::cout << "insert: " << error.what() << std::endl;
    }

    try {
        error_str[5] = 'x';
        std::cout << "NOT THROWN" << std::endl;
    } catch (const std::out_of_range& error) {
        std::cout << "operator[]: " << error.what() << std::endl;
    }

    try {
        error_str.erase(0, -1);
        std::cout << "NOT THROWN" << std::endl;
    } catch (const std::invalid_argument& error) {
        std::cout << "erase: " << error.what() << std::endl;
    }

    try {
        MyString sub = error_str.substr(10);
        std::cout << "NOT THROWN" << std::endl;
    } catch (const std::out_of_range& error) {
        std::cout << "substr: " << error.what() << std::endl;
    }

    try {
        const char* null_str = nullptr;
        MyString from_null(null_str);
        std::cout << "NOT THROWN" << std::endl;
    } catch (const std::invalid_argument& error) {
        std::cout << "constructor: " << error.what() << std::endl;
    }

    PrintString(error_str);
    
    return 0;
}