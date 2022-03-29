#include <atomic>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>

using namespace std;
class Solution {
public:
  double findMedianSortedArrays(vector<int> &nums1, vector<int> &nums2) {
    int middleIndex = nums1.size() + nums2.size() / 2;
    bool isOdd = false;
    if ((nums1.size() + nums2.size()) % 2 == 0) {
      isOdd = true;
    }

    int start1 = 0, start2 = 0, cur = 0;
    int selectedEle = 0;
    double towNumberSum = 0.0;
    while (start1 < nums1.size() && start2 < nums2.size()) {
      if (nums1[start1] < nums2[start2]) {
        selectedEle = nums1[start1++];
      } else {
        selectedEle = nums2[start2++];
      }
      if (cur == middleIndex - 1 && isOdd) {
        towNumberSum += selectedEle;
      }
      if (cur == middleIndex) {
        towNumberSum += selectedEle;
      }
      cur++;
    }

    while (start1 < nums1.size()) {
      selectedEle = nums1[start1++];
      if (cur == middleIndex - 1 && isOdd) {
        towNumberSum += selectedEle;
      }
      if (cur == middleIndex) {
        towNumberSum += selectedEle;
      }
      cur++;
    }

    while (start2 < nums2.size()) {
      selectedEle = nums2[start2++];
      if (cur == middleIndex - 1 && isOdd) {
        towNumberSum += selectedEle;
      }
      if (cur == middleIndex) {
        towNumberSum += selectedEle;
      }
      cur++;
    }

    if (!isOdd)
      towNumberSum = 2 * towNumberSum;

    return towNumberSum / 2;
  }
};

int main(int argc, char const *argv[]) {
  std::vector<int> nums1{1, 3};
  std::vector<int> nums2{2};
  Solution s;
  std::cout << s.findMedianSortedArrays(nums1, nums2) << std::endl;
  std::vector<int> nums3{1, 2};
  std::vector<int> nums4{3, 4};
  std::cout << s.findMedianSortedArrays(nums3, nums4) << std::endl;
  return 0;
}
