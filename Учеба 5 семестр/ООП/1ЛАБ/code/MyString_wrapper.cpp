#include <pybind11/pybind11.h>
#include <string>
#include "mystring.h"

namespace py = pybind11;

// The autotest builds the module with -std=c++11, where py::overload_cast
// is unavailable, so overloads are selected with static_cast instead.
using InsertChars = void (MyString::*)(int, int, char);
using InsertString = void (MyString::*)(int, const std::string&);
using InsertMyString = void (MyString::*)(int, const MyString&);
using InsertStringCount = void (MyString::*)(int, const std::string&, int);
using InsertMyStringCount = void (MyString::*)(int, const MyString&, int);
using InsertStringPart = void (MyString::*)(int, const std::string&, int, int);
using InsertMyStringPart = void (MyString::*)(int, const MyString&, int, int);

using AppendChars = void (MyString::*)(int, char);
using AppendString = void (MyString::*)(const std::string&);
using AppendMyString = void (MyString::*)(const MyString&);
using AppendStringCount = void (MyString::*)(const std::string&, int);
using AppendMyStringCount = void (MyString::*)(const MyString&, int);
using AppendStringPart = void (MyString::*)(const std::string&, int, int);
using AppendMyStringPart = void (MyString::*)(const MyString&, int, int);

using ReplaceString = void (MyString::*)(int, int, const std::string&);
using ReplaceMyString = void (MyString::*)(int, int, const MyString&);
using ReplaceStringCount = void (MyString::*)(int, int, const std::string&, int);
using ReplaceMyStringCount = void (MyString::*)(int, int, const MyString&, int);
using ReplaceStringPart = void (MyString::*)(int, int, const std::string&, int, int);
using ReplaceMyStringPart = void (MyString::*)(int, int, const MyString&, int, int);

using SubstrFrom = MyString (MyString::*)(int) const;
using SubstrCount = MyString (MyString::*)(int, int) const;

using FindString = int (MyString::*)(const std::string&, int) const;
using FindMyString = int (MyString::*)(const MyString&, int) const;

PYBIND11_MODULE(mystring, m) {
    m.doc() = "Python wrapper for the MyString C++ class";

    py::class_<MyString> my_string(m, "MyString");
    my_string
        // Constructors
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def(py::init<const MyString&>())
        .def(py::init<const std::string&, int>())
        .def(py::init<const MyString&, int>())
        .def(py::init<int, char>())

        // Getters
        .def("c_str", &MyString::c_str)
        .def("size", &MyString::size)
        .def("capacity", &MyString::capacity)
        .def("empty", &MyString::empty)

        // Clearing and memory
        .def("clear", &MyString::clear)
        .def("shrink_to_fit", &MyString::shrink_to_fit)

        // Insertion
        .def("insert", static_cast<InsertChars>(&MyString::insert))
        .def("insert", static_cast<InsertString>(&MyString::insert))
        .def("insert", static_cast<InsertMyString>(&MyString::insert))
        .def("insert", static_cast<InsertStringCount>(&MyString::insert))
        .def("insert", static_cast<InsertMyStringCount>(&MyString::insert))
        .def("insert", static_cast<InsertStringPart>(&MyString::insert))
        .def("insert", static_cast<InsertMyStringPart>(&MyString::insert))

        // Appending
        .def("append", static_cast<AppendChars>(&MyString::append))
        .def("append", static_cast<AppendString>(&MyString::append))
        .def("append", static_cast<AppendMyString>(&MyString::append))
        .def("append", static_cast<AppendStringCount>(&MyString::append))
        .def("append", static_cast<AppendMyStringCount>(&MyString::append))
        .def("append", static_cast<AppendStringPart>(&MyString::append))
        .def("append", static_cast<AppendMyStringPart>(&MyString::append))

        // Erasing
        .def("erase", &MyString::erase)

        // Replacement
        .def("replace", static_cast<ReplaceString>(&MyString::replace))
        .def("replace", static_cast<ReplaceMyString>(&MyString::replace))
        .def("replace", static_cast<ReplaceStringCount>(&MyString::replace))
        .def("replace", static_cast<ReplaceMyStringCount>(&MyString::replace))
        .def("replace", static_cast<ReplaceStringPart>(&MyString::replace))
        .def("replace", static_cast<ReplaceMyStringPart>(&MyString::replace))

        // Substrings
        .def("substr", static_cast<SubstrFrom>(&MyString::substr))
        .def("substr", static_cast<SubstrCount>(&MyString::substr))

        // Search
        .def("find", static_cast<FindString>(&MyString::find),
             py::arg("str"), py::arg("index") = 0)
        .def("find", static_cast<FindMyString>(&MyString::find),
             py::arg("str"), py::arg("index") = 0)

        // Comparison
        .def("compare", &MyString::compare)
        .def("__eq__", [](const MyString& self, const MyString& other) { return self == other; })
        .def("__ne__", [](const MyString& self, const MyString& other) { return self != other; })
        .def("__lt__", [](const MyString& self, const MyString& other) { return self < other; })
        .def("__le__", [](const MyString& self, const MyString& other) { return self <= other; })
        .def("__gt__", [](const MyString& self, const MyString& other) { return self > other; })
        .def("__ge__", [](const MyString& self, const MyString& other) { return self >= other; })

        // Concatenation
        .def("__add__", [](const MyString& self, const MyString& other) { return self + other; })
        .def("__iadd__",
             [](MyString& self, const MyString& other) -> MyString& {
                 self += other;
                 return self;
             },
             py::return_value_policy::reference)

        // Element access
        .def("__getitem__", [](const MyString& self, int index) { return self[index]; })
        .def("__setitem__", [](MyString& self, int index, char ch) { self[index] = ch; })

        // Python special methods
        .def("__len__", &MyString::size)
        .def("__str__", [](const MyString& self) { return std::string(self.c_str()); })
        .def("__repr__", [](const MyString& self) {
            return "MyString(\"" + std::string(self.c_str()) + "\")";
        });

    // Passed by value, so no out-of-class definition is needed in C++11
    my_string.attr("cNotFound") = py::int_(static_cast<int>(MyString::cNotFound));

    py::implicitly_convertible<py::str, MyString>();
}