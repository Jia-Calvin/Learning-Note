#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

class Solution {
   public:
    int minNumberInRotateArray(vector<int> rotateArray) {
        int start = 0;
        int end = rotateArray.size() - 1;
        int mid;

        if (rotateArray[start] < rotateArray[end]) {
            return rotateArray[start];
        }

        while (rotateArray[start] >= rotateArray[end]) {
            mid = (start + end) / 2;
            if (rotateArray[mid] >= rotateArray[end]) {
                start = mid;
            } else {
                end = mid;
            }

            if (end - start == 1) {
                break;
            }
        }

        return rotateArray[end];
    }
};

int main(int argc, char const *argv[]) {
    Solution *s = new Solution();
    vector<int> rotate{1, 1, 1, 1, 0, 0, 1};
    cout << s->minNumberInRotateArray(rotate) << endl;

    return 0;
}
