#include <algorithm>
#include <iostream>
#include <math.h>
#include <string>
#include <vector>
using namespace std;

int lps(string x, int lps_len) {
    int x_len = x.length();
    if (x_len == 0) {
        return lps_len;
    }

    if (x_len == 1) {
        return lps_len + 1;
    }

    if (x[0] == x[x_len - 1]) {
        return lps(x.substr(1, x_len - 2), lps_len + 2);
    } else {
        return max(lps(x.substr(0, x_len - 1), lps_len), lps(x.substr(1, x_len - 1), lps_len));
    }
}

void print_vec(vector<vector<int>> vec) {
    for (int i = 0; i < vec.size(); i++) {
        for (int j = 0; j < vec[0].size(); j++) {
            cout << vec[i][j] << ", ";
        }
        cout << endl;
    }
    cout << endl;
}

int lps_dyn(string x) {
    int x_len = x.length();

    vector<vector<int>> tab = vector<vector<int>>(x_len, vector<int>(x_len, 0));

    for (int i = 0; i < x_len; i++) {
        tab[i][i] = 1;
    }
    for (int j = 1; j < x_len; j++) {
        for (int i = j - 1; i >= 0; i--) {
            if (x[i] == x[j]) {
                tab[i][j] = tab[i + 1][j - 1] + 2;
            } else {
                tab[i][j] = max(tab[i + 1][j], tab[i][j - 1]);
            }
        }
    }

    return tab[0][x_len - 1];
}

int main() {
    // string a;
    // while (getline(cin, a)) {
    //     cout << a.length();
    //     cout << a.length() - lps_dyn(a) - 1 << endl;
    // }

    string s = "abcda";
    cout << s.length() - lps_dyn(s) - 1 << endl;
    cout << 'Z' - 'A' << endl;
    // string s;
    // cin >> s;
    // int len = s.length();
    // int index = 0;
    // for (int i = 0; i < len; i++, index++) {
    //     int n = s[i] - 'a';
    //     for (int j = 0; j < 4 - i; j++) {
    //         index += n * pow(25, j);
    //     }
    // }
    // cout << index - 1 << endl;
    return 0;
}