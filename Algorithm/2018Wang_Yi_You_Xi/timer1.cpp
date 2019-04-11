#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char const *argv[]) {
    int n = 1;
    cin >> n;
    char str[9];
    int hour;
    int minu;
    int second;

    for (int i = 0; i < n; i++) {
        cin >> str;
        hour = (str[0] - '0') * 10 + (str[1] - '0');
        minu = (str[3] - '0') * 10 + (str[4] - '0');
        second = (str[6] - '0') * 10 + (str[7] - '0');

        if (hour > 23) {
            str[0] = '0';
        }
        if (minu > 59) {
            str[3] = '0';
        }
        if (second > 59) {
            str[6] = '0';
        }
        cout << str << endl;
    }
    // cout << hour << endl;
    // cout << minu << endl;
    // cout << second << endl;

    system("pause");
    return 0;
}

// 19:90:23 23:59:59
