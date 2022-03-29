#include <iostream>
#include <list>
#include <memory>
#include <stack>
#include <vector>

void printArr(std::vector<int> &arr) {
    for (int i = 0; i < arr.size(); i++) {
        std::cout << arr[i];
    }
    std::cout << std::endl;
}

void pirintOnetoMaxNumCore(std::vector<int> &num, int n, int start) {
    if (start == n) {
        printArr(num);
        return;
    }
    for (int i = 0; i < 10; i++) {
        num[start] = i;
        pirintOnetoMaxNumCore(num, n, start + 1);
    }
}

void pirintOnetoMaxNum(int n) {
    if (n <= 0) return;
    std::vector<int> num(n, 0);
    pirintOnetoMaxNumCore(num, n, 0);
}

int main(int argc, char const *argv[]) {
    pirintOnetoMaxNum(6);
    return 0;
}
