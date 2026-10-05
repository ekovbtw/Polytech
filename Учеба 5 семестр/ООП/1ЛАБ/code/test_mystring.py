import pytest

from mystring import MyString


def state(s):
    return (str(s), s.size(), s.capacity())


def test_constructors():
    assert state(MyString()) == ("", 0, 0)
    assert state(MyString("Hello world!")) == ("Hello world!", 12, 13)
    assert state(MyString(MyString("hello"))) == ("hello", 5, 6)
    assert state(MyString("hello", 4)) == ("hell", 4, 5)
    assert state(MyString(MyString("hello"), 4)) == ("hell", 4, 5)
    assert state(MyString(5, "!")) == ("!!!!!", 5, 6)


def test_erase_shrink_clear():
    s = MyString("Hello world!")
    s.erase(5, 6)
    assert state(s) == ("Hello!", 6, 13)
    s.shrink_to_fit()
    assert state(s) == ("Hello!", 6, 7)
    s.clear()
    assert state(s) == ("", 0, 7)
    assert s.empty()


def test_insert():
    s = MyString("aaaaa")
    s.insert(0, 1, "!")
    s.insert(3, 2, "@")
    assert state(s) == ("!aa@@aaa", 8, 9)

    s = MyString("aaaaa")
    s.insert(1, "@@@@@")
    assert state(s) == ("a@@@@@aaaa", 10, 11)

    s = MyString("aaaaa")
    s.insert(1, "@@@@@", 2)
    assert state(s) == ("a@@aaaa", 7, 8)

    s = MyString("aaaaa")
    s.insert(1, "abcde", 1, 2)
    assert state(s) == ("abcaaaa", 7, 8)


def test_append():
    s = MyString()
    s.append(3, "!")
    s.append(3, "@")
    assert state(s) == ("!!!@@@", 6, 7)

    s = MyString()
    s.append("Hello world", 0, 6)
    s.append("Hello world", 6, 5)
    assert state(s) == ("Hello world", 11, 12)


def test_replace():
    s = MyString("hello amazing world")
    s.replace(6, 7, "wonderful")
    assert state(s) == ("hello wonderful world", 21, 22)

    s = MyString("hello amazing world")
    s.replace(6, 7, "wonderful", 6)
    assert state(s) == ("hello wonder world", 18, 20)

    s = MyString("hello amazing world")
    s.replace(6, 7, "wonderful", 1, 2)
    assert state(s) == ("hello on world", 14, 20)


def test_substr():
    s = MyString("hello amazing world")
    assert state(s.substr(6)) == ("amazing world", 13, 14)
    assert state(s.substr(6, 7)) == ("amazing", 7, 8)


def test_concatenation():
    left = MyString("hel")
    right = MyString("lo")
    assert state(left + right) == ("hello", 5, 6)
    assert state(left) == ("hel", 3, 4)
    assert str(left + "lo") == "hello"

    s = MyString("hel")
    original = s
    s += "lo"
    assert s is original
    assert state(s) == ("hello", 5, 6)


def test_indexing():
    s = MyString("hello")
    assert s[2] == "l"
    s[2] = "L"
    assert str(s) == "heLlo"
    assert len(s) == 5


def test_compare():
    a = MyString("abcd")
    b = MyString("abce")
    assert a.compare(b) == -1
    assert b.compare(a) == 1
    assert (a == b, a != b, a > b, a >= b, a < b, a <= b) == \
        (False, True, False, False, True, True)
    assert a == "abcd"


def test_find():
    s = MyString("hello amazing world amazing")
    assert s.find("amazing") == 6
    assert s.find("amazing", 7) == 20
    assert s.find("absent") == MyString.cNotFound


def test_exceptions():
    s = MyString("hello")
    with pytest.raises(IndexError):
        s.insert(10, "abc")
    with pytest.raises(IndexError):
        _ = s[5]
    with pytest.raises(ValueError):
        s.erase(0, -1)
    assert str(s) == "hello"