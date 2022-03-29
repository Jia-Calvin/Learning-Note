#include <iostream>
using namespace std;

void printArr(int *arr, int n) {
  for (int i = 0; i < n; i++) {
    printf("%d ", arr[i]);
  }
  printf("\n");
}

/*
    归并排序：分治的递归算法，主要思想就是假设有两个已经排好序的序列，每个序列有一个序号标签，
    每一次比较将最小（最大）的数值放入活动的数组tmpArr，当某个序列被比较完（都放入了tmpArr）时，
    将另外一个序列依次放入tmpArr，最后将活动的数组复制回原来的数组内。
 */
void merge(int *arr, int *tmpArr, int start, int mid, int end) {
  int i = start;
  int j = mid;
  int tmp_p = start;

  while (i < mid && j <= end) {
    if (arr[i] < arr[j]) {
      tmpArr[tmp_p++] = arr[i++];
    } else {
      tmpArr[tmp_p++] = arr[j++];
    }
  }

  // 把剩下元素的append到tmp里
  while (i < mid)
    tmpArr[tmp_p++] = arr[i++];
  while (j <= end)
    tmpArr[tmp_p++] = arr[j++];

  // 拷贝回原来数组
  for (int i = start; i <= end; i++) {
    arr[i] = tmpArr[i];
  }
}

void mergeSort(int *arr, int *tmpArr, int start, int end) {
  if (start >= end) {
    return;
  }

  int mid = (start + end) / 2;
  mergeSort(arr, tmpArr, start, mid);
  mergeSort(arr, tmpArr, mid + 1, end);

  merge(arr, tmpArr, start, mid + 1, end);
}

void MergeSort(int *arr, int start, int end) {
  int *tmpArr = (int *)malloc(sizeof(int) * (end - start + 1));
  mergeSort(arr, tmpArr, start, end);
  free(tmpArr);
}

int main(int argc, char const *argv[]) {
  int arr[] = {3, 4, 5, 1, 5,  6,    13,     3123, 122, 2141, 1,  24,  5,   67,
               8, 9, 5, 0, -1, -123, -12333, -550, 0,   -1,   -3, -11, 3123};
  int n = sizeof(arr) / sizeof(int);
  printArr(arr, n);
  MergeSort(arr, 0, n - 1);
  printArr(arr, n);
  return 0;
}
