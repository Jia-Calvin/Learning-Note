#include <iostream>
#include <limits.h>
using namespace std;

int getWaterStorge(int* arr, int n) {
    int maxAllPillar = INT_MIN;
    int maxPillarIndx = INT_MIN;
    int sumWater = 0;
    // 选出所有柱子中最大的那个，有同样大小的柱子也无所谓，
    // 在遍历算出水量的过程会考虑在内，将最大柱子的序号保存下来
    for (int i = 0; i < n; i++) {
        if (arr[i] > maxAllPillar) {
            maxAllPillar = arr[i];
            maxPillarIndx = i;
        }
    }

    int leftStart = 0;
    int leftPos = leftStart + 1;
    /*
        从左边开始往右遍历，等到左边起始点遇到最大柱子的序号时跳出
        关键步骤在于for循环内只计算凹槽内的水量，若是连续的两个柱子，
        例如 1，2 则会直接跳出，因为没有凹槽存在，leftPos是寻找比
        左边开始leftStart大的柱子，就计算一次凹槽水量，重复这个过程。
        同样在右边向最大序号遍历过程也是同样操作
    */
    while (leftStart < maxPillarIndx) {
        while (arr[leftStart] > arr[leftPos]) {
            leftPos++;
        }
        for (int i = leftStart + 1; i < leftPos; i++) {
            sumWater = sumWater + (arr[leftStart] - arr[i]);
        }
        leftStart = leftPos;
        leftPos++;
    }

    int rightStart = n - 1;
    int rightPos = rightStart - 1;
    while (rightStart > maxPillarIndx) {
        while (arr[rightStart] > arr[rightPos]) {
            rightPos--;
        }
        for (int i = rightStart - 1; i > rightPos; i--) {
            sumWater = sumWater + (arr[rightStart] - arr[i]);
        }
        rightStart = rightPos;
        rightPos--;
    }

    return sumWater;
}

int main(int argc, char const* argv[]) {
    int arr[] = {0, 2, 0, 1, 0, 3, 1, 0, 1, 3, 2, 1, 2, 1, 0, 3, 0, 0, 0, 0, 0};
    int n = sizeof(arr) / sizeof(int);
    int water = getWaterStorge(arr, n);
    cout << water << endl;
    return 0;
}
