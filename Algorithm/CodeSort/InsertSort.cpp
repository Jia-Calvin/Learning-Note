#include <iostream>
using namespace std;

/*
    插入排序的思想就是在排序P位置上的数字时，希望0到p-1是已经排好序的序列;
    当从左遍历找某个数字恰好比p位置上的数字大的时候(从小到大的序列)，
    对这个数字开始到P-1位置的数字都进行交换.
*/

void InsertSort(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (arr[i] < arr[j]) {
                swap(arr[i], arr[j]);
            }
        }
    }
}

int main(int argc, char const *argv[]) {
    int arr[] = {6,  5, 3,  4, 2, 1, 13, 3123, 122,  2141,   1,
                 24, 5, 67, 8, 9, 5, 0,  -1,   -123, -12333, -550};
    int n = sizeof(arr) / sizeof(int);
    InsertSort(arr, n);

    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    // system("pause");
    return 0;
}
