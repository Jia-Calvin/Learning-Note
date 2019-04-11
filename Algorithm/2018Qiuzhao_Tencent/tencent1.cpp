#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

long long countC(int all, int select) {
    if (select > all / 2) {
        select = all - select;
    }

    long long result = 1;
    for (int i = 0; i < select; i++) {
        result = result * (all - i);
    }

    long long result_in = 1;
    for (int i = 2; i <= select; i++) {
        result_in = result_in * i;
    }

    return result / result_in;
}

int main(int argc, char const *argv[]) {
    int k, a_length, a_num, b_length, b_num;
    cin >> k >> a_length >> a_num >> b_length >> b_num;

    vector<vector<int>> cnt;

    for (int i = 0; i <= a_num; i++) {
        vector<int> comb;
        int remain = k - a_length * i;
        if (remain >= 0) {
            if (remain % b_length == 0) {
                comb.push_back(i);
                comb.push_back(remain / b_length);
                cnt.push_back(comb);
            }
        } else {
            break;
        }
    }

    // for (int i = 0; i < cnt.size(); i++) {
    //     cout << cnt[i][0] << ", " << cnt[i][1] << endl;
    // }

    long long result = 0;
    for (int i = 0; i < cnt.size(); i++) {
        result += countC(a_num, cnt[i][0]) * countC(b_num, cnt[i][1]);
    }

    cout << result % 1000000007 << endl;

    system("pause");
    return 0;
}
