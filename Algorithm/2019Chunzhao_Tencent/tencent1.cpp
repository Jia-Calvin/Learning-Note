#include <math.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stack>
#include <vector>
using namespace std;

int main() {
    int M, n;
    cin >> M >> n;  // 1--M
    vector<int> a(n);
    for (int i = 0; i < n; ++i) {
        cin >> a[i];
    }
    sort(a.begin(), a.end());
    int number = 0;
    int m = M;
    while (m > 0) {
        int p;
        int min = m;
        for (int i = 0; i < n; ++i) {
            p = a[i];
            int flag;
            if (a[i] <= m) {
                flag = max(p - 1, m - p);
            } else if (a[i] > m) {
                break;
            }
            if (flag < min) min = flag;
        }
        m = min;
        number++;
    }
    cout << number << endl;
    return 0;
}

// 20 4 1 2 5 10