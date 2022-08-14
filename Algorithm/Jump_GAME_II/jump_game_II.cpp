#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
using namespace std;

// 以 2, 1, 3, 1, 3, 1, 4, 1, 1, 4为例子来说明这个算法的有效之处
// 算法的核心思想是，起始点开始起跳，以每次起跳最远能够达到的next_max_pos为截止点，
// 以curr_max_pos + 1为起始点，划分区间这个跳跃区间。
// 例如上面的例子就被划分为

//  0    1  2    3  4  5    6  7    8  9  10
// [2], [1, 3], [1, 3, 1], [4, 1], [1, 4    ]

// 每次起跳落地的低点都可以在这个区间内的任何一点，而下一个区间的长度则由当前这个区间内所有点中能够
// 去到最远的那个点决定， 例如 [2]中的2决定了[1, 3]的3为截止点，[1,3]中的3决定了[1, 3,
// 1]的1为截止点， [1, 3, 1]中的3决定了[4, 1]中的1为截止点，[4, 1]中的4决定了[1, 4    ]的
// 为截止点（此时大于长度总和结束跳跃）。 算法的思想是每次都需要获得下一个区间中能够去到最远的地方。

int jump(vector<int>& nums) {
    int step = 0, curr_max_pos = 0, next_max_pos = 0;
    for (int i = 0; i < nums.size(); i++) {
        // 当前能够到达最远的位置的indx大于等长度-1，则表明能够达到终点，此时跳出
        if (curr_max_pos >= nums.size() - 1)
            break;
        // 进入到当前的区间，遍历区间中每个点获取当前区间中能够到达最远的indx，以此划分下一个区间。
        while (i <= curr_max_pos) {
            next_max_pos = max(nums[i] + i, next_max_pos);
            i++;
        }
        // 记得-1，不然就会有两次+1，否则会少跑一个点
        i--;
        // 准备进入下一个循环，替换当前的最远位置（已利用当前区间选出下一个区间）
        curr_max_pos = next_max_pos;
        step++;
    }
    return step;
}

int main(int argc, char const* argv[]) {
    vector<int> nums = {2, 1, 3, 1, 3, 1, 4, 1, 1, 4};
    cout << jump(nums) << endl;
    system("pause");
    return 0;
}