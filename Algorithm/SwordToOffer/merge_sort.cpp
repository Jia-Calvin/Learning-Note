

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

void MyMerge(std::vector<int>& arr, std::vector<int>& tmp, int start, int middle, int end) {
    int i = start;
    int j = middle;
    int pos = start;
    while (i < middle && j <= end) {
        if (arr[i] < arr[j]) {
            tmp[pos] = arr[i];
            i++;
        } else {
            tmp[pos] = arr[j];
            j++;
        }
        pos++;
    }

    while (i < middle) {
        tmp[pos] = arr[i];
        i++;
        pos++;
    }

    while (j <= end) {
        tmp[pos] = arr[j];
        j++;
        pos++;
    }

    for (int i = start; i <= end; i++) {
        arr[i] = tmp[i];
    }
}

// (1)先分裂左右 （2）后合并
void MergeSortCore(std::vector<int>& arr, std::vector<int>& tmp, int start, int end) {
    if (start >= end) {
        return;
    }
    int middle = (end + start + 1) / 2;
    std::cout << "start=" << start << ", middle=" << middle << ", end=" << end << std::endl;
    MergeSortCore(arr, tmp, start, middle - 1);
    MergeSortCore(arr, tmp, middle, end);

    MyMerge(arr, tmp, start, middle, end);
}

void MergeSort(std::vector<int>& arr, int start, int end) {
    if (start < 0 || end >= arr.size()) {
        throw "xxx";
    }
    std::vector<int> tmp(arr.size(), 0);
    MergeSortCore(arr, tmp, start, end);
}

int main(int argc, char const* argv[]) {
    std::vector<int> arr{1, -1 - 3, 6, 5,  3, 4, 2, 1, 13, 3123, 122,    2141,
                         1, 24,     5, 67, 8, 9, 5, 0, -1, -123, -12333, -550};
    printArr(arr);
    MergeSort(arr, 4, arr.size() - 1);
    printArr(arr);
    return 0;
}
