#include <algorithm>
#include <iostream>
#include <math.h>
#include <sstream>
#include <vector>
using namespace std;

int x = 0;
int y = 0;

class Solution {
public:
    bool Find(int target, vector<vector<int>> array) {
        int i = array.size() - 1;
        if (array[i].size() <= 0) {
            return false;
        }
        int j = 0;
        int j_next = 0;
        for (; i >= 0; i--) {
            if (array[i][0] > target) {
                continue;
            } else {
                j = j_next;
                for (; j < array[i].size(); j++) {
                    if (array[i][j] < target) {
                        j_next = j;
                    } else if (array[i][j] == target) {
                        x = i;
                        y = j;
                        return true;
                    }
                }
            }
        }

        return false;
    }
};

int main(int argc, char const* argv[]) {
    vector<vector<int>> tmp;
    tmp.push_back(vector<int>{1, 2, 8, 9});
    tmp.push_back(vector<int>{2, 4, 9, 12});
    tmp.push_back(vector<int>{4, 7, 10, 13});
    tmp.push_back(vector<int>{6, 8, 11, 15});

    for (int i = 0; i < tmp.size(); i++) {
        for (int j = 0; j < tmp[0].size(); j++) {
            cout << tmp[i][j] << ", ";
        }
        cout << endl;
    }
    cout << endl;

    Solution* s = new Solution();
    cout << s->Find(7, tmp) << endl;
    cout << x << ", " << y << endl;
    delete s;
    system("pause");
    return 0;
}