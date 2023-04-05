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

    static std::list<T> parallelQuickSortList(std::list<T> l) {
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
            std::async(&QuickSort<T>::parallelQuickSortList, std::move(lowerPart));
        std::list<T> newHigherPart = parallelQuickSortList(std::move(l));
        result.splice(result.begin(), newLowerPart.get());
        result.splice(result.end(), newHigherPart);
        return result;
    }

    static std::vector<T> quickSort(std::vector<T> elements) {
        if (elements.empty()) {
            return elements;
        }
        auto selectedIter = elements.begin();
        auto predict = [&](T ele) { return ele <= *selectedIter; };
        auto partitionedIter = std::partition(selectedIter + 1, elements.end(), predict);

        std::vector<T> lowerPart, highPart, res;
        lowerPart.insert(lowerPart.end(), selectedIter + 1, partitionedIter);
        highPart.insert(highPart.end(), partitionedIter, elements.end());
        std::vector<T> lowerSorted = quickSort(lowerPart);
        std::vector<T> highSorted = quickSort(highPart);

        res.insert(res.end(), lowerSorted.begin(), lowerSorted.end());
        res.push_back(*selectedIter);
        res.insert(res.end(), highSorted.begin(), highSorted.end());

        return res;
    }

    static std::vector<T> parallelQuickSortVec(std::vector<T> elements) {
        if (elements.empty()) {
            return elements;
        }
        auto selectedIter = elements.begin();
        auto predict = [&](T ele) { return ele <= *selectedIter; };
        auto partitionedIter = std::partition(selectedIter + 1, elements.end(), predict);
        std::vector<T> lowerPart, highPart, res;
        lowerPart.insert(lowerPart.end(), selectedIter + 1, partitionedIter);
        highPart.insert(highPart.end(), partitionedIter, elements.end());

        std::future<std::vector<T>> future =
            std::async(std::move(QuickSort<T>::parallelQuickSortVec), std::move(highPart));
        std::vector<T> lowerSorted = parallelQuickSortVec(lowerPart);

        res.insert(res.end(), lowerSorted.begin(), lowerSorted.end());
        res.push_back(*selectedIter);
        std::vector<T> highSorted = future.get();
        res.insert(res.end(), highSorted.begin(), highSorted.end());

        return res;
    }
};


int main(int argc, char const* argv[]) {
    std::list<int> l{1, 1, 6, 8, 10, 1, 1, 11, -1, 0, 0, 0, -1, -1, 2, 3, 4, 5, 100};
    std::cout << "Original list\n    ";
    for (int r : l)
        std::cout << r << ", ";
    std::cout << std::endl;

    QuickSort<int> q;

    auto res = q.parallelQuickSortList(l);
    std::cout << "ParallelQuickSort list\n    ";
    for (int r : res)
        std::cout << r << ", ";
    std::cout << std::endl;

    auto res1 = q.quickSort(l);
    std::cout << "QuickSort list\n    ";
    for (int r : res1)
        std::cout << r << ", ";
    std::cout << std::endl;

    std::vector<int> v{1, 1, 6, 8, 10, 1, 1, 11, -1, 0, 0, 0, -1, -1, 2, 3, 4, 5, 100};
    auto res2 = q.parallelQuickSortVec(v);
    std::cout << "ParallelQuickSort vector\n    ";
    for (int r : res2)
        std::cout << r << ", ";
    std::cout << std::endl;

    auto res3 = q.quickSort(v);
    std::cout << "QuickSort vector\n    ";
    for (int r : res3)
        std::cout << r << ", ";
    std::cout << std::endl;

    return 0;
}
