#include <iostream>
// #include <algorithm>
// #include <string>
using namespace std;
void printArr(int *arr, int n);

// 这个函数就是把堆(大小为n)的某个顶上的元素下滤
void PercDown(int *arr, int i, int n) {
    int elementPercDown = arr[i];
    int maxChildIndx;

    // 判断对于i来说是否存在左儿子，有左儿子再进循环
    for (; 2 * i + 1 < n; i = maxChildIndx) {
        // 左儿子为2i+1, 右儿子为2i+2（数组从0开始）
        // 先假定左儿子为大的儿子
        maxChildIndx = 2 * i + 1;
        // 若存在两个儿子，选出最大的那个
        if (maxChildIndx + 1 < n && arr[maxChildIndx] < arr[maxChildIndx + 1]) {
            maxChildIndx++;
        }
        // 比较大的儿子与需要下滤的元素大小，若需要下滤则交换
        if (elementPercDown < arr[maxChildIndx]) {
            arr[i] = arr[maxChildIndx];
        } else {
            // 当前元素已经比左右儿子都要大了，直接break
            break;
        }
    }
    arr[i] = elementPercDown;
}

/*
    堆排序主要分为两个步骤：第一步，建堆，从右到左，从下到上对所有的堆进行堆特性的排列
    第二步：将堆顶的元素（最大或最小）与数组最后一个元素交换，再对交换后的堆顶元素进行下滤（此时的堆大小需要相应减一）
 */
void HeapSort(int *arr, int n) {
    // 从右到左，从下到上一步步建堆
    for (int i = n / 2; i >= 0; i--) {
        PercDown(arr, i, n);
    }

    // 将堆顶的元素与数组最后一个元素交换，再对交换后的堆顶元素进行下滤
    for (int i = n - 1; i > 0; i--) {
        swap(arr[0], arr[i]);
        PercDown(arr, 0, i);
    }
}

int main(int argc, char const *argv[]) {
    int arr[] = {6,  5, 3,  4, 2, 1, 13, 3123, 122,  2141,   1,
                 24, 5, 67, 8, 9, 5, 0,  -1,   -123, -12333, -550};
    int N = 22;
    printArr(arr, N);
    HeapSort(arr, N);
    printArr(arr, N);
    system("pause");
    return 0;
}

void printArr(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}
