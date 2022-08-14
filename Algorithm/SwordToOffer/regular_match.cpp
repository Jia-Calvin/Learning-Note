#include <iostream>
#include <list>
#include <memory>
#include <stack>
#include <vector>

bool match(const std::string& str, const std::string& regular) {
    std::cout << "str: " << str << ", regular: " << regular << std::endl;
    if (str.size() == 0 && regular.size() == 0) {
        return true;
    }
    if (str.size() != 0 && regular.size() == 0) {
        return false;
    }

    if (regular.size() > 1 && regular[1] == '*') {
        if (regular[0] == str[0]) {
            return match(str.substr(1), regular.substr(2)) || match(str.substr(1), regular) ||
                match(str, regular.substr(2));
        } else {
            return match(str, regular.substr(2));
        }
    }

    if (regular[0] == str[0] || regular[0] == '.') {
        return match(str.substr(1), regular.substr(1));
    }

    return false;
}

int main(int argc, char const* argv[]) {
    std::string str = "aaa";
    std::string regular = "aa.a";
    std::cout << (match(str, regular) ? "is matched" : " is not matched") << std::endl;
    return 0;
}
