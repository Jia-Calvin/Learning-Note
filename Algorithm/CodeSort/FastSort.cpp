#include <iostream>
using namespace std;

void printArr(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        if (i != n - 1) {
            printf("%2d, ", arr[i]);
        } else {
            printf("%2d", arr[i]);
        }
    }
    printf("\n");
}

/*
    快排选择枢纽元方式：三值中值作为枢纽元，尽可能利用枢纽元分割成近似相等的两份
    最后将中值（枢纽元），放到最后一位去
 */
// int median3(int *arr, int left, int right) {
//     int center = (left + right) / 2;

//     if (arr[left] > arr[center]) {
//         swap(arr[left], arr[center]);
//     }
//     if (arr[left] > arr[right]) {
//         swap(arr[right], arr[left]);
//     }
//     if (arr[center] > arr[right]) {
//         swap(arr[center], arr[right]);
//     }

//     swap(arr[center], arr[right]);
//     return arr[right];
// }
/*
    快速排序依旧利用了分治递归策略，以枢纽元为中心分为两部分，依次递归进入排序，
    它与归并排序不同的地方在于，他是先对最外层的数字进行左右大小分割，再递归进入对小
    数组进行排序的，而归并排序是相反的，归并排序得利用分割到剩下1直接返回，将两个小数组
    不断地拼接，最后拼接成一个最长的有序的数组。
  */

void fSort(int *arr, int start, int end) {
    int pivot = arr[end];
    if (start >= end) {
        return;
    }
    int i = start;
    int j = end - 1;
    while (1) {
        while (arr[i] < pivot) {
            i++;
        }
        while (arr[j] >= pivot) {
            j--;
        }
        if (i < j) {
            swap(arr[i], arr[j]);
        } else {
            swap(arr[i], arr[end]);
            break;
        }
    }

    fSort(arr, start, i - 1);
    fSort(arr, i + 1, end);
}

void fastSort(int *arr, int n) { fSort(arr, 0, n - 1); }

int main(int argc, char const *argv[]) {
    int arr[] = {3, 4,  3, 231, 335, 11, 62,   13, 3123, 122, 2141, 3,   4,
                 5, 67, 8, 9,   5,   0,  -550, 0,  -1,   -3,  -11,  -550};
    int n = sizeof(arr) / sizeof(int);
    printArr(arr, n);
    // fastSort(arr, n);
    printArr(arr, n);

    printArr(arr, n);
    ffSort(arr, 0, n - 1);
    printArr(arr, n);
    return 0;
}
