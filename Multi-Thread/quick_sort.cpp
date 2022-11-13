#include <cassert>
#include <future>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>


template <class T>
class QuickSort {
public:
    static std::list<T> quickSort(std::list<T> l) {
        if (l.empty()) {
            return l;
        }
        std::list<T> result;
        // pivot先过去result
        result.splice(result.begin(), l, l.begin());
        const T& pivot = *result.begin();
        auto dividedIt =
            std::partition(l.begin(), l.end(), [pivot](const T& ele) { return ele < pivot; });
        std::list<T> lowerPart;
        lowerPart.splice(lowerPart.end(), l, l.begin(), dividedIt);
        // not async 同步排序
        std::list<T> newLowerPart = quickSort(std::move(lowerPart));
        std::list<T> newHigherPart = quickSort(std::move(l));
        result.splice(result.begin(), newLowerPart);
        result.splice(result.end(), newHigherPart);
        return result;
    }

    static std::list<T> parallelQuickSort(std::list<T> l) {
        if (l.empty()) {
            return l;
        }
        std::list<T> result;
        // pivot先过去result
        result.splice(result.begin(), l, l.begin());
        const T& pivot = *result.begin();
        auto dividedIt =
            std::partition(l.begin(), l.end(), [pivot](const T& ele) { return ele < pivot; });
        std::list<T> lowerPart;
        lowerPart.splice(lowerPart.end(), l, l.begin(), dividedIt);
        //  async 异步并行排序
        std::future<std::list<T>> newLowerPart =
            std::async(&QuickSort<T>::parallelQuickSort, std::move(lowerPart));
        std::list<T> newHigherPart = parallelQuickSort(std::move(l));
        result.splice(result.begin(), newLowerPart.get());
        result.splice(result.end(), newHigherPart);
        return result;
    }
};


int main(int argc, char const* argv[]) {
    std::list<int> l{1, 1, 6, 8, 10, 1, 1, 11, -1, 0, 0, 0, -1, -1, 2, 3, 4, 5, 100};
    std::cout << "Original list\n    ";
    for (int r : l)
        std::cout << r << ", ";
    std::cout << std::endl;

    QuickSort<int> q;

    auto res = q.parallelQuickSort(l);
    std::cout << "ParallelQuickSort list\n    ";
    for (int r : res)
        std::cout << r << ", ";
    std::cout << std::endl;

    auto res1 = q.quickSort(l);
    std::cout << "QuickSort list\n    ";
    for (int r : res1)
        std::cout << r << ", ";
    return 0;
}
