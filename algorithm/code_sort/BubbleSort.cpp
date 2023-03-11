#include <iostream>
using namespace std;

/*
    冒泡排序的思想就是将，最大（最小）的数一个一个比较往上推，推到最顶端，
    紧接着推第二个大，再推第三个大，依次排完全部数字
*/
void BubbleSort(int* arr, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 1; j < n - i; j++) {
            if (arr[j - 1] > arr[j]) {
                swap(arr[j - 1], arr[j]);
            }
        }
    }
}

int main(int argc, char const* argv[]) {
    int arr[] = {6,  5, 3, 4, 2, 1,  13,   3123,   122,  2141, 1,  24, 5,
                 67, 8, 9, 5, 0, -1, -123, -12333, -550, 0,    -1, -3, -11};
    int n = sizeof(arr) / sizeof(int);
    BubbleSort(arr, n);
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    system("pause");
    return 0;
}
