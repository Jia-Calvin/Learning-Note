#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
vector<vector<int>> tab;
string Reverse(string str) {
    int low = 0;
    int high = str.length() - 1;

    while (low < high) {
        char temp = str[low];
        str[low] = str[high];
        str[high] = temp;
        ++low;
        --high;
    }
    return str;
}

void lcs(string str, string str_re) {
    int m = str.length();
    int n = str.length();

    tab = vector<vector<int> >(m + 1, vector<int>(n + 1));

    for (int i = 0; i < m + 1; i++) {
        for (int j = 0; j < n + 1; j++) {
            if (i == 0 || j == 0) {
                tab[i][j] = 0;
            } else if (str[i - 1] == str_re[j - 1]) {
                tab[i][j] = tab[i - 1][j - 1] + 1;
            } else {
                tab[i][j] = max(tab[i - 1][j], tab[i][j - 1]);
            }
        }
    }
}

int point(string str) {
    if (str.length() == 0) {
        return 0;
    }

    string str_re = Reverse(str);

    lcs(str, str_re);
    int max_length = tab[str.length()][str_re.length()];
    int diffij = 99999999999;
    int x = 0, y = 0;
    for (int i = 0; i < tab.size(); i++) {
        for (int j = 0; j < tab[0].size(); j++) {
            if (max_length == tab[i][j]) {
                int true_j = str_re.length() - j + 1;
                if (diffij > abs(true_j - i)) {
                    diffij = abs(true_j - i);
                    y = i - 1;
                    x = true_j - 1;
                }
            }
        }
    }
    int length = y - x + 1;
    return length - max_length + 1 + point(str.replace(x, length, ""));
}

int points(int input1, vector<int> input2) {
    string str = "";
    string tmp;
    stringstream ss;
    for (int i = 0; i < input1; i++) {
        ss << input2[i];
        ss >> tmp;
        ss.clear();
        str += tmp;
    }

    return point(str);
}

int main(int argc, char const* argv[]) {
    vector<int> a;
    a.push_back(1);
    a.push_back(2);
    // a.push_back(3);
    // a.push_back(1);
    // a.push_back(5);

    cout << points(2, a) << endl;
    return 0;
}
