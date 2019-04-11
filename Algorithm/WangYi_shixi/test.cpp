#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

bool isNum(string str, int i) {
    if (str[i] - '0' > 9 || str[i] - '0' < 0) {
        return false;
    }
    return true;
}

bool isNumeric(char* str) {
    string str_new = "";
    stringstream ss;
    ss << str;
    ss >> str_new;

    if (str_new.length() == 0) {
        return false;
    }

    bool is_exist_dot = false;
    bool is_exist_E = false;
    for (int i = 0; i < str_new.length(); i++) {
        if (i == 0 && (str_new[i] == '+' || str_new[i] == '-')) {
            continue;
        }
        if (str_new[i] == '.') {
            if (is_exist_dot || is_exist_E) {
                return false;
            }
            if (i - 1 < 0 || i + 1 > str_new.length() - 1) {
                return false;
            }
            if (!isNum(str_new, i + 1)) {
                return false;
            }
            is_exist_dot = true;
            continue;
        }

        if (str_new[i] == 'E' || str_new[i] == 'e') {
            if (i - 1 < 0 || i + 1 > str_new.length() - 1) {
                return false;
            }
            if (!isNum(str_new, i - 1)) {
                return false;
            }
            if (str_new[i + 1] == '+' || str_new[i + 1] == '-') {
                i++;
            }
            is_exist_E = true;
            continue;
        }

        if (!isNum(str_new, i)) {
            return false;
        }
    }
    return true;
}

int main(int argc, char const* argv[]) {
    char* str = "-.123";
    // char* pattern = ".*";

    cout << isNumeric(str) << endl;

    system("pause");
}
