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
    bool IsPopOrder(vector<int> pushV, vector<int> popV) {
        if (pushV.size() == 0 || popV.size() == 0) {
            return false;
        }

        stack<int> stck;
        for (int i = 0; i < popV.size(); i++) {
            int pop_value = popV[i];
            int push_stop = -1;
            int j;
            for (j = 0; j < pushV.size(); j++) {
                if (pop_value == pushV[j]) {
                    push_stop = j;
                    break;
                }
            }

            if (push_stop >= 0) {
                for (int j = 0; j <= push_stop; j++) {
                    stck.push(pushV[j]);
                }
                pushV = vector<int>(pushV.begin() + push_stop + 1, pushV.end());
            }

            if (stck.size() > 0) {
                if (stck.top() == pop_value) {
                    stck.pop();
                    continue;
                }
            }

            if (j == pushV.size() && push_stop == -1) {
                return false;
            }
        }

        return true;
    }
};
int main(int argc, char const* argv[]) {
    vector<int> pushV{3};
    vector<int> popV{2};

    Solution* s = new Solution();
    cout << s->IsPopOrder(pushV, popV) << endl;

    return 0;
}
