#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
    int N1 = nums1.size();
    int N2 = nums2.size();
    int total = N1 + N2;
    vector<int> num(total);

    int i = 0, j = 0, k = 0;

    while (i + j <= total / 2) {
        if (i < N1 && j < N2) {
            if (nums1[i] <= nums2[j]) {
                num[k++] = nums1[i++];
            } else {
                num[k++] = nums2[j++];
            }
        } else if (i < N1) {
            num[k++] = nums1[i++];
        } else {
            num[k++] = nums2[j++];
        }
    }
    return total % 2 == 0 ? ((double)num[(total - 1) / 2] + (double)num[total / 2]) / 2
                          : num[total / 2];
}
int main(int argc, char const* argv[]) {
    // vector<int> nums1 = {21, 12, 45};
    vector<int> nums1 = {1, 2};
    // vector<int> nums2 = {
    //     21, 132,  455,     1178, 45421, 6946,  9798, 1232,   11,  87,
    //     54, -122, -344331, 12,   7,     -3567, 21,   -16574, 321,
    // };
    vector<int> nums2 = {3, 4};
    sort(nums1.begin(), nums1.end());
    sort(nums2.begin(), nums2.end());

    for (int i = 0; i < nums1.size(); i++) {
        cout << nums1[i] << ", ";
    }
    cout << endl;
    for (int i = 0; i < nums2.size(); i++) {
        cout << nums2[i] << ", ";
    }
    cout << endl;

    cout << "Median of two sorted arrays is: " << findMedianSortedArrays(nums1, nums2) << endl;

    system("pause");
    return 0;
}
