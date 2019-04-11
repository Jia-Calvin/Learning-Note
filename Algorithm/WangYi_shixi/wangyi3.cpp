#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char const *argv[]) {
    ios::sync_with_stdio(false);
    int n, m;
    cin >> n >> m;
    vector<int> arr(n, 0);
    for (int i = 0; i < n; i++) {
        cin >> arr[i];
    }
    sort(arr.begin(), arr.end());
    int i = 0, j = n - 1;
    int cnt = 0;
    while (i < j) {
        if (arr[i] + arr[j] > m) {
            i++;
            j--;
            cnt += 2;
        } else {
            i += 2;
            cnt += 1;
        }
    }
    cout << cnt + (i == j) << endl;
    system("pause");
    return 0;
}
