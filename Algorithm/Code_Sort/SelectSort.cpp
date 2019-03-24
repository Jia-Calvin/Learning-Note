#include <iostream>
using namespace std;

/*
    选择排序思想就是将，每次将最小（最大）数字选出来，和第一位交换
    然后选出次小（次大），和第二位交换。。以此类推
   (每次记录那个选出来的序号保存在indx)
*/
void SelectSort(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        int indx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[indx] > arr[j]) {
                indx = j;
            }
        }
      swap(arr[indx],arr[i]);
    }
}

int main(int argc, char const *argv[]) {
    int arr[] = {6,  5, 3, 4, 2, 1,  13,   3123,   122,  2141, 1,  24, 5,
                 67, 8, 9, 5, 0, -1, -123, -12333, -550, 0,    -1, -3, -11};
    int n = sizeof(arr) / sizeof(int);
    SelectSort(arr, n);
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    return 0;
}
