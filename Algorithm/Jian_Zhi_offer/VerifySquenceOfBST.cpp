#include <algorithm>
#include <iostream>
#include <math.h>
#include <queue>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;
class Solution {
public:
    bool VerifySquenceOfBST(vector<int> sequence) {
        if (sequence.size() == 0) {
            return false;
        }
        if (sequence.size() == 1) {
            return true;
        }

        int root_value = sequence[sequence.size() - 1];
        int first_cut = sequence.size() - 1;
        for (int i = 0; i < sequence.size() - 1; i++) {
            if (sequence[i] > root_value) {
                first_cut = i;
                break;
            }
        }

        for (int i = first_cut + 1; i < sequence.size() - 1; i++) {
            if (sequence[i] < root_value) {
                return false;
            }
        }

        bool left_flag = true;
        if (first_cut > 0) {
            left_flag =
                VerifySquenceOfBST(vector<int>(sequence.begin(), sequence.begin() + first_cut));
        }
        bool right_flag = true;
        if (sequence.size() - 1 > first_cut) {
            right_flag =
                VerifySquenceOfBST(vector<int>(sequence.begin() + first_cut, sequence.end() - 1));
        }
        return right_flag && left_flag;
    }
};

int main(int argc, char const* argv[]) {
    Solution* s = new Solution();

    vector<int> test{4, 7, 6, 8};
    cout << s->VerifySquenceOfBST(test) << endl;

    return 0;
}
