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
    void reOrderArray(vector<int>& arr) {
        if (arr.size() <= 1) {
            return;
        }
        vector<int> odd;
        vector<int> even;
        for (int i = 0; i < arr.size(); i++) {
            if (arr[i] % 2 == 0) {
                even.push_back(arr[i]);
            } else {
                odd.push_back(arr[i]);
            }
        }
        int k = 0;
        for (int i = 0; i < odd.size(); i++) {
            arr[k++] = odd[i];
        }
        for (int i = 0; i < even.size(); i++) {
            arr[k++] = even[i];
        }
    }
};

int main(int argc, char const* argv[]) {
    Solution* s = new Solution();
    vector<int> array{0, 1, 5, 8, 2, 6, 66, 44, 2, 1, 5, 7, 7};
    s->reOrderArray(array);
    for (int i = 0; i < array.size(); i++) {
        cout << array[i] << ", ";
    }
    cout << endl;
    return 0;
}
