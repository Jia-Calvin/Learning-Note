#include <iostream>
#include <list>
#include <memory>
#include <stack>
#include <vector>

void printArr(std::vector<int>& arr) {
    for (int i = 0; i < arr.size(); i++) {
        if (i != arr.size() - 1) {
            printf("%2d, ", arr[i]);
        } else {
            printf("%2d", arr[i]);
        }
    }
    printf("\n");
}

void FastSort(std::vector<int>& arr, int start, int end) {
    if (start < 0 || end >= arr.size()) {
        throw "xxxxx";
    }

    if (start >= end) {
        return;
    }
    int pivot = arr[end];

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
            std::swap(arr[i], arr[j]);
        } else {
            std::swap(arr[i], arr[end]);
            break;
        }
    }

    FastSort(arr, start, i - 1);
    FastSort(arr, i + 1, end);
}

int main(int argc, char const* argv[]) {
    std::vector<int> arr{1, -1 - 3, 6, 5,  3, 4, 2, 1, 13, 3123, 122,    2141,
                         1, 24,     5, 67, 8, 9, 5, 0, -1, -123, -12333, -550};
    printArr(arr);
    FastSort(arr, 0, arr.size() - 1);
    printArr(arr);
    return 0;
}
