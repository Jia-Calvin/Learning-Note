#include <algorithm>
#include <iostream>
#include <math.h>
#include <sstream>
#include <vector>

using namespace std;

class RangeModule {
private:
    vector<vector<int>> range_all;

public:
    RangeModule() {}

    void printRange() {
        for (int i = 0; i < range_all.size(); i++) {
            cout << "[" << range_all[i][0] << ", " << range_all[i][1] << ")"
                 << " ";
        }
        cout << endl;
    }

    void addRange(int left, int right) {
        int n = range_all.size();
        vector<vector<int>> tmp;
        for (int i = 0; i <= n; i++) {
            if (i == n || range_all[i][0] > right) {
                tmp.push_back(vector<int>{left, right});
                while (i < n) {
                    tmp.push_back(range_all[i++]);
                }
            } else if (range_all[i][1] < left)
                tmp.push_back(range_all[i]);
            else {
                left = min(left, range_all[i][0]);
                right = max(right, range_all[i][1]);
            }
        }
        swap(range_all, tmp);
    }

    bool queryRange(int left, int right) {
        int n = range_all.size(), l = 0, r = n - 1;
        while (l <= r) {
            int m = l + (r - l) / 2;
            if (range_all[m][0] >= right)
                r = m - 1;
            else if (range_all[m][1] <= left)
                l = m + 1;
            else
                return range_all[m][0] <= left && range_all[m][1] >= right;
        }
        return false;
    }

    void removeRange(int left, int right) {
        int n = range_all.size();
        vector<vector<int>> tmp;
        for (int i = 0; i < n; i++) {
            if (range_all[i][1] <= left || range_all[i][0] >= right)
                tmp.push_back(range_all[i]);
            else {
                if (range_all[i][0] < left)
                    tmp.push_back({range_all[i][0], left});
                if (range_all[i][1] > right)
                    tmp.push_back({right, range_all[i][1]});
            }
        }
        swap(range_all, tmp);
    }
};
class A {
public:
    A() {
        printf("A");
    }
    virtual ~A() {
        printf("~A");
    }
};
class B : public A {
public:
    B() {
        printf("B");
    }
    ~B() {
        printf("~B");
    }
};

int main(int argc, char const* argv[]) {
    A* c = new B[2];
    delete[] c;
    // return 0;

    RangeModule* rangeModule = new RangeModule();
    rangeModule->addRange(10, 20);
    rangeModule->addRange(10, 20);
    rangeModule->printRange();

    rangeModule->addRange(0, 9);
    rangeModule->printRange();

    rangeModule->addRange(31, 21);
    rangeModule->printRange();

    rangeModule->addRange(13, 15);
    rangeModule->printRange();

    rangeModule->printRange();
    return 0;
}
