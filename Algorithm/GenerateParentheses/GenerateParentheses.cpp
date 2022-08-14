#include <iostream>
#include <string>
#include <vector>
using namespace std;

class Solution {
public:
    vector<string> generateParenthesis(int n) {
        vector<string> result;
        addPairCount(result, "", n, 0);
        return result;
    }
    void addPairCount(vector<string>& v, string str, int n, int m) {
        if (m == 0 && n == 0) {
            v.push_back(str);
            return;
        }
        if (m > 0)
            addPairCount(v, str + ')', n, m - 1);
        if (n > 0)
            addPairCount(v, str + '(', n - 1, m + 1);
    }
};
int main(int argc, char const* argv[]) {
    Solution s;
    vector<string> result = s.generateParenthesis(3);

    for (int i = 0; i < result.size(); i++) {
        cout << result[i] << endl;
    }

    return 0;
}