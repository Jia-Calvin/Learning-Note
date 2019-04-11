#include <iostream>
#include <string>
using namespace std;

int main() {
    int n;
    cin >> n;
    char str[128];
    cin >> str;
    string new_str = "";
    for (int i = 0; i < n; i++) {
        int start = i * 9;
        if (str[start] == '1') {
            for (int j = 1; j < 9; j++) {
                new_str = new_str + str[start + j];
            }
        } else {
            for (int j = 8; j >= 1; j--) {
                new_str = new_str + str[start + j];
            }
        }
        new_str = new_str + " ";
    }
    cout << new_str;
    // system("pause");
    return 0;
}