#include <math.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

int func(long long n, long long m) {
    long long res = pow(m - 1, n - 1);
    long long mid = 0;
    for (int i = 1; i < n - 2; i++) {
        mid += pow(m - 1, i);
    }
    if (n % 2 == 0) {
        res += mid;
    } else {
        res -= mid;
    }
    return res;
}

int main(int argc, char const *argv[]) {
    long long n = 3, m = 3;
    // cin >> n >> m;
    cout << func(n, m) << endl;

    return 0;
}
