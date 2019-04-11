#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

int gcd(int m, int n) { return n == 0 ? m : gcd(n, m % n); }

int main(int argc, char const *argv[]) {
    int n;
    cin >> n;
}
